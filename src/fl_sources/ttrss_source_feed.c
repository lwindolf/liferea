/**
 * @file ttrss_source_feed.c  tt-rss feed subscription routines
 * 
 * Copyright (C) 2010-2011 Lars Lindner <lars.lindner@gmail.com>
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

#include <glib.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "itemlist.h"
#include "itemset.h"
#include "json.h"
#include "metadata.h"
#include "subscription.h"
#include "xml.h"

#include "fl_sources/ttrss_source.h"

static void
ttrss_feed_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult* const result, updateFlags flags)
{
	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), "content"));
			GList		*iter = json_array_get_elements (array);
			GList		*items = NULL;

			/*
			   We expect to get something like this
			   
			   [{"id":118,
			     "unread":true,
			     "marked":false,
			     "updated":1287927675,
			     "is_updated":false,
			     "title":"IBM Says New ...",
			     "link":"http:\/\/rss.slashdot.org\/~r\/Slashdot\/slashdot\/~3\/ALuhNKO3NV4\/story01.htm",
			     "feed_id":"5",
			     "content":"coondoggie writes ..."
			    },
			    {"id":117,
			     "unread":true,
			     "marked":false,
			     "updated":1287923814,
                           [...]
                         */
                         
			while (iter) {
				JsonNode *node = (JsonNode *)iter->data;
				itemPtr item = item_new ();
				const gchar *content;
				gchar *xhtml;
				
				item_set_id (item, g_strdup_printf ("%lld", json_get_int (node, "id")));
				item_set_title (item, json_get_string (node, "title"));
				item_set_source (item, json_get_string (node, "link"));
				content = json_get_string (node, "content");
				xhtml = xhtml_extract_from_string (content, NULL);
				item_set_description (item, xhtml);
				xmlFree (xhtml);

				item->time = json_get_int (node, "updated");
				
				if (json_get_bool (node, "unread")) {
					item->readStatus = FALSE;
				}
				else {
					item->readStatus = TRUE;
				}
				if (json_get_bool (node, "marked"))
					item->flagStatus = TRUE;
					
				items = g_list_append (items, (gpointer)item);
				
				iter = g_list_next (iter);
			}
			
			/* merge against feed cache */
			if (items) {
				itemSetPtr itemSet = node_get_itemset (subscription->node);
				gint newCount = itemset_merge_items (itemSet, items, TRUE /* feed valid */, FALSE /* markAsRead */);
				itemlist_merge_itemset (itemSet);
				itemset_free (itemSet);

				feedlist_node_was_updated (subscription->node, newCount);
			}

			subscription->node->available = TRUE;
		} else {
			subscription->node->available = FALSE;

			g_string_append (((feedPtr)subscription->node->data)->parseErrors, _("Could not parse JSON returned by tt-rss API!"));
		}
	} else {
		subscription->node->available = FALSE;
	}
}

static gboolean
ttrss_feed_subscription_prepare_update_request (subscriptionPtr subscription, 
                                                 struct updateRequest *request)
{
	debug0 (DEBUG_UPDATE, "ttrss_feed_subscription_prepare_update_request()");
	nodePtr root = node_source_root_from_node (subscription->node);
	ttrssSourcePtr source = (ttrssSourcePtr) root->data;
	const gchar *feed_id;

	debug0 (DEBUG_UPDATE, "preparing tt-rss feed subscription for update");
	
	g_assert(source); 
	if (source->loginState == TTRSS_SOURCE_STATE_NONE) { 
		subscription_update (root->subscription, 0);
		return FALSE;
	}
	
	feed_id = metadata_list_get (subscription->metadata, "ttrss-feed-id");
	if (!feed_id) {
		g_warning ("tt-rss feed without id! (%s)", subscription->node->title);
		return FALSE;
	}
	request->postdata = g_strdup_printf (TTRSS_JSON_HEADLINES, source->session_id, feed_id, 15 /* items to fetch */ );
	update_request_set_source (request, g_strdup_printf (TTRSS_URL, metadata_list_get (root->subscription->metadata, "ttrss-url")));

	return TRUE;
}

struct subscriptionType ttrssSourceFeedSubscriptionType = {
	ttrss_feed_subscription_prepare_update_request,
	ttrss_feed_subscription_process_update_result
};

