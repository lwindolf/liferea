/**
 * @file reedah_source_feed_list.c  Reedah feed list handling routines
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


#include "reedah_source_feed_list.h"

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
#include "xml.h" // FIXME

#include "fl_sources/opml_source.h"
#include "fl_sources/reedah_source.h"

static void
reedah_source_check_node_for_removal (nodePtr node, gpointer user_data)
{
	JsonArray	*array = (JsonArray *)user_data;
	GList		*iter, *elements;
	gboolean	found = FALSE;

	if (IS_FOLDER (node)) {
		/* Auto-remove folders if they do not have children */
		if (!node->children)
			feedlist_node_removed (node);

		node_foreach_child_data (node, reedah_source_check_node_for_removal, user_data);
	} else {
		elements = iter = json_array_get_elements (array);
		while (iter) {
			JsonNode *json_node = (JsonNode *)iter->data;
			// FIXME: Compare with unescaped string
			if (g_str_equal (node->subscription->source, json_get_string (json_node, "id") + 5)) {
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

/* subscription list merging functions */

static void
reedah_source_merge_feed (ReedahSourcePtr source, const gchar *url, const gchar *title, const gchar *id, nodePtr folder)
{
	nodePtr	node;

	node = feedlist_find_node (source->root, NODE_BY_URL, url);
	if (!node) {
		debug2 (DEBUG_UPDATE, "adding %s (%s)", title, url);
		node = node_new (feed_get_node_type ());
		node_set_title (node, title);
		node_set_data (node, feed_new ());

		node_set_subscription (node, subscription_new (url, NULL, NULL));
		node->subscription->type = source->root->source->type->feedSubscriptionType;

		/* Save Reedah feed id which we need to fetch items... */
		node->subscription->metadata = metadata_list_append (node->subscription->metadata, "reedah-feed-id", id);
		db_subscription_update (node->subscription);

		node_set_parent (node, source->root, -1);
		feedlist_node_imported (node);

		/**
		 * @todo mark the ones as read immediately after this is done
		 * the feed as retrieved by this has the read and unread
		 * status inherently.
		 */
		subscription_update (node->subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
		subscription_icon_update (node->subscription);
	} else {
		node_source_update_folder (node, folder);
	}
}

/* OPML subscription type implementation */

static void
reedah_subscription_opml_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	ReedahSourcePtr	source = (ReedahSourcePtr) subscription->node->data;

	subscription->updateJob = NULL;

	// FIXME: the following code is very similar to ttrss!
	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), "subscriptions"));
			GList		*iter, *elements, *citer, *celements;

			/* We expect something like this:

			   [{"id":"feed\/http:\/\/rss.slashdot.org\/Slashdot\/slashdot",
                             "title":"Slashdot",
                             "categories":[],
                             "firstitemmsec":"1368112925514",
                             "htmlUrl":"null"},
                           ...

			   Note that the data doesn't contain an URL.
			   We recover it from the id field.
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
						if (label) {
							folder = node_source_find_or_create_folder (source->root, label, label);
							break;
						}
						citer = g_list_next (citer);
					}
					g_list_free (celements);
				}

				/* ignore everything without a feed url */
				if (json_get_string (node, "id")) {
					reedah_source_merge_feed (source,
					                          json_get_string (node, "id") + 5,	// FIXME: Unescape string!
					                          json_get_string (node, "title"),
					                          json_get_string (node, "id"),
					                          folder);
				}
				iter = g_list_next (iter);
			}
			g_list_free (elements);

			/* Remove old nodes we cannot find anymore */
			node_foreach_child_data (source->root, reedah_source_check_node_for_removal, array);

			/* Save new subscription tree to OPML cache file */
			opml_source_export (subscription->node);
			subscription->node->available = TRUE;
		} else {
			g_print ("Invalid JSON returned on Reedah feed list request! >>>%s<<<", result->data);
		}

		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "reedah_subscription_cb(): ERROR: failed to get subscription list!");
	}

	if (!(flags & NODE_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));
}

/** functions for an efficient updating mechanism */

static void
reedah_source_opml_quick_update_helper (xmlNodePtr match, gpointer userdata)
{
	ReedahSourcePtr gsource = (ReedahSourcePtr) userdata;
	xmlNodePtr      xmlNode;
	xmlChar         *id, *newestItemTimestamp;
	nodePtr         node = NULL;
	const gchar     *oldNewestItemTimestamp;

	xmlNode = xpath_find (match, "./string[@name='id']");
	id = xmlNodeGetContent (xmlNode);

	if (g_str_has_prefix ((gchar *)id, "feed/"))
		node = feedlist_find_node (gsource->root, NODE_BY_URL, (gchar *)(id + strlen ("feed/")));
	else {
		xmlFree (id);
		return;
	}

	if (node == NULL) {
		xmlFree (id);
		return;
	}

	xmlNode = xpath_find (match, "./number[@name='newestItemTimestampUsec']");
	newestItemTimestamp = xmlNodeGetContent (xmlNode);

	oldNewestItemTimestamp = g_hash_table_lookup (gsource->lastTimestampMap, node->subscription->source);

	if (!oldNewestItemTimestamp ||
	    (newestItemTimestamp &&
	     !g_str_equal (newestItemTimestamp, oldNewestItemTimestamp))) {
		debug3(DEBUG_UPDATE, "ReedahSource: auto-updating %s "
		       "[oldtimestamp%s, timestamp %s]",
		       id, oldNewestItemTimestamp, newestItemTimestamp);
		g_hash_table_insert (gsource->lastTimestampMap,
				    g_strdup (node->subscription->source),
				    g_strdup ((gchar *)newestItemTimestamp));

		subscription_update (node->subscription, 0);
	}

	xmlFree (newestItemTimestamp);
	xmlFree (id);
}

static void
reedah_source_opml_quick_update_cb (const struct updateResult* const result, gpointer userdata, updateFlags flags)
{
	ReedahSourcePtr gsource = (ReedahSourcePtr) userdata;
	xmlDocPtr       doc;

	if (!result->data) {
		/* what do I do? */
		debug0 (DEBUG_UPDATE, "ReedahSource: Unable to get unread counts, this update is aborted.");
		return;
	}
	doc = xml_parse (result->data, result->size, NULL);
	if (!doc) {
		debug0 (DEBUG_UPDATE, "ReedahSource: The XML failed to parse, maybe the session has expired. (FIXME)");
		return;
	}

	xpath_foreach_match (xmlDocGetRootElement (doc),
			    "/object/list[@name='unreadcounts']/object",
			    reedah_source_opml_quick_update_helper, gsource);

	xmlFreeDoc (doc);
}

gboolean
reedah_source_opml_quick_update(ReedahSourcePtr source)
{
	subscriptionPtr subscription = source->root->subscription;

	UpdateRequest *request = update_request_new (
		source->root->source->api.unread_count,
		subscription->updateState,
		subscription->updateOptions
	);

	update_request_set_auth_value(request, source->root->source->authToken);

	update_execute_request (source, request, reedah_source_opml_quick_update_cb,
				source, 0);

	return TRUE;
}


static void
reedah_source_opml_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	reedah_subscription_opml_cb (subscription, result, flags);
}

static gboolean
reedah_source_opml_subscription_prepare_update_request (subscriptionPtr subscription, UpdateRequest *request)
{
	nodePtr node = subscription->node;
	ReedahSourcePtr	source = (ReedahSourcePtr)node->data;

	g_assert(node->source);
	if (node->source->loginState == NODE_SOURCE_STATE_NONE) {
		debug0(DEBUG_UPDATE, "ReedahSource: login");
		reedah_source_login (source, 0);
		return FALSE;
	}
	debug1 (DEBUG_UPDATE, "updating Reedah subscription (node id %s)", node->id);

	update_request_set_source (request, node->source->api.subscription_list);
	update_request_set_auth_value (request, node->source->authToken);

	return TRUE;
}

/* OPML subscription type definition */

struct subscriptionType reedahSourceOpmlSubscriptionType = {
	reedah_source_opml_subscription_prepare_update_request,
	reedah_source_opml_subscription_process_update_result
};
