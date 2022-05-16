/**
 * @file theoldreader_source_feed_list.c  TheOldReader feed list handling
 *
 * Copyright (C) 2013-2018  Lars Windolf <lars.windolf@gmx.de>
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


#include "theoldreader_source_feed_list.h"

#include <glib.h>
#include <string.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "folder.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "subscription_icon.h"
#include "xml.h"

#include "fl_sources/opml_source.h"
#include "fl_sources/theoldreader_source.h"

static void
theoldreader_source_check_node_for_removal (nodePtr node, gpointer user_data)
{
	JsonArray	*array = (JsonArray *)user_data;
	GList		*iter, *elements;
	gboolean	found = FALSE;

	if (IS_FOLDER (node)) {
		/* Auto-remove folders if they do not have children */
		if (!node->children)
			feedlist_node_removed (node);

		node_foreach_child_data (node, theoldreader_source_check_node_for_removal, user_data);
	} else {
		elements = iter = json_array_get_elements (array);
		while (iter) {
			JsonNode *json_node = (JsonNode *)iter->data;
			if (g_str_equal (node->subscription->source, json_get_string (json_node, "url"))) {
				debug1 (DEBUG_UPDATE, "node: %s", node->subscription->source);
				found = TRUE;
				break;
			}
			iter = g_list_next (iter);
		}
		g_list_free (elements);

		if (!found)
			feedlist_node_removed (node);
	}
}

static void
theoldreader_source_merge_feed (TheOldReaderSourcePtr source, const gchar *url, const gchar *title, const gchar *id, nodePtr folder)
{
	nodePtr	node;

	node = feedlist_find_node (source->root, NODE_BY_URL, url);
	if (!node) {
		debug2 (DEBUG_UPDATE, "adding %s (%s)", title, url);
		node = node_new (feed_get_node_type ());
		node_set_title (node, title);
		node_set_data (node, feed_new ());
		node_set_parent (node, folder?folder:source->root, -1);
		node_set_subscription (node, subscription_new (url, NULL, NULL));
		node->subscription->type = source->root->source->type->feedSubscriptionType;
		node->subscription->metadata = metadata_list_append (node->subscription->metadata, "theoldreader-feed-id", id);

		feedlist_node_imported (node);

		subscription_update (node->subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
		subscription_icon_update (node->subscription);

	} else {
		node_set_title (node, title);
		node_source_update_folder (node, folder);
	}
}

/* JSON subscription list processing implementation */

static void
theoldreader_subscription_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	TheOldReaderSourcePtr	source = (TheOldReaderSourcePtr) subscription->node->data;

	debug1 (DEBUG_UPDATE,"theoldreader_subscription_cb(): %s", result->data);

	subscription->updateJob = NULL;

	// FIXME: the following code is very similar to ttrss!
	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), "subscriptions"));
			GList		*iter, *elements, *citer, *celements;

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
				nodePtr folder = NULL;

				/* Check for categories, if there use first one as folder */
				categories = json_get_node (node, "categories");
				if (categories && JSON_NODE_TYPE (categories) == JSON_NODE_ARRAY) {
					citer = celements = json_array_get_elements (json_node_get_array (categories));
					while (citer) {
						const gchar *label = json_get_string ((JsonNode *)citer->data, "label");
						const gchar *id    = json_get_string ((JsonNode *)citer->data, "id");
						if (label) {
							folder = node_source_find_or_create_folder (source->root, label, label);

							/* Store category id also for folder (needed when subscribing new feeds) */
							g_hash_table_insert (source->folderToCategory, g_strdup (folder->id), g_strdup (id));

							break;
						}
						citer = g_list_next (citer);
					}
					g_list_free (celements);
				}

				/* ignore everything without a feed url */
				if (json_get_string (node, "url")) {
					theoldreader_source_merge_feed (source,
					                                json_get_string (node, "url"),
					                                json_get_string (node, "title"),
					                                json_get_string (node, "id"),
									folder);
				}
				iter = g_list_next (iter);
			}
			g_list_free (elements);

			/* Remove old nodes we cannot find anymore */
			node_foreach_child_data (source->root, theoldreader_source_check_node_for_removal, array);

			/* Save new subscription tree to OPML cache file */
			opml_source_export (subscription->node);

			subscription->node->available = TRUE;
		} else {
			g_print ("Invalid JSON returned on TheOldReader request! >>>%s<<<", result->data);
		}

		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "theoldreader_subscription_cb(): ERROR: failed to get subscription list!");
	}

	if (!(flags & NODE_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));
}

static void
theoldreader_source_opml_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	theoldreader_subscription_cb (subscription, result, flags);
}

static gboolean
theoldreader_source_opml_subscription_prepare_update_request (subscriptionPtr subscription, UpdateRequest *request)
{
	nodePtr node = subscription->node;

	g_assert(node->source);
	if (node->source->loginState == NODE_SOURCE_STATE_NONE) {
		debug0 (DEBUG_UPDATE, "TheOldReaderSource: login");
		theoldreader_source_login (node->data, 0);
		return FALSE;
	}
	debug1 (DEBUG_UPDATE, "updating TheOldReader subscription (node id %s)", node->id);

	update_request_set_source (request, node->source->api.subscription_list);
	update_request_set_auth_value (request, node->source->authToken);

	return TRUE;
}

/* OPML subscription type definition */

struct subscriptionType theOldReaderSourceOpmlSubscriptionType = {
	theoldreader_source_opml_subscription_prepare_update_request,
	theoldreader_source_opml_subscription_process_update_result
};
