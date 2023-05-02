/**
 * @file google_source_feed.c  Google reader feed subscription routines
 * 
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2022 Lars Windolf <lars.windolf@gmx.de>
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
#include "debug.h"

#include "feedlist.h"
#include "subscription.h"
#include "node.h"
#include "metadata.h"
#include "db.h"
#include "item_state.h"
#include "itemlist.h"
#include "json.h"
#include "json_api_mapper.h"
#include "fl_sources/google_source.h"
#include "fl_sources/google_reader_api_edit.h"

void
google_source_migrate_node(nodePtr node) 
{
	/* scan the node for bad ID's, if so, brutally remove the node */
	itemSetPtr itemset = node_get_itemset (node);
	GList *iter = itemset->ids;
	for (; iter; iter = g_list_next (iter)) {
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item && item->sourceId) {
			if (!g_str_has_prefix(item->sourceId, "tag:google.com")) {
				debug1(DEBUG_UPDATE, "Item with sourceId [%s] will be deleted.", item->sourceId);
				db_item_remove(GPOINTER_TO_UINT(iter->data));
			} 
		}
		if (item) item_unload (item);
	}

	/* cleanup */
	itemset_free (itemset);
}

static void
google_source_feed_item_cb (JsonNode *node, itemPtr item)
{
	JsonNode	*canonical, *categories;
	GList		*elements, *iter;

	/* Determine link: path is "canonical[0]/@href" */
	canonical = json_get_node (node, "canonical");
	if (canonical && JSON_NODE_TYPE (canonical) == JSON_NODE_ARRAY) {
		iter = elements = json_array_get_elements (json_node_get_array (canonical));
		while (iter) {
			const gchar *href = json_get_string ((JsonNode *)iter->data, "href");
			if (href) {
				item_set_source (item, href);
				break;
			}
			iter = g_list_next (iter);
		}

		g_list_free (elements);
	}

	/* Determine read state: check for category with ".*state/com.google/read" */
	categories = json_get_node (node, "categories");
	if (categories && JSON_NODE_TYPE (categories) == JSON_NODE_ARRAY) {
		iter = elements = json_array_get_elements (json_node_get_array (categories));
		while (iter) {
			const gchar *category = json_node_get_string ((JsonNode *)iter->data);
			if (category) {
				item->readStatus = (strstr (category, "state\\/com.google\\/read") != NULL);
				break;
			}
			iter = g_list_next (iter);
		}

		g_list_free (elements);
	}
}

static void
google_source_feed_subscription_process_update_result (const struct updateResult* const result, gpointer user_data, updateFlags flags)
{
	subscriptionPtr	subscription = (subscriptionPtr)user_data;
	
	if (result->data && result->httpstatus == 200) {
		GList		*items = NULL;
		jsonApiMapping	mapping;

		/*
		   We expect to get something like this

		   [{"crawlTimeMsec":"1375821312282",
		     "id"::"tag:google.com,reader:2005\/item\/4ee371db36f84de2",
		     "categories":["user\/15724899091976567759\/state\/com.google\/reading-list",
		                   "user\/15724899091976567759\/state\/com.google\/fresh"],
		     "title":"Firefox 23 Arrives With New Logo, Mixed Content Blocker, and Network Monitor",
		     "published":1375813680,
		     "updated":1375821312,
		     "alternate":[{"href":"http://rss.slashdot.org/~r/Slashdot/slashdot/~3/Q4450FchLQo/story01.htm","type":"text/html"}],
		     "canonical":[{"href":"http://slashdot.feedsportal.com/c/35028/f/647410/s/2fa2b59c/sc[...]", "type":"text/html"}],
		     "summary":{"direction":"ltr","content":"An anonymous reader writes [...]"},
		     "author":"Soulskill",
		     "origin":{"streamId":"feed/http://rss.slashdot.org/Slashdot/slashdot","title":"Slashdot",
		     "htmlurl":"http://slashdot.org/"
		    },

                   [...]
                 */
		
		/* Note: The link and read status cannot be mapped as there might be multiple ones
 		   so the callback helper function extracts the first from the array */
		mapping.id		= "id";
		mapping.title		= "title";
		mapping.link		= NULL;
		mapping.description	= "summary/content";
		mapping.read		= "read";
		mapping.updated		= "updated";
		mapping.author		= "author";
		mapping.flag		= "marked";

		mapping.xhtml		= TRUE;
		mapping.negateRead	= TRUE;

		items = json_api_get_items (result->data, "items", &mapping, &google_source_feed_item_cb);

		/* merge against feed cache */
		if (items) {
			itemSetPtr itemSet = node_get_itemset (subscription->node);
			debug3 (DEBUG_UPDATE, "merging %d items into node %s (%s)\n", g_list_length(itemSet->ids), subscription->node->id, subscription->node->title);
			subscription->node->newCount = itemset_merge_items (itemSet, items, TRUE /* feed valid */, FALSE /* markAsRead */);
			itemlist_merge_itemset (itemSet);
			itemset_free (itemSet);
		} else {
			debug3 (DEBUG_UPDATE, "result empty %s (%s): %s\n", subscription->node->id, subscription->node->title, result->data);
		}
		subscription->node->available = TRUE;
	} else {
		subscription->node->available = FALSE;
		g_string_append (((feedPtr)subscription->node->data)->parseErrors, _("Could not parse JSON returned by Google Reader API!"));
	}
}

static void
google_source_feed_subscription_process_ids_result (subscriptionPtr subscription, const struct updateResult* const result, updateFlags flags)
{
	JsonParser	*parser;
	nodePtr		root = node_source_root_from_node (subscription->node);
	
	if (!(result->data && result->httpstatus == 200)) {
		subscription->node->available = FALSE;
		return;
	}
	
	/*
	   We expect to get something like this
	   
	  {
	  "itemRefs": [
	    {
	      "id": "62d3f48c511dbe78de004829",
	      "directStreamIds": [],
	      "timestampUsec": "1658057859783000"
	    },
	    {
	      "id": "62d3f48c511dbe78de00482a",
	      "directStreamIds": [],
	      "timestampUsec": "1658057858783000"
	    },
	    {
	      "id": "62d3f48c511dbe78de00482b",
	      "directStreamIds": [],
	      "timestampUsec": "1658057857783000"
	    },
	*/
	
	parser = json_parser_new ();
	if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
		JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), "itemRefs"));
		GList		*elements = json_array_get_elements (array);
		GList		*iter = elements;
		GString		*query = g_string_new ("");
		
		g_string_append_printf (query, "output=json&mediaRss=true&T=%s", 
		                        root->source->authToken + strlen("GoogleLogin auth="));

		while (iter) {
			JsonNode *node = (JsonNode *)iter->data;
			const gchar *id = json_get_string (node, "id");

			if (id)
				g_string_append_printf (query, "&i=%s", id);
				
			iter = g_list_next (iter);
		}
		
		debug3 (DEBUG_UPDATE, "got %d ids for %s (%s)\n", g_list_length(elements), subscription->node->id, subscription->node->title);
		
		/* Only if we got some ids */
		if (elements) {
			g_autofree gchar 	*url;
			UpdateRequest		*request;

			url = g_strdup_printf ("%s/reader/api/0/stream/items/contents", root->subscription->source);
		
			request = update_request_new (
				url,
				subscription->updateState,
				subscription->updateOptions
			);
			request->postdata = g_strdup (query->str);

			// Redundant to the token already passed in postdata, but FreshRSS fails without it
			update_request_set_auth_value (request, root->source->authToken);

			(void) update_execute_request (root->subscription,
			                               request,
			                               google_source_feed_subscription_process_update_result,
       			                               subscription,
       			                               flags);
		}

		g_list_free (elements);
		g_string_free (query, TRUE);
	} else {
		subscription->node->available = FALSE;
	}
	g_object_unref (parser);
}

static gboolean
google_source_feed_subscription_prepare_ids_request (subscriptionPtr subscription, 
                                                     UpdateRequest *request)
{
	debug0 (DEBUG_UPDATE, "preparing google reader feed subscription for update");
	nodePtr root = node_source_root_from_node (subscription->node);

	if (root->source->loginState == NODE_SOURCE_STATE_NONE) {
		subscription_update (root->subscription, 0);
		return FALSE;
	}
	
	if (!metadata_list_get (subscription->metadata, "feed-id")) {
		debug2 (DEBUG_UPDATE, "Skipping Google Reader API feed '%s' (%s) without id!", subscription->source, subscription->node->id);
		return FALSE;
	}

	if (!g_str_equal (request->source, GOOGLE_READER_BROADCAST_FRIENDS_URL)) {
		g_autofree gchar* sourceEscaped = g_uri_escape_string (metadata_list_get (subscription->metadata, "feed-id"), NULL, TRUE);
		g_autofree gchar* url;
		
		/* Note: we have to do a /stream/items/ids here as several Google Reader
		   clone implementations (e.g. Miniflux) do not implement /stream/contents.
		   
		   Also /stream/items/contents needs to be done via POST as some implementations
		   do not provide a GET endpoint (e.g. Miniflux) */
		                       
		// FIXME: move to API fields!
		// FIXME: do not use hard-coded 50
		// FIXME: consider passing nt=<epoch> (latest fetch timestamp)
		url = g_strdup_printf ("%s/reader/api/0/stream/items/ids?s=%s&client=liferea&n=50&output=json&merge=true",
		                       root->subscription->source,
		                       sourceEscaped);                       

		update_request_set_source (request, url);
		update_request_set_auth_value (request, root->source->authToken);
	}
	return TRUE;
}

struct subscriptionType googleSourceFeedSubscriptionType = {
	google_source_feed_subscription_prepare_ids_request,
	google_source_feed_subscription_process_ids_result
};


