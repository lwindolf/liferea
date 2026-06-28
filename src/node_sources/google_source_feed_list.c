/**
 * @file google_source_feed_list.c  Google reader feed list handling routines.
 * 
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2011 Peter Oliver
 * Copyright (C) 2011 Sergey Snitsaruk <narren96c@gmail.com>
 * Copyright (C) 2022-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "google_source_feed_list.h"

#include <glib.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "subscription_icon.h"

#include "node_sources/opml_source.h"
#include "node_sources/google_source.h"
#include "node_sources/google_reader_api_edit.h"

/* subscription list merging functions */

static void
google_source_check_node_for_removal (Node *node, gpointer user_data)
{
	JsonArray	*array = (JsonArray *)user_data;
	gboolean	found = FALSE;

	if (IS_FOLDER (node)) {
		/* Auto-remove folders if they do not have children */
		if (!node->children)
			feedlist_node_removed (node);
		else
			node_foreach_child_data (node, google_source_check_node_for_removal, user_data);
	} else {
		const gchar *feedId = metadata_list_get (node->subscription->metadata, "feed-id");
		if (!feedId)
			return;
		
		g_autoptr(GList) list = json_array_get_elements (array);
		GList *iter = list;
		while (iter) {
			JsonNode *json_node = (JsonNode *)iter->data;
			const gchar *url = json_get_string (json_node, "url");
			const gchar *id = json_get_string(json_node, "id");
			if (url && id &&
			    g_str_equal (node->subscription->source, url) &&
			    g_str_equal (feedId, id)) {
				debug (DEBUG_UPDATE, "check for removal node: %s (%s)", feedId, node->subscription->source);
				found = TRUE;
				break;
			}
			iter = g_list_next (iter);
		}
		if (!found)
			feedlist_node_removed (node);
	}
}

static void
google_source_merge_feed (Node *root, const gchar *url, const gchar *title, const gchar *id, Node *folder)
{
	Node *node = feedlist_find_node (root, NODE_BY_URL, url);
	if (!node) {
		debug (DEBUG_UPDATE, "adding %s (id=%s, url=%s)", title, id, url);
		node = node_new ("feed");
		node_set_title (node, title);
		node_set_parent (node, folder?folder:root, -1);
		node_set_subscription (node, subscription_new (url, NULL, NULL));
		node->subscription->type = root->source->type->feedSubscriptionType;
		node->subscription->metadata = metadata_list_append (node->subscription->metadata, "feed-id", id);

		feedlist_node_imported (node);

		subscription_update (node->subscription, UPDATE_REQUEST_RESET_TITLE | UPDATE_REQUEST_PRIORITY_HIGH);
		subscription_icon_update (node->subscription);

	} else {
		node_set_title (node, title);
		node_source_update_folder (node, folder?folder:root);
	}
}

/* subscription type implementation */

static void
google_source_feed_list_subscription_process_update_result (subscriptionPtr subscription, const UpdateResult * const result, updateFlags flags)
{
	Node *root = subscription->node;

	debug (DEBUG_UPDATE, "google_source_feed_list_subscription_process_update_result(): %s", result->data);

	while (true) {
		if (!result->data || result->httpstatus != 200) {
			debug (DEBUG_UPDATE, "google_source: ERROR: failed to get subscription list!");
 			root->available = FALSE;
			return;
		}

		g_autoptr(JsonParser) parser = json_parser_new ();
		if (!json_parser_load_from_data (parser, result->data, -1, NULL)) {
			debug (DEBUG_UPDATE, "google_source: Received invalid JSON!");
			root->available = FALSE;
			return;
		}

		JsonNode	*j;
		JsonArray	*array;
		GList		*iter, *citer, *celements;
		g_autoptr(GList) elements = NULL;

		j = json_parser_get_root (parser);
		if (j)
			j = json_get_node (j, "subscriptions");
		if (j && JSON_NODE_HOLDS_ARRAY(j))
			array = json_node_get_array (j);
		else {
			debug (DEBUG_UPDATE, "google_source: JSON without 'subscriptions' array received!");
			root->available = FALSE;
			return;
		}

		/* We expect something like this:

			[{"id":"feed/51d49b79d1716c7b18000025",
			"title":"LZone",
			"categories":[{"id":"user/-/label/myfolder","label":"myfolder"}],
			"sortid":"51d49b79d1716c7b18000025",
			"firstitemmsec":"1371403150181",
			"url":"https://lzone.de/rss.xml",
			"htmlUrl":"https://lzone.de",
			"iconUrl":"http://s.yeoldereader.com/system/uploads/feed/picture/5152/884a/4dce/57aa/7e00/icon_0a6a.ico"},
			...
		*/
		elements = iter = json_array_get_elements (array);
		/* Add all new nodes we find */
		while (iter) {
			JsonNode *categories, *node = (JsonNode *)iter->data;
			Node *folder = NULL;

			/* Check for categories, if there use first one as folder */
			categories = json_get_node (node, "categories");
			if (categories && JSON_NODE_TYPE (categories) == JSON_NODE_ARRAY) {
				citer = celements = json_array_get_elements (json_node_get_array (categories));
				while (citer) {
					const gchar *label = json_get_string ((JsonNode *)citer->data, "label");
					const gchar *id    = json_get_string ((JsonNode *)citer->data, "id");
					if (label && id) {
						folder = node_source_find_or_create_folder (root, label, label);

						/* Store category id also for folder (needed when subscribing new feeds) */
						GHashTable *h = g_object_get_data (G_OBJECT (root), "folderToCategory");
						g_hash_table_insert (h, g_strdup (folder->id), g_strdup (id));

						break;
					}
					citer = g_list_next (citer);
				}
				g_list_free (celements);
			}

			/* ignore everything without a feed url */
			if (json_get_string (node, "url")) {
				google_source_merge_feed (root,
								json_get_string (node, "url"),
								json_get_string (node, "title"),
								json_get_string (node, "id"),
								folder);
			}
			iter = g_list_next (iter);
		}

		node_foreach_child_data (root, google_source_check_node_for_removal, array);
		opml_source_export (root);

		root->available = TRUE;
		break;
	}

	if (!(flags & NODE_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (root, node_update_subscription, GUINT_TO_POINTER (0));
}

static gboolean
google_source_feed_list_subscription_prepare_update_request (subscriptionPtr subscription, UpdateRequest *request)
{
	Node *root = subscription->node;

	if (root->source->loginState == NODE_SOURCE_STATE_NONE) {
		debug (DEBUG_UPDATE, "google_source: login needed first!");
		google_source_login (root, 0);
		return FALSE;
	}
	debug (DEBUG_UPDATE, "google_source: updating subscription list (node id %s)", root->id);
	
	update_request_set_source (request, root->source->api.subscription_list);
	update_request_set_auth_value (request, root->source->authToken);
	
	return TRUE;
}

/* OPML subscription type definition */

struct subscriptionType googleSourceOpmlSubscriptionType = {
	google_source_feed_list_subscription_prepare_update_request,
	google_source_feed_list_subscription_process_update_result
};

