/**
 * @file subscription.c  Downloading suitable subscription icons
 *
 * Copyright (C) 2003-2020 Lars Windolf <lars.windolf@gmx.de>
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

typedef struct iconDownloadCtxt {
	gchar		        *id;		/**< icon cache id (=node id) */
	GSList			*urls;		/**< ordered list of URLs to try */
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
	GSList  *iter;

	if (!ctxt) return;
	g_free (ctxt->id);

	for (iter = ctxt->urls; iter; iter = g_slist_next (iter))
		g_free (iter->data);

	g_slist_free (ctxt->urls);
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
	gchar		*tmp;
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
	
	if (result->size > 0 && result->data) {
		gchar *iconUri = html_discover_favicon (result->data, result->source);
		if (iconUri) {
			updateRequestPtr request;
			
			debug2 (DEBUG_UPDATE, "Found link for favicon %s: %s", ctxt->id, iconUri);
			request = update_request_new ();
			request->source = iconUri;
			request->options = update_options_copy (ctxt->options);
			update_execute_request (node_from_id (ctxt->id), request, subscription_icon_download_data_cb, ctxt, flags);

			return;
		}
	}

	debug2 (DEBUG_UPDATE, "No links in HTML '%s' for icon '%s' found!", result->source, ctxt->id);
	subscription_icon_download_next (ctxt);	/* no sucess, try next... */
}

/* Performs a download of the first URL in ctxt->urls */
static void
subscription_icon_download_next (iconDownloadCtxtPtr ctxt)
{
	gchar			*url;
	updateRequestPtr	request;
	update_result_cb	callback;

	debug_enter("subscription_icon_download_next");
	
	if (ctxt->urls) {
		url = (gchar *)ctxt->urls->data;
		ctxt->urls = g_slist_remove (ctxt->urls, url);
		debug2 (DEBUG_UPDATE, "Icon '%s' trying URL: '%s'", ctxt->id, url);

		request = update_request_new ();
		request->source = url;
		request->options = update_options_copy (ctxt->options);

		if (strstr (url, "/favicon.ico"))
			callback = subscription_icon_download_data_cb;
		else
			callback = subscription_icon_download_html_cb;

		update_execute_request (node_from_id (ctxt->id), request, callback, ctxt, FEED_REQ_PRIORITY_HIGH);
	} else {
		debug1 (DEBUG_UPDATE, "Icon '%s' could not be downloaded!", ctxt->id);
		subscription_icon_download_ctxt_free (ctxt);
	}
	
	debug_exit ("subscription_icon_download_next");
}

void
subscription_icon_update (subscriptionPtr subscription)
{
	iconDownloadCtxtPtr	ctxt;

	debug1 (DEBUG_UPDATE, "trying to download favicon.ico for \"%s\"", node_get_title (subscription->node));
  subscription->updateState->lastFaviconPoll = g_get_real_time();

	ctxt = subscription_icon_download_ctxt_new ();
	ctxt->id = g_strdup (subscription->node->id);
	ctxt->options = update_options_copy (subscription->updateOptions);
	ctxt->urls = favicon_get_urls (subscription,
	                               node_get_base_url (subscription->node));

	subscription_icon_download_next (ctxt);
}
