/**
 * @file ttrss_source_feed_list.c  tt-rss feed list handling routines.
 * 
 * Copyright (C) 2010-2011  Lars Windolf <lars.lindner@gmail.com>
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


#include "ttrss_source_feed_list.h"

#include <glib.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/ttrss_source.h"

static void
ttrss_source_merge_feed (ttrssSourcePtr source, const gchar *url, const gchar *title, gint64 id)
{
	nodePtr	node;
	GSList	*iter;
	gchar	*tmp;

	/* check if node to be merged already exists */
	iter = source->root->children;
	while (iter) {
		node = (nodePtr)iter->data;
		if (g_str_equal (node->subscription->source, url))
			return;
		iter = g_slist_next (iter);
	}
	
	debug2 (DEBUG_UPDATE, "adding %s (%s)", title, url);
	node = node_new (feed_get_node_type ());
	node_set_title (node, title);
	node_set_data (node, feed_new ());
		
	node_set_subscription (node, subscription_new (url, NULL, NULL));
	node->subscription->type = &ttrssSourceFeedSubscriptionType;
	
	/* Save tt-rss feed id which we need to fetch items... */
	tmp = g_strdup_printf ("%" G_GINT64_FORMAT, id);
	metadata_list_set (&node->subscription->metadata, "ttrss-feed-id", tmp);
	g_free (tmp);
	
	node_set_parent (node, source->root, -1);
	feedlist_node_imported (node);
		
	/**
	 * @todo mark the ones as read immediately after this is done
	 * the feed as retrieved by this has the read and unread
	 * status inherently.
	 */
	subscription_update (node->subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
	subscription_update_favicon (node->subscription);
	
	/* Important: we must not loose the feed id! */
	db_subscription_update (node->subscription);
}

/* source subscription type implementation */

static void
ttrss_subscription_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	ttrssSourcePtr source = (ttrssSourcePtr) subscription->node->data;

	debug1 (DEBUG_UPDATE,"ttrss_subscription_cb(): %s", result->data);
	
	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), "content"));
			GList		*iter, *elements;
			GSList		*siter;
		
			/* We expect something like this:
			
			[ {"feed_url":"http://feeds.arstechnica.com/arstechnica/everything", 
			   "title":"Ars Technica", 
			   "id":6, 
			   "unread":20, 
			   "has_icon":true, 
			   "cat_id":0, 
			   "last_updated":1287853210}, 
			  {"feed_url":"http://rss.slashdot.org/Slashdot/slashdot", 
			   "title":"Slashdot", 
			   "id":5, 
			   "unread":33, 
			   "has_icon":true, 
			   "cat_id":0, 
			   "last_updated":1287853206}, 
			   [...]
			   
			   */

			elements = iter = json_array_get_elements (array);
			/* Add all new nodes we find */
			while (iter) {
				JsonNode *node = (JsonNode *)iter->data;
				
				/* ignore everything without a feed url */
				if (json_get_string (node, "feed_url")) {
					ttrss_source_merge_feed (source, 
					                         json_get_string (node, "feed_url"),
					                         json_get_string (node, "title"),
					                         json_get_int (node, "id"));
				}
				iter = g_list_next (iter);
			}
			g_list_free (elements);

			/* Remove old nodes we cannot find anymore */
			siter = source->root->children;
			while (siter) {
				nodePtr node = (nodePtr)siter->data;
				gboolean found = FALSE;
				
				elements = iter = json_array_get_elements (array);
				while (iter) {
					JsonNode *json_node = (JsonNode *)iter->data;
					if (g_str_equal (node->subscription->source, json_get_string (json_node, "feed_url"))) {
						debug1 (DEBUG_UPDATE, "node: %s", node->subscription->source);
						found = TRUE;
						break;
					}
					iter = g_list_next (iter);
				}
				g_list_free (elements);

				if (!found)			
					feedlist_node_removed (node);
				
				siter = g_slist_next (siter);
			}
			
			opml_source_export (subscription->node);	/* save new feeds to feed list */				   
			subscription->node->available = TRUE;			
			//return;
		} else {
			g_warning ("Invalid JSON returned on tt-rss request! >>>%s<<<", result->data);
		}

		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "ttrss_subscription_cb(): ERROR: failed to get subscription list!");
	}

	if (!(flags & TTRSS_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));
			
}

static void
ttrss_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	debug0 (DEBUG_UPDATE, "ttrss_subscription_process_update_result");
	ttrss_subscription_cb (subscription, result, flags);
}

static gboolean
ttrss_subscription_prepare_update_request (subscriptionPtr subscription, struct updateRequest *request)
{
	debug0 (DEBUG_UPDATE, "ttrs_subscription_prepare_update_request");
	ttrssSourcePtr	source = (ttrssSourcePtr) subscription->node->data;
	gchar *source_uri;

	g_assert (source);
	if (source->loginState == TTRSS_SOURCE_STATE_NONE) {
		debug0 (DEBUG_UPDATE, "TtRssSource: login");
		ttrss_source_login (source, 0);
		return FALSE;
	}
	debug1 (DEBUG_UPDATE, "updating tt-rss subscription (node id %s)", subscription->node->id);

	// FIXME: if (!source->selfUpdating) trigger remote update first!

	source_uri = g_strdup_printf (TTRSS_URL, metadata_list_get (subscription->metadata, "ttrss-url"));
	update_request_set_source (request, source_uri);
	g_free (source_uri);
	request->postdata = g_strdup_printf (TTRSS_JSON_SUBSCRIPTION_LIST, source->session_id);
	
	return TRUE;
}

/* OPML subscription type definition */

struct subscriptionType ttrssSourceSubscriptionType = {
	ttrss_subscription_prepare_update_request,
	ttrss_subscription_process_update_result
};
