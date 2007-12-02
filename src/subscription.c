/**
 * @file subscription.c common subscription handling
 * 
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <string.h>

#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "favicon.h"
#include "feedlist.h"
#include "metadata.h"
#include "subscription.h"
#include "net/cookies.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"

/* The allowed feed protocol prefixes (see http://25hoursaday.com/draft-obasanjo-feed-URI-scheme-02.html) */
#define FEED_PROTOCOL_PREFIX "feed://"
#define FEED_PROTOCOL_PREFIX2 "feed:"

subscriptionPtr
subscription_new (const gchar *source,
                  const gchar *filter,
                  updateOptionsPtr options)
{
	subscriptionPtr	subscription;
	
	subscription = g_new0 (struct subscription, 1);
	subscription->updateOptions = options;
	
	if (!subscription->updateOptions)
		subscription->updateOptions = g_new0 (struct updateOptions, 1);
		
	subscription->updateState = g_new0 (struct updateState, 1);	
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

/* Checks wether updating a feed makes sense. */
gboolean
subscription_can_be_updated (subscriptionPtr subscription)
{
	if (subscription->updateJob) {
		ui_mainwindow_set_status_bar (_("Subscription \"%s\" is already being updated!"), node_get_title (subscription->node));
		return FALSE;
	}
	
	if (subscription->discontinued) {
		ui_mainwindow_set_status_bar (_("The subscription \"%s\" was discontinued. Liferea won't update it anymore!"), node_get_title (subscription->node));
		return FALSE;
	}

	if (!subscription_get_source (subscription)) {
		g_warning ("Feed source is NULL! This should never happen - cannot update!");
		return FALSE;
	}
	return TRUE;
}	

void
subscription_reset_update_counter (subscriptionPtr subscription, GTimeVal *now) 
{
	if (!subscription)
		return;
		
	subscription->updateState->lastPoll.tv_sec = now->tv_sec;
	db_update_state_save (subscription->node->id, subscription->updateState);
	debug1 (DEBUG_UPDATE, "Resetting last poll counter to %ld.", subscription->updateState->lastPoll.tv_sec);
}

static void
subscription_favicon_downloaded (gpointer user_data)
{
	nodePtr	node = (nodePtr)user_data;

	node_set_icon (node, favicon_load_from_cache (node->id));
	ui_node_update (node->id);
}

void
subscription_update_favicon (subscriptionPtr subscription)
{
	debug1 (DEBUG_UPDATE, "trying to download favicon.ico for \"%s\"", node_get_title (subscription->node));
	ui_mainwindow_set_status_bar (_("Updating favicon for \"%s\""), node_get_title (subscription->node));
	g_get_current_time (&subscription->updateState->lastFaviconPoll);
	db_update_state_save (subscription->node->id, subscription->updateState);
	favicon_download (subscription->node->id,
	                  node_get_base_url (subscription->node),
			  subscription_get_source (subscription),
			  subscription->updateOptions,		// FIXME: correct?
	                  subscription_favicon_downloaded, 
			  (gpointer)subscription->node);
}

typedef struct subscriptionUpdateCtxt {
	subscriptionPtr		subscription;
	subscription_update_cb	callback;
} *subscriptionUpdateCtxtPtr;

static void
subscription_process_update_result (const struct updateResult * const result, gpointer user_data, guint32 flags)
{
	subscriptionUpdateCtxtPtr ctxt = (subscriptionUpdateCtxtPtr)user_data;
	subscriptionPtr	subscription = ctxt->subscription;
	nodePtr		node = subscription->node;
	gboolean	processing = FALSE;
	
	/* 1. preprocessing */

	/* update the subscription URL on permanent redirects */
	if (result->source && g_str_equal (result->source, subscription_get_source (subscription))) {
		subscription_set_source (subscription, result->source);
		ui_mainwindow_set_status_bar (_("The URL of \"%s\" has changed permanently and was updated"), node_get_title(node));
	}

	if (401 == result->httpstatus) { /* unauthorized */
		if (flags & FEED_REQ_AUTH_DIALOG)
			ui_auth_dialog_new (subscription, flags);
	} else if (410 == result->httpstatus) { /* gone */
		subscription->discontinued = TRUE;
		node->available = TRUE;
		ui_mainwindow_set_status_bar (_("\"%s\" is discontinued. Liferea won't updated it anymore!"), node_get_title (node));
	} else if (304 == result->httpstatus) {
		node->available = TRUE;
		ui_mainwindow_set_status_bar (_("\"%s\" has not changed since last update"), node_get_title(node));
	} else {
		processing = TRUE;
	}

	subscription_update_error_status (subscription, result->httpstatus, result->returncode, result->filterErrors);

	if (flags & FEED_REQ_DOWNLOAD_FAVICON)
		subscription_update_favicon (subscription);
	
	/* 2. call subscription/node type specific processing */
	if (processing) {
		if (ctxt->callback) {
			(*ctxt->callback) (node, result, flags);
		} else {
			node_process_update_result (node, result, flags);
		}
	}
	
	/* 3. generic postprocessing */
	subscription->updateJob = NULL;

	update_state_set_lastmodified (subscription->updateState, update_state_get_lastmodified (result->updateState));
	update_state_set_etag (subscription->updateState, update_state_get_etag (result->updateState));
	update_state_set_cookies (subscription->updateState, update_state_get_cookies (result->updateState));
	g_get_current_time (&subscription->updateState->lastPoll);
	db_update_state_save (subscription->node->id, subscription->updateState);
	
	itemview_update_node_info (subscription->node);
	itemview_update ();
	ui_node_update (subscription->node->id);
	feedlist_schedule_save ();
	
	g_free (ctxt);
}

void
subscription_update_with_callback (subscriptionPtr subscription, subscription_update_cb callback, guint flags)
{
	subscriptionUpdateCtxtPtr	ctxt;
	updateRequestPtr		request;
	GTimeVal			now;
	
	if (!subscription)
		return;
	
	debug1 (DEBUG_UPDATE, "Scheduling %s to be updated", node_get_title (subscription->node));
	
	ctxt = g_new0 (struct subscriptionUpdateCtxt, 1);
	ctxt->subscription = subscription;
	ctxt->callback = callback;
	
	/* Retries that might have long timeouts must be 
	   cancelled to immediately execute the user request. */
	if (subscription->updateJob) {
		update_job_cancel_retry (subscription->updateJob);
		subscription->updateJob = NULL;
	}
	 
	if (subscription_can_be_updated (subscription)) {
		ui_mainwindow_set_status_bar (_("Updating \"%s\""), node_get_title (subscription->node));

		g_get_current_time (&now);
		subscription_reset_update_counter (subscription, &now);

		request = update_request_new ();
		request->updateState = update_state_copy (subscription->updateState);
		request->options = update_options_copy (subscription->updateOptions);
		request->source = g_strdup (subscription_get_source (subscription));
		request->allowRetries = (flags & FEED_REQ_ALLOW_RETRIES)? 1 : 0;

		if (subscription_get_filter (subscription))
			request->filtercmd = g_strdup (subscription_get_filter (subscription));
		
		subscription->updateJob = update_execute_request (subscription, request, subscription_process_update_result, ctxt, flags);
	}
}

void
subscription_update (subscriptionPtr subscription, guint flags)
{
	subscription_update_with_callback (subscription, NULL, flags);
}

void
subscription_auto_update (subscriptionPtr subscription)
{
	gint		interval;
	guint		flags = 0;
	GTimeVal	now;
	
	if (!subscription)
		return;

	interval = subscription_get_update_interval (subscription);
	if (-1 == interval)
		interval = conf_get_int_value (DEFAULT_UPDATE_INTERVAL);
			
	if (-2 >= interval || 0 == interval)
		return;		/* don't update this subscription */
		
	if (conf_get_bool_value (ENABLE_FETCH_RETRIES))
		flags |= FEED_REQ_ALLOW_RETRIES;

	g_get_current_time (&now);
	
	if (favicon_update_needed (subscription->node->id, subscription->updateState, &now))
		flags |= FEED_REQ_DOWNLOAD_FAVICON;
			
	if (subscription->updateState->lastPoll.tv_sec + interval*60 <= now.tv_sec)
		subscription_update (subscription, flags);
}

void
subscription_cancel_update (subscriptionPtr subscription)
{
	g_assert (subscription->updateJob);
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

const gchar *
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
subscription_get_filter (subscriptionPtr subscription)
{
	return subscription->filtercmd;
}

void
subscription_set_cookies (subscriptionPtr subscription, const gchar *cookies)
{
	g_free (subscription->updateState->cookies);

	if (cookies)
		subscription->updateState->cookies = g_strdup(cookies);
	else
		subscription->updateState->cookies = NULL;
}

void
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
	
	if ('|' != source[0])
		/* check if we've got matching cookies ... */
		subscription_set_cookies (subscription, cookies_find_matching (source));
	else 
		subscription_set_cookies (subscription, NULL);
	
	if (NULL == subscription_get_orig_source (subscription))
		subscription_set_orig_source (subscription, source);
}

void
subscription_set_filter (subscriptionPtr subscription, const gchar *filter)
{
	g_free (subscription->filtercmd);
	subscription->filtercmd = g_strdup (filter);
	feedlist_schedule_save ();
}

/**
 * Creates a new error description according to the passed
 * HTTP status and the feeds parser errors. If the HTTP
 * status is a success status and no parser errors occurred
 * no error messages is created.
 *
 * @param feed		feed
 * @param httpstatus	HTTP status
 * @param resultcode	the update code's return code (see update.h)
 */
void
subscription_update_error_status (subscriptionPtr subscription,
                                  gint httpstatus,
                                  gint resultcode,
                                  gchar *filterError)
{
	const gchar	*errmsg = NULL;
	gboolean	errorFound = FALSE;

	if (subscription->filterError)
		g_free (subscription->filterError);
	if (subscription->httpError)
		g_free (subscription->httpError);
	if (subscription->updateError)
		g_free (subscription->updateError);
		
	subscription->filterError = g_strdup (filterError);
	subscription->updateError = NULL;
	subscription->httpError = NULL;
	subscription->httpErrorCode = httpstatus;
	
	if (((httpstatus >= 200) && (httpstatus < 400)) && /* HTTP codes starting with 2 and 3 mean no error */
	    (NULL == subscription->filterError))
		return;
	
	if ((200 != httpstatus) || (resultcode != NET_ERR_OK)) {	
		/* first specific codes (guarantees tmp to be set) */
		errmsg = common_http_error_to_str (httpstatus);

		/* second netio errors */
		if (common_netio_error_to_str (resultcode))
			errmsg = common_netio_error_to_str (resultcode);

		errorFound = TRUE;
		subscription->httpError = g_strdup (errmsg);
	}
	
	/* if none of the above error descriptions matched... */
	if (!errorFound)
		subscription->updateError = g_strdup (_("There was a problem while reading this subscription. Please check the URL and console output."));
}

subscriptionPtr
subscription_import (xmlNodePtr xml, gboolean trusted)
{
	subscriptionPtr	subscription;
	xmlChar		*source, *filter, *intervalStr, *tmp;

	subscription = subscription_new (NULL, NULL, NULL);
	
	source = xmlGetProp (xml, BAD_CAST "xmlUrl");
	if (!source)
		source = xmlGetProp (xml, BAD_CAST "xmlurl");	/* e.g. for AmphetaDesk */
		
	if (source) {
		if (!trusted && source[0] == '|') {
			/* FIXME: Display warning dialog asking if the command
			   is safe? */
			tmp = g_strdup_printf ("unsafe command: %s", source);
			xmlFree (source);
			source = tmp;
		}
	
		subscription_set_source (subscription, source);
		xmlFree (source);
	
		if ((filter = xmlGetProp (xml, BAD_CAST "filtercmd"))) {
			if (!trusted) {
				/* FIXME: Display warning dialog asking if the command
				   is safe? */
				tmp = g_strdup_printf ("unsafe command: %s", filter);
				xmlFree (filter);
				filter = tmp;
			}

			subscription_set_filter (subscription, filter);
			xmlFree (filter);
		}
		
		intervalStr = xmlGetProp (xml, BAD_CAST "updateInterval");
		subscription_set_update_interval (subscription, common_parse_long (intervalStr, -1));
		xmlFree (intervalStr);
	
		/* no proxy flag */
		tmp = xmlGetProp (xml, BAD_CAST "dontUseProxy");
		if (tmp && !xmlStrcmp (tmp, BAD_CAST "true"))
			subscription->updateOptions->dontUseProxy = TRUE;
		xmlFree (tmp);
		
		/* authentication options */
		subscription->updateOptions->username = xmlGetProp (xml, BAD_CAST "username");
		subscription->updateOptions->password = xmlGetProp (xml, BAD_CAST "password");
	}
	
	return subscription;
}

void
subscription_export (subscriptionPtr subscription, xmlNodePtr xml, gboolean trusted)
{
	gchar *interval = g_strdup_printf ("%d", subscription_get_update_interval (subscription));

	xmlNewProp (xml, BAD_CAST "xmlUrl", BAD_CAST subscription_get_source (subscription));
	
	if (subscription_get_filter (subscription))
		xmlNewProp (xml, BAD_CAST"filtercmd", BAD_CAST subscription_get_filter (subscription));
		
	if(trusted) {
		xmlNewProp (xml, BAD_CAST"updateInterval", BAD_CAST interval);

		if (subscription->updateOptions->dontUseProxy)
			xmlNewProp (xml, BAD_CAST"dontUseProxy", BAD_CAST"true");
			
		if (subscription->updateOptions->username)
			xmlNewProp (xml, BAD_CAST"username", subscription->updateOptions->username);
		if (subscription->updateOptions->password)
			xmlNewProp (xml, BAD_CAST"password", subscription->updateOptions->password);
	}
	
	g_free (interval);
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
	
	update_job_cancel_by_owner (subscription);
	update_options_free (subscription->updateOptions);
	update_state_free (subscription->updateState);
	metadata_list_free (subscription->metadata);
	
	g_free (subscription);
}
