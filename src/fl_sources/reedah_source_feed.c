/**
 * @file reedah_source_feed.c  Reedah feed subscription routines
 *
 * Copyright (C) 2013-2022  Lars Windolf <lars.windolf@gmx.de>
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
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "common.h"
#include "debug.h"
#include "db.h"
#include "feedlist.h"
#include "item_state.h"
#include "itemlist.h"
#include "json.h"
#include "json_api_mapper.h"
#include "metadata.h"
#include "node.h"
#include "reedah_source.h"
#include "subscription.h"
#include "xml.h"

void
reedah_source_migrate_node (nodePtr node)
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
reedah_item_callback (JsonNode *node, itemPtr item)
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
reedah_feed_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult* const result, updateFlags flags)
{
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
		mapping.read		= NULL;
		mapping.updated		= "updated";
		mapping.author		= "author";
		mapping.flag		= "marked";

		mapping.xhtml		= TRUE;
		mapping.negateRead	= TRUE;

		items = json_api_get_items (result->data, "items", &mapping, &reedah_item_callback);

		/* merge against feed cache */
		if (items) {
			itemSetPtr itemSet = node_get_itemset (subscription->node);
			subscription->node->newCount = itemset_merge_items (itemSet, items, TRUE /* feed valid */, FALSE /* markAsRead */);
			itemlist_merge_itemset (itemSet);
			itemset_free (itemSet);

			subscription->node->available = TRUE;
		} else {
			subscription->node->available = FALSE;
			g_string_append (((feedPtr)subscription->node->data)->parseErrors, _("Could not parse JSON returned by Reedah API!"));
		}
	} else {
		subscription->node->available = FALSE;
	}
}

static gboolean
reedah_feed_subscription_prepare_update_request (subscriptionPtr subscription,
                                                 UpdateRequest *request)
{
	debug0 (DEBUG_UPDATE, "preparing Reedah feed subscription for update\n");
	ReedahSourcePtr source = (ReedahSourcePtr) node_source_root_from_node (subscription->node)->data;

	g_assert(source);
	if (source->root->source->loginState == NODE_SOURCE_STATE_NONE) {
		subscription_update (node_source_root_from_node (subscription->node)->subscription, 0) ;
		return FALSE;
	}

	if (!metadata_list_get (subscription->metadata, "reedah-feed-id")) {
		g_print ("Skipping Reedah feed '%s' (%s) without id!", subscription->source, subscription->node->id);
		return FALSE;
	}

	gchar* source_escaped = g_uri_escape_string(metadata_list_get (subscription->metadata, "reedah-feed-id"), NULL, TRUE);
	// FIXME: move to .h
	// FIXME: do not use hard-coded 30
	gchar* newUrl = g_strdup_printf ("http://www.reedah.com/reader/api/0/stream/contents/%s?client=liferea&n=30", source_escaped);
	update_request_set_source (request, newUrl);
	g_free (newUrl);
	g_free (source_escaped);

	update_request_set_auth_value (request, source->root->source->authToken);
	return TRUE;
}

struct subscriptionType reedahSourceFeedSubscriptionType = {
	reedah_feed_subscription_prepare_update_request,
	reedah_feed_subscription_process_update_result
};
