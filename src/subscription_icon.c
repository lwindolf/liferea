/**
 * @file subscription.c  Downloading suitable subscription icons
 *
 * Copyright (C) 2003-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "subscription_icon.h"

#include <string.h>

#include "common.h"
#include "debug.h"
#include "favicon.h"
#include "feedlist.h"
#include "html.h"
#include "metadata.h"
#include "update.h"
#include "ui/feed_list_view.h"

#define ICON_DOWNLOAD_MAX_URLS	10

typedef struct iconDownloadCtxt {
	gchar		        *id;		/**< icon cache id (=node id) */
	GSList			*urls;		/**< ordered list of URLs to try */
	GSList			*doneUrls;	/**< list of URLs we did already try, used for lookup to avoid loops */
	updateOptionsPtr	options;	/**< download options */
} *iconDownloadCtxtPtr;

static iconDownloadCtxtPtr
subscription_icon_download_ctxt_new ()
{
	return g_new0 (struct iconDownloadCtxt, 1);
}

static void
subscription_icon_download_ctxt_free (iconDownloadCtxtPtr ctxt)
{
	if (!ctxt)
		return;

	g_free (ctxt->id);

	g_slist_free_full (ctxt->urls, g_free);
	g_slist_free_full (ctxt->doneUrls, g_free);

	update_options_free (ctxt->options);
}

static void subscription_icon_download_next (iconDownloadCtxtPtr ctxt);

static void
subscription_icon_downloaded (const gchar *id)
{
	nodePtr node = node_from_id (id);

	node_load_icon (node);
	feed_list_view_update_node (id);
}

static void
subscription_icon_download_data_cb (const struct updateResult * const result, gpointer user_data, updateFlags flags)
{
	iconDownloadCtxtPtr ctxt = (iconDownloadCtxtPtr)user_data;
	gboolean	success = FALSE;

	debug4 (DEBUG_UPDATE, "icon download processing (%s, %d bytes, content type %s) for favicon %s", result->source, result->size, result->contentType, ctxt->id);

	if (result->data &&
	    result->size > 0 &&
	    result->contentType /*&&*/
	   /* the MIME type is wrong much too often, so we cannot check it... */
	   /*(!strncmp("image", result->contentType, 5))*/) {
		success = favicon_save_from_data (result, ctxt->id);

	} else {
		debug1 (DEBUG_UPDATE, "No data in download result for favicon %s!", ctxt->id);
	}

	if (!success) {
		subscription_icon_download_next (ctxt);
	} else {
		subscription_icon_downloaded (ctxt->id);
		subscription_icon_download_ctxt_free (ctxt);
	}
}

static void
subscription_icon_download_html_cb (const struct updateResult * const result, gpointer user_data, updateFlags flags)
{
	iconDownloadCtxtPtr ctxt = (iconDownloadCtxtPtr)user_data;
	gboolean success = FALSE;

	if (result->size > 0 && result->data) {
		GSList *links = html_discover_favicon (result->data, result->source);
		if (links) {
			/* We have a definitive set of favicons now as reported
			   by the website. Therefore we drop all guess work we
			   have so far in favour of this list.

			   This is important as the first downloadable link wins.
			   And we have "<url>/favicon.ico" quite early in the
			   original list. Our new list however is sorted by
			   icon size for highest quality first. */
			g_slist_free (g_steal_pointer (&(ctxt->urls)));
			ctxt->urls = links;
			success = TRUE;
		}
	}

	if (!success)
		debug2 (DEBUG_UPDATE, "No links in HTML '%s' for icon '%s' found!", result->source, ctxt->id);

	subscription_icon_download_next (ctxt);	/* continue favicon download */
}

static GRegex *image_extension_match = NULL;

/* Performs a download of the first URL in ctxt->urls */
static void
subscription_icon_download_next (iconDownloadCtxtPtr ctxt)
{
	gchar			*url;
	UpdateRequest		*request;
	update_result_cb	callback;

	if (g_slist_length (ctxt->doneUrls) > ICON_DOWNLOAD_MAX_URLS) {
		debug2 (DEBUG_UPDATE, "Stopping icon '%s' discovery after trying %d URLs.", ctxt->id, ICON_DOWNLOAD_MAX_URLS);
		subscription_icon_download_ctxt_free (ctxt);
		return;
	}

	if (ctxt->urls) {
		url = (gchar *)ctxt->urls->data;
		ctxt->urls = g_slist_remove (ctxt->urls, url);
		ctxt->doneUrls = g_slist_append (ctxt->doneUrls, url);

		debug2 (DEBUG_UPDATE, "Icon '%s' trying URL: '%s'", ctxt->id, url);

		request = update_request_new (
			url,
			NULL, 	// updateState
			ctxt->options
		);

		if (!image_extension_match)
			image_extension_match = g_regex_new ("\\.(ico|png|gif|jpg|svg)$", G_REGEX_CASELESS, 0, NULL);

		if (g_regex_match (image_extension_match, url, 0, NULL))
			callback = subscription_icon_download_data_cb;
		else
			callback = subscription_icon_download_html_cb;

		update_execute_request (node_from_id (ctxt->id), request, callback, ctxt, FEED_REQ_PRIORITY_HIGH | FEED_REQ_NO_FEED);
	} else {
		debug1 (DEBUG_UPDATE, "Icon '%s' discovery/download failed!", ctxt->id);
		subscription_icon_download_ctxt_free (ctxt);
	}
}

void
subscription_icon_update (subscriptionPtr subscription)
{
	iconDownloadCtxtPtr	ctxt;

	debug1 (DEBUG_UPDATE, "trying to download icon for \"%s\"", node_get_title (subscription->node));
 	subscription->updateState->lastFaviconPoll = g_get_real_time();

	ctxt = subscription_icon_download_ctxt_new ();
	ctxt->id = g_strdup (subscription->node->id);
	ctxt->urls = favicon_get_urls (subscription,
	                               node_get_base_url (subscription->node));

	/* Do not copy update options as it is too dangerous (especially
	   for online backends as for example TinyTinyRSS where we do not
	   want to send the TinyTinyRSS credentials to the original website
	   when fetching the favicon, see Github #678)!

	   For simplicity we do not support fetching favicons with Basic Auth

	   We just pass the proxy flag below. */
	ctxt->options = g_new0 (struct updateOptions, 1);
	ctxt->options->dontUseProxy = subscription->updateOptions->dontUseProxy;

	subscription_icon_download_next (ctxt);
}
