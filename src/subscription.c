/**
 * @file subscription.c  common subscription handling
 *
 * Copyright (C) 2003-2025 Lars Windolf <lars.windolf@gmx.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "subscription.h"

#include <math.h>
#include <string.h>

#include "auth.h"
#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "itemlist.h"
#include "metadata.h"
#include "net.h"
#include "subscription_icon.h"
#include "xml.h"
#include "ui/auth_dialog.h"
#include "ui/feed_list_view.h"
#include "ui/liferea_shell.h"

/* The allowed feed protocol prefixes (see http://25hoursaday.com/draft-obasanjo-feed-URI-scheme-02.html) */
#define FEED_PROTOCOL_PREFIX "feed://"
#define FEED_PROTOCOL_PREFIX2 "feed:"

#define ONE_MONTH_MICROSECONDS (gint64)(60*60*24*31) * (gint64)G_USEC_PER_SEC

subscriptionPtr
subscription_new (const gchar *source,
                  const gchar *filter,
                  updateOptionsPtr options)
{
	subscriptionPtr	subscription;

	subscription = g_new0 (struct subscription, 1);
	subscription->type = feed_get_subscription_type ();
	subscription->updateOptions = options;
	subscription->cacheLimit = CACHE_DEFAULT;
	subscription->valid = TRUE;

	if (!subscription->updateOptions)
		subscription->updateOptions = g_new0 (struct updateOptions, 1);

	subscription->updateState = update_state_new ();
	subscription->updateInterval = -1;
	subscription->defaultInterval = -1;

	if (source) {
		gboolean feedPrefix = FALSE;
		gchar *uri = g_strdup (source);
		g_strstrip (uri);	/* strip confusing whitespaces */

		/* strip feed protocol prefix variant 1 */
		if (uri == strstr (uri, FEED_PROTOCOL_PREFIX)) {
			gchar *tmp = uri;
			uri = g_strdup (uri + strlen (FEED_PROTOCOL_PREFIX));
			g_free (tmp);
			feedPrefix = TRUE;
		}

		/* strip feed protocol prefix variant 2 */
		if (uri == strstr (uri, FEED_PROTOCOL_PREFIX2)) {
			gchar *tmp = uri;
			uri = g_strdup (uri + strlen (FEED_PROTOCOL_PREFIX2));
			g_free (tmp);
			feedPrefix = TRUE;
		}

		/* ensure protocol prefix (but only for feed:[//] URIs to avoid
		   breaking local file and command line subscriptions) */
		if (feedPrefix && !strstr (uri, "://")) {
			gchar *tmp = uri;
			uri = g_strdup_printf ("http://%s", uri);
			g_free (tmp);
		}

		subscription_set_source (subscription, uri);
		g_free (uri);
	}

	if (filter)
		subscription_set_filter (subscription, filter);

	return subscription;
}

/* Checks whether updating a feed makes sense. */
static gboolean
subscription_can_be_updated (subscriptionPtr subscription)
{
	if (subscription->updateJob) {
		liferea_shell_set_status_bar (_("Subscription \"%s\" is already being updated!"), node_get_title (subscription->node));
		return FALSE;
	}

	if (subscription->discontinued) {
		liferea_shell_set_status_bar (_("The subscription \"%s\" was discontinued. Liferea won't update it anymore!"), node_get_title (subscription->node));
		return FALSE;
	}

	// can be the case for newsbins
	if (!subscription_get_source (subscription))
		return FALSE;

	return TRUE;
}

void
subscription_reset_update_counter (subscriptionPtr subscription, guint64 *now)
{
	if (!subscription)
		return;

	subscription->updateState->lastPoll = *now;
	debug (DEBUG_UPDATE, "Resetting last poll counter of %s to %lld.", subscription->source, subscription->updateState->lastPoll);
}

/**
 * Updates the error status of the given subscription
 *
 * @param subscription	the subscription
 * @param httpstatus	the new HTTP status code
 * @param filterError	filter error string (or NULL)
 */
static void
subscription_update_error_status (subscriptionPtr subscription,
                                  gint httpstatus,
                                  gchar *filterError)
{
	if (subscription->filterError)
		g_free (subscription->filterError);
	if (subscription->httpError)
		g_free (subscription->httpError);
	if (subscription->updateError)
		g_free (subscription->updateError);

	subscription->filterError = g_strdup (filterError);
	subscription->updateError = NULL;	// FIXME: this might not be very useful!
	subscription->httpError = NULL;
	subscription->httpErrorCode = httpstatus;

	/* Note: the httpstatus we get here is a libsoup status code
	   which is either a HTTP status code or a libsoup status.
	   https://developer.gnome.org/libsoup/unstable/libsoup-2.4-soup-status.html

	   Therefore we know if it is between 200 an 399 all is fine.

	   Otherwise we build a message according to the libsoup doc
	 */

	if (!((httpstatus >= 200) && (httpstatus < 400)))
		subscription->httpError = g_strdup (network_strerror (httpstatus));
}

static void
subscription_process_update_result (const UpdateResult * const result, gpointer user_data, guint32 flags)
{
	subscriptionPtr subscription = (subscriptionPtr)user_data;
	Node		*node = subscription->node;
	gboolean	processing = FALSE;
	guint		count, maxcount;
	gchar		*statusbar;

	/* 1. preprocessing */
	statusbar = g_strdup ("");

	g_assert (subscription->updateJob);
	/* update the subscription URL on permanent redirects */
	if ((301 == result->httpstatus || 308 == result->httpstatus) && result->source && !g_str_equal (result->source, subscription->updateJob->request->source)) {
		debug (DEBUG_UPDATE, "The URL of \"%s\" has changed permanently and was updated to \"%s\"", node_get_title(node), result->source);
		subscription_set_source (subscription, result->source);
		statusbar = g_strdup_printf (_("The URL of \"%s\" has changed permanently and was updated"), node_get_title(node));
	}

	/* consider everything that prevents processing the data we got */
	if (304 == result->httpstatus) {
		node->available = TRUE;
		statusbar = g_strdup_printf (_("\"%s\" has not changed since last update"), node_get_title(node));
	} else if (result->httpstatus >= 400 || !result->data) {
		/* Default */
		subscription->error = FETCH_ERROR_NET;
		node->available = FALSE;

		/* Special handling */
		if (401 == result->httpstatus) { /* unauthorized */
			subscription->error = FETCH_ERROR_AUTH;
			auth_dialog_new (subscription, flags);
		}
		if (410 == result->httpstatus) { /* gone */
			subscription_set_discontinued (subscription, TRUE);
			statusbar = g_strdup_printf (_("\"%s\" is discontinued. Liferea won't updated it anymore!"), node_get_title (node));
		}
	} else if (result->filterErrors) {
		node->available = FALSE;
		subscription->error = FETCH_ERROR_NET;
	} else {
		processing = TRUE;
	}

	/* Clear status bar if we are last update in progress */
	update_job_queue_get_count (&count, &maxcount);
	if (1 >= count)
		liferea_shell_set_status_bar (statusbar);
	else
		liferea_shell_set_status_bar (_("Updating (%d / %d) ..."), maxcount - count, maxcount);
	g_free (statusbar);

	subscription_update_error_status (subscription, result->httpstatus, result->filterErrors);

	subscription->updateJob = NULL;

	/* 2. call subscription type specific processing */
	if (processing)
		SUBSCRIPTION_TYPE (subscription)->process_update_result (subscription, result, flags);

	/* 3. call favicon updating only after subscription processing
	      to ensure we have valid baseUrl for feed nodes...

	      check creation date and update favicon if older than one month */
	if (g_get_real_time() > (subscription->updateState->lastFaviconPoll + ONE_MONTH_MICROSECONDS))
		subscription_icon_update (subscription);

	/* 4. generic postprocessing */
	update_state_set_lastmodified (subscription->updateState, update_state_get_lastmodified (result->updateState));
	update_state_set_cookies (subscription->updateState, update_state_get_cookies (result->updateState));
	update_state_set_etag (subscription->updateState, update_state_get_etag (result->updateState));
	subscription->updateState->lastPoll = g_get_real_time();

	db_subscription_update (subscription);
	db_node_update (subscription->node);

	if (processing && subscription->node->newCount > 0) {
		// FIXME: use new-items signal in itemview class
		feedlist_new_items (node->newCount);
		feedlist_node_was_updated (node);
	}
}

void
subscription_update (subscriptionPtr subscription, guint flags)
{
	UpdateRequest	*request;
	guint64		now;
	guint		count, maxcount;

	if (!subscription)
		return;

	if (subscription->updateJob)
		return;

	debug (DEBUG_UPDATE, "Scheduling %s to be updated (flags=%u)", node_get_title (subscription->node), flags);

	if (subscription_can_be_updated (subscription)) {
		now = g_get_real_time();
		subscription_reset_update_counter (subscription, &now);

		request = update_request_new (
			subscription_get_source (subscription),
			subscription->updateState,
			subscription->updateOptions
		);
		update_request_allow_commands (request, TRUE);

		if (subscription_get_filter (subscription))
			request->filtercmd = g_strdup (subscription_get_filter (subscription));

		if (SUBSCRIPTION_TYPE (subscription)->prepare_update_request (subscription, request))
			subscription->updateJob = update_job_new (subscription, request, subscription_process_update_result, subscription, flags);
		else
			g_object_unref (request);

		update_job_queue_get_count (&count, &maxcount);
		if (count > 1)
			liferea_shell_set_status_bar (_("Updating (%d / %d) ..."), maxcount - count, maxcount);
		else
			liferea_shell_set_status_bar (_("Updating '%s'..."), node_get_title (subscription->node));
	}
}

void
subscription_auto_update (subscriptionPtr subscription)
{
	gint		interval;
	guint		flags = 0;
	guint64	now;

	if (!subscription)
		return;

	interval = subscription_get_update_interval (subscription);
	if (-1 == interval)
		conf_get_int_value (DEFAULT_UPDATE_INTERVAL, &interval);

	if (-2 >= interval || 0 == interval)
		return;		/* don't update this subscription */

	now = g_get_real_time();

	if (subscription->updateState->lastPoll + (guint64)interval * (guint64)(60 * G_USEC_PER_SEC) <= now)
		subscription_update (subscription, flags);
}

void
subscription_cancel_update (subscriptionPtr subscription)
{
	if (!subscription->updateJob)
		return;

	update_job_cancel_by_owner (subscription);
	subscription->updateJob = NULL;
}

gint
subscription_get_update_interval (subscriptionPtr subscription)
{
	return subscription->updateInterval;
}

void
subscription_set_update_interval (subscriptionPtr subscription, gint interval)
{
	if (0 == interval) {
		interval = -1;	/* This is evil, I know, but when this method
				   is called to set the update interval to 0
				   we mean "never updating". The updating logic
				   expects -1 for "never updating" and 0 for
				   updating according to the global update
				   interval... */
	}
	subscription->updateInterval = interval;
	feedlist_schedule_save ();
}

guint
subscription_get_default_update_interval (subscriptionPtr subscription)
{
	return subscription->defaultInterval;
}

void
subscription_set_default_update_interval (subscriptionPtr subscription, guint interval)
{
	subscription->defaultInterval = interval;
}

void
subscription_set_discontinued (subscriptionPtr subscription, gboolean newState)
{
	subscription->discontinued = newState;
}

static const gchar *
subscription_get_orig_source (subscriptionPtr subscription)
{
	return subscription->origSource;
}

const gchar *
subscription_get_source (subscriptionPtr subscription)
{
	return subscription->source;
}

const gchar *
subscription_get_homepage (subscriptionPtr subscription)
{
	return metadata_list_get (subscription->metadata, "homepage");
}

const gchar *
subscription_get_filter (subscriptionPtr subscription)
{
	return subscription->filtercmd;
}

static void
subscription_set_orig_source (subscriptionPtr subscription, const gchar *source)
{
	g_free (subscription->origSource);
	subscription->origSource = g_strchomp (g_strdup (source));
	feedlist_schedule_save ();
}

void
subscription_set_source (subscriptionPtr subscription, const gchar *source)
{
	g_free (subscription->source);
	subscription->source = g_strchomp (g_strdup (source));
	feedlist_schedule_save ();

	update_state_set_cookies (subscription->updateState, NULL);

	if (NULL == subscription_get_orig_source (subscription))
		subscription_set_orig_source (subscription, source);
}

void
subscription_set_homepage (subscriptionPtr subscription, const gchar *newHtmlUrl)
{
	gchar 	*htmlUrl = NULL;

	if (newHtmlUrl) {
		if (strstr (newHtmlUrl, "://")) {
			/* absolute URI can be used directly */
			htmlUrl = g_strchomp (g_strdup (newHtmlUrl));
		} else {
			/* relative URI part needs to be expanded */
			gchar *tmp, *source;

			source = g_strdup (subscription_get_source (subscription));
			tmp = strrchr (source, '/');
			if (tmp)
				*(tmp+1) = '\0';

			htmlUrl = (gchar *)common_build_url (newHtmlUrl, source);
			g_free (source);
		}

		metadata_list_set (&subscription->metadata, "homepage", htmlUrl);
		g_free (htmlUrl);
	}
}

void
subscription_set_filter (subscriptionPtr subscription, const gchar *filter)
{
	g_free (subscription->filtercmd);
	subscription->filtercmd = g_strdup (filter);
	feedlist_schedule_save ();
}

void
subscription_set_auth_info (subscriptionPtr subscription,
                            const gchar *username,
                            const gchar *password)
{
	g_assert (NULL != subscription->updateOptions);

	g_free (subscription->updateOptions->username);
	g_free (subscription->updateOptions->password);

	subscription->updateOptions->username = g_strdup (username);
	subscription->updateOptions->password = g_strdup (password);

	liferea_auth_info_store (subscription);
}

subscriptionPtr
subscription_import (xmlNodePtr xml, gboolean trusted)
{
	subscriptionPtr	subscription;
	xmlChar		*source, *homepage, *filter, *intervalStr, *tmp;

	subscription = subscription_new (NULL, NULL, NULL);

	source = xmlGetProp (xml, BAD_CAST "xmlUrl");
	if (!source)
		source = xmlGetProp (xml, BAD_CAST "xmlurl");	/* e.g. for AmphetaDesk */

	xmlChar	*typeStr = xmlGetProp (xml, BAD_CAST"type");
	subscription->fhp = feed_type_str_to_fhp ((gchar *)typeStr);
	xmlFree (typeStr);

	if (source) {
		if (!trusted && source[0] == '|') {
			/* FIXME: Display warning dialog asking if the command
			   is safe? */
			tmp = (xmlChar *)g_strdup_printf ("unsafe command: %s", source);
			xmlFree (source);
			source = tmp;
		}

		subscription_set_source (subscription, (gchar *)source);
		xmlFree (source);

		homepage = xmlGetProp (xml, BAD_CAST "htmlUrl");
		if (homepage && xmlStrcmp (homepage, BAD_CAST ""))
			subscription_set_homepage (subscription, (gchar *)homepage);
		xmlFree (homepage);

		if ((filter = xmlGetProp (xml, BAD_CAST "filtercmd"))) {
			if (!trusted) {
				/* FIXME: Display warning dialog asking if the command
				   is safe? */
				tmp = (xmlChar *)g_strdup_printf ("unsafe command: %s", filter);
				xmlFree (filter);
				filter = tmp;
			}

			subscription_set_filter (subscription, (gchar *)filter);
			xmlFree (filter);
		}

		intervalStr = xmlGetProp (xml, BAD_CAST "updateInterval");
		subscription_set_update_interval (subscription, common_parse_long ((gchar *)intervalStr, -1));
		xmlFree (intervalStr);

		/* no proxy flag */
		tmp = xmlGetProp (xml, BAD_CAST "dontUseProxy");
		if (tmp && !xmlStrcmp (tmp, BAD_CAST "true"))
			subscription->updateOptions->dontUseProxy = TRUE;
		xmlFree (tmp);

		/* authentication options */
		subscription->updateOptions->username = (gchar *)xmlGetProp (xml, BAD_CAST "username");
		subscription->updateOptions->password = (gchar *)xmlGetProp (xml, BAD_CAST "password");
	}

	/* Set the feed cache limit */
	tmp = xmlGetProp (xml, BAD_CAST "cacheLimit");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"unlimited"))
		subscription->cacheLimit = CACHE_UNLIMITED;
	else
		subscription->cacheLimit = common_parse_long ((gchar *)tmp, CACHE_DEFAULT);
	xmlFree (tmp);

	/* enclosure auto download flag */
	tmp = xmlGetProp (xml, BAD_CAST"encAutoDownload");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		subscription->encAutoDownload = TRUE;
	xmlFree (tmp);

	/* comment feed handling flag */
	tmp = xmlGetProp (xml, BAD_CAST"ignoreComments");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		subscription->ignoreComments = TRUE;
	xmlFree (tmp);

	tmp = xmlGetProp (xml, BAD_CAST"markAsRead");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		subscription->markAsRead = TRUE;
	xmlFree (tmp);

	tmp = xmlGetProp (xml, BAD_CAST"html5Extract");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		subscription->html5Extract = TRUE;
	xmlFree (tmp);

	tmp = xmlGetProp (xml, BAD_CAST"loadItemLink");
	if (tmp && !xmlStrcmp ((xmlChar *)tmp, BAD_CAST"true"))
		subscription->loadItemLink = TRUE;
	xmlFree (tmp);

	tmp = xmlGetProp (xml, BAD_CAST"alwaysShowInReducedMode");
	if (tmp && !xmlStrcmp (tmp, BAD_CAST"true"))
		subscription->alwaysShowInReduced = TRUE;
	xmlFree (tmp);

	return subscription;
}

void
subscription_export (subscriptionPtr subscription, xmlNodePtr xml, gboolean trusted)
{
	gchar *interval = g_strdup_printf ("%d", subscription_get_update_interval (subscription));

	xmlNewProp (xml, BAD_CAST "xmlUrl", BAD_CAST subscription_get_source (subscription));

	if (subscription_get_homepage (subscription))
		xmlNewProp (xml, BAD_CAST"htmlUrl", BAD_CAST subscription_get_homepage (subscription));
	else
		xmlNewProp (xml, BAD_CAST"htmlUrl", BAD_CAST "");

	if (subscription_get_filter (subscription))
		xmlNewProp (xml, BAD_CAST"filtercmd", BAD_CAST subscription_get_filter (subscription));

	xmlNewProp (xml, BAD_CAST"updateInterval", BAD_CAST interval);

	if (subscription->updateOptions->dontUseProxy)
		xmlNewProp (xml, BAD_CAST"dontUseProxy", BAD_CAST"true");

	if (trusted) {
		g_autofree gchar *cacheLimit = NULL;

		if (subscription->cacheLimit >= 0)
			cacheLimit = g_strdup_printf ("%d", subscription->cacheLimit);
		if (subscription->cacheLimit == CACHE_UNLIMITED)
			cacheLimit = g_strdup ("unlimited");
		if (cacheLimit)
			xmlNewProp (xml, BAD_CAST"cacheLimit", BAD_CAST cacheLimit);

		if (subscription->encAutoDownload)
			xmlNewProp (xml, BAD_CAST"encAutoDownload", BAD_CAST"true");

		if (subscription->ignoreComments)
			xmlNewProp (xml, BAD_CAST"ignoreComments", BAD_CAST"true");

		if (subscription->markAsRead)
			xmlNewProp (xml, BAD_CAST"markAsRead", BAD_CAST"true");

		if (subscription->html5Extract)
			xmlNewProp (xml, BAD_CAST"html5Extract", BAD_CAST"true");

		if (subscription->loadItemLink)
			xmlNewProp (xml, BAD_CAST"loadItemLink", BAD_CAST"true");

		if (subscription->alwaysShowInReduced)
			xmlNewProp (xml, BAD_CAST"alwaysShowInReducedMode", BAD_CAST"true");

		debug (DEBUG_CACHE, "adding subscription: source=%s interval=%d cacheLimit=%s",
			subscription_get_source (subscription),
			subscription_get_update_interval (subscription),
			(cacheLimit != NULL ? cacheLimit : ""));
	}

	g_free (interval);
}

// content scraping

static void
subscription_html5_enrich_item_cb (const UpdateResult * const result, gpointer userdata, updateFlags flags) {
	itemPtr item;
	gchar	*article;

	if (!result->data || result->httpstatus >= 400)
		return;

	item = item_load (GPOINTER_TO_UINT (userdata));
	if (!item)
		return;

	article = xhtml_extract_from_string (result->data, result->source);
	if (article) {
		// Enable AMP images by replacing <amg-img> by <img>
		gchar **tmp_split = g_strsplit(article, "<amp-img", 0);
		gchar *tmp = g_strjoinv("<img", tmp_split);
		g_strfreev (tmp_split);
		g_free (article);
		article = tmp;

		metadata_list_set (&(item->metadata), "richContent", article);
		db_item_update (item);
		g_free (article);
	}
	item_unload (item);
}

static void
subscription_text_enrich_item_cb (const UpdateResult * const result, gpointer user_data, updateFlags flags)
{
        if (!result->data || result->size <= 0) {
                debug (DEBUG_UPDATE, "text fetch failed: %s", result->source);
                return;
        }

        itemPtr item = item_load (GPOINTER_TO_UINT (user_data));
        if (!item)
                return;

        g_autoptr(GString) description = g_string_new ("<pre>");
        g_string_append_len (description, result->data, result->size);
        g_string_append (description, "</pre>");
	metadata_list_set (&(item->metadata), "richContent", description->str);
        db_item_update (item);

        item_unload (item);
}


/**
 * Checks content of an items source and tries to crawl content
 */
void
subscription_enrich_item (subscriptionPtr subscription, itemPtr item)
{
	UpdateRequest *request;

	if (!item->source) {
		debug (DEBUG_PARSING, "Cannot enrich item %s because it has no source!", item->title);
		return;
	}

	// Don't enrich twice
	if (NULL != metadata_list_get (item->metadata, "richContent")) {
		debug (DEBUG_PARSING, "Skipping already enriched item %s", item->title);
		return;
	}

	// Fetch item->link document and try to parse it as XHTML
	debug (DEBUG_PARSING, "Fetching content for %ld %s : %s", item->id, item->title, item->source);
	request = update_request_new (
		item->source,
		NULL,	// updateState
		subscription->updateOptions	// Pass options of parent feed (e.g. password, proxy...)
	);

	if (strstr ("gopher://", item->source) != NULL)
		update_job_new (subscription, request, subscription_text_enrich_item_cb, GUINT_TO_POINTER (item->id), UPDATE_REQUEST_NO_FEED);
	else
		update_job_new (subscription, request, subscription_html5_enrich_item_cb, GUINT_TO_POINTER (item->id), UPDATE_REQUEST_NO_FEED);
}

guint
subscription_get_max_item_count (subscriptionPtr subscription)
{
	gint	default_max_items;

	switch (subscription->cacheLimit) {
		case CACHE_DEFAULT: // -1
			conf_get_int_value (DEFAULT_MAX_ITEMS, &default_max_items);
			return default_max_items;
			break;
		case CACHE_DISABLE: // 0
		case CACHE_UNLIMITED:
			return G_MAXUINT;
			break;
		default: // any other positive value
			return subscription->cacheLimit;
			break;
	}
}

void
subscription_free (subscriptionPtr subscription)
{
	if (!subscription)
		return;

	g_free (subscription->updateError);
	g_free (subscription->filterError);
	g_free (subscription->httpError);
	g_free (subscription->source);
	g_free (subscription->origSource);
	g_free (subscription->filtercmd);

	if (subscription->parseErrors)
		g_string_free (subscription->parseErrors, TRUE);

	update_job_cancel_by_owner (subscription);
	update_options_free (subscription->updateOptions);
	update_state_free (subscription->updateState);
	metadata_list_free (subscription->metadata);

	g_free (subscription);
}
