/**
 * @file ttrss_source_feed_list.c  tt-rss feed list handling routines.
 *
 * Copyright (C) 2010-2018  Lars Windolf <lars.windolf@gmx.de>
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
#include "folder.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "subscription_icon.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/ttrss_source.h"

/* subscription list merging functions */

static void
ttrss_source_check_node_for_removal (nodePtr node, gpointer user_data)
{
	JsonArray	*array = (JsonArray *)user_data;
	GList		*iter, *elements;
	gboolean	found = FALSE;

	if (IS_FOLDER (node)) {
		/* Auto-remove folders if they do not have children */
		if (!node->children)
			feedlist_node_removed (node);

		node_foreach_child_data (node, ttrss_source_check_node_for_removal, user_data);
	} else {
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
	}
}

static void
ttrss_source_merge_feed (ttrssSourcePtr source, const gchar *url, const gchar *title, gint64 id, nodePtr folder)
{
	nodePtr		node;
	gchar		*tmp;

	/* check if node to be merged already exists */
	node = feedlist_find_node (source->root, NODE_BY_URL, url);

	if (!node) {
		debug2 (DEBUG_UPDATE, "adding %s (%s)", title, url);
		node = node_new (feed_get_node_type ());
		node_set_title (node, title);
		node_set_data (node, feed_new ());

		node_set_subscription (node, subscription_new (url, NULL, NULL));
		node->subscription->type = source->root->source->type->feedSubscriptionType;

		/* Save tt-rss feed id which we need to fetch items... */
		tmp = g_strdup_printf ("%" G_GINT64_FORMAT, id);
		metadata_list_set (&node->subscription->metadata, "ttrss-feed-id", tmp);
		g_free (tmp);

		node_set_parent (node, folder?folder:source->root, -1);
		feedlist_node_imported (node);

		/**
		 * @todo mark the ones as read immediately after this is done
		 * the feed as retrieved by this has the read and unread
		 * status inherently.
		 */
		subscription_update (node->subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
		subscription_icon_update (node->subscription);

		/* Important: we must not loose the feed id! */
		db_subscription_update (node->subscription);
	} else {
		node_source_update_folder (node, folder);
	}
}

/* source subscription type implementation */

static void
ttrss_source_subscription_list_cb (const struct updateResult * const result, gpointer user_data, guint32 flags)
{
	subscriptionPtr subscription = (subscriptionPtr) user_data;
	ttrssSourcePtr source = (ttrssSourcePtr) subscription->node->data;

	debug1 (DEBUG_UPDATE,"ttrss_subscription_cb(): %s", result->data);

	subscription->updateJob = NULL;

	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonNode	*content = json_get_node (json_parser_get_root (parser), "content");
			JsonArray	*array;
			GList		*iter, *elements;

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


			   Or an error message that could look like this:

			      {"seq":null,"status":1,"content":{"error":"NOT_LOGGED_IN"}}

			   */

			if (!content || (JSON_NODE_TYPE (content) != JSON_NODE_ARRAY)) {
				debug0 (DEBUG_UPDATE, "ttrss_subscription_cb(): Failed to get subscription list!");
				subscription->node->available = FALSE;
				return;
			}

			array = json_node_get_array (content);
			elements = iter = json_array_get_elements (array);
			/* Add all new nodes we find */
			while (iter) {
				JsonNode *node = (JsonNode *)iter->data;

				/* Get category id */
				gchar *category = NULL;
				gint cat_id = json_get_int (node, "cat_id");
				if (cat_id > 0)
					category = g_strdup_printf ("%d", cat_id);

				/* ignore everything without a feed url */
				if (json_get_string (node, "feed_url")) {
					ttrss_source_merge_feed (source,
					                         json_get_string (node, "feed_url"),
					                         json_get_string (node, "title"),
					                         json_get_int (node, "id"),
								 node_source_find_or_create_folder (source->root, category, NULL));
				}
				iter = g_list_next (iter);
			}
			g_list_free (elements);

			/* Remove old nodes we cannot find anymore */
			node_foreach_child_data (source->root, ttrss_source_check_node_for_removal, array);

			/* Save new subscription tree to OPML cache file */
			opml_source_export (subscription->node);

			subscription->node->available = TRUE;
		} else {
			g_print ("Invalid JSON returned on TinyTinyRSSS request! >>>%s<<<", result->data);
		}

		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "ttrss_subscription_cb(): ERROR: failed to get TinyTinyRSS subscription list!");
	}

	if (!(flags & NODE_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));
}

static void
ttrss_source_update_subscription_list (ttrssSourcePtr source, subscriptionPtr subscription)
{
	UpdateRequest	*request;
	gchar		*source_uri;

	source_uri = g_strdup_printf (TTRSS_URL, source->url);

	request = update_request_new (
		source_uri,
		subscription->updateState,
		subscription->updateOptions
	);

	g_free (source_uri);

	request->postdata = g_strdup_printf (TTRSS_JSON_SUBSCRIPTION_LIST, source->session_id);

	subscription->updateJob = update_execute_request (subscription, request, ttrss_source_subscription_list_cb, subscription, FEED_REQ_NO_FEED);
}

static void
ttrss_source_merge_categories (ttrssSourcePtr source, nodePtr parent, gint parentId, JsonNode *items)
{
	JsonArray	*array = json_node_get_array (items);
	GList		*iter, *elements;

	elements = iter = json_array_get_elements (array);
	while (iter) {
		JsonNode *node = (JsonNode *)iter->data;

		gint id = json_get_int (node, "bare_id");
		if (id > 0) {
			const gchar *type = json_get_string (node, "type");
			const gchar *name = json_get_string (node, "name");

			/* ignore everything without a name or bare_id */
			if (name) {

				/* Process child categories */
				if (type && g_str_equal (type, "category")) {
					nodePtr folder;
					gchar *folderId = g_strdup_printf ("%d", id);

					debug2 (DEBUG_UPDATE, "TinyTinyRSS category id=%ld name=%s", id, name);
					folder = node_source_find_or_create_folder (parent, folderId, name);
					g_free (folderId);

					/* Process child categories ... */
					if (json_get_node (node, "items")) {
						/* Store category id also for folder (needed when subscribing new feeds) */
						g_hash_table_insert (source->folderToCategory, g_strdup (folder->id), GINT_TO_POINTER (id));

						/* Recurse... */
						ttrss_source_merge_categories (source, folder, id, json_get_node (node, "items"));
					}
				/* Process child feeds */
				} else {
					debug3 (DEBUG_UPDATE, "TinyTinyRSS feed=%s folder=%d (%ld)", name, parentId, id);
					g_hash_table_insert (source->categories, GINT_TO_POINTER (id), GINT_TO_POINTER (parentId));
				}
			}

		}
		iter = g_list_next (iter);
	}
	g_list_free (elements);
}

static void
ttrss_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	ttrssSourcePtr		source = (ttrssSourcePtr) subscription->node->data;

	debug1 (DEBUG_UPDATE, "ttrss_subscription_process_update_result: %s", result->data);

	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonNode	*content = json_get_node (json_parser_get_root (parser), "content");
			JsonNode	*categories, *items;

			/* We expect something like this:

				{"categories":{"identifier":"id","label":"name","items":[
				{"id":"CAT:-1","items":[
					{"id":"FEED:-4","name":"All articles","unread":1547,"type":"feed","error":"","updated":"","icon":"images\/tag.png","bare_id":-4,"auxcounter":0},
					{"id":"FEED:-3","name":"Fresh articles","unread":0,"type":"feed","error":"","updated":"","icon":"images\/fresh.png","bare_id":-3,"auxcounter":0},
					{"id":"FEED:-1","name":"Starred articles","unread":0,"type":"feed","error":"","updated":"","icon":"images\/mark_set.svg","bare_id":-1,"auxcounter":0},
					{"id":"FEED:-2","name":"Published articles","unread":0,"type":"feed","error":"","updated":"","icon":"images\/pub_set.svg","bare_id":-2,"auxcounter":0},
					{"id":"FEED:0","name":"Archived articles","unread":0,"type":"feed","error":"","updated":"","icon":"images\/archive.png","bare_id":0,"auxcounter":0},
					{"id":"FEED:-6","name":"Recently read","unread":0,"type":"feed","error":"","updated":"","icon":"images\/recently_read.png","bare_id":-6,"auxcounter":0}],
				"name":"Special","type":"category","unread":0,"bare_id":-1},
				{"id":"CAT:1","bare_id":1,"auxcounter":0,"name":"OSS","items":[
					{"id":"CAT:2","bare_id":2,"name":"News","items":[
						{"id":"FEED:4","bare_id":4,"auxcounter":0,"name":"Tiny Tiny RSS: Forum","checkbox":false,"unread":0,"error":"","icon":false,"param":"Jan 03, 13:15"},
						{"id":"FEED:3","bare_id":3,"auxcounter":0,"name":"Tiny Tiny RSS: New Releases","checkbox":false,"unread":0,"error":"","icon":false,"param":"Jan 03, 13:15"}],
					"checkbox":false,"type":"category","unread":0,"child_unread":0,"auxcounter":0,"param":"(2 feeds)"},
					{"id":"FEED:6","bare_id":6,"auxcounter":0,"name":"Ars Technica","checkbox":false,"unread":0,"error":"","icon":"feed-icons\/6.ico","param":"Sep 18, 20:43"},
					{"id":"FEED:7","bare_id":7,"auxcounter":0,"name":"LZone","checkbox":false,"unread":0,"error":"","icon":"feed-icons\/7.ico","param":"Jul 06, 23:09"}],
				"checkbox":false,"type":"category","unread":0,"child_unread":0,"param":"(4 feeds)"},
				{"id":"CAT:0","bare_id":0,"auxcounter":0,"name":"Uncategorized","items":[
					{"id":"FEED:5","bare_id":5,"auxcounter":0,"name":"Slashdot","checkbox":false,"error":"","icon":false,"param":"Jan 03, 13:15","unread":0,"type":"feed"}],
				"type":"category","checkbox":false,"unread":0,"child_unread":0,"param":"(1 feed)"}]}}}

			   So we need to:
 			     - ignore all negative categories
			     - treat feeds in category #0 as root level feeds
			     - traverse all categories > #1
			     - remember category ids in source->categories hash

			   As we need to perform a subscription list update anyway we can ignore all feed infos
			*/

			if (!content) {
				debug0 (DEBUG_UPDATE, "ttrss_subscription_process_update_result(): Failed to get subscription list!");
				subscription->node->available = FALSE;
				return;
			}

			categories = json_get_node (content, "categories");
			if (!categories) {
				debug0 (DEBUG_UPDATE, "ttrss_subscription_process_update_result(): Failed to get categories list: no 'categories' element found!");
				subscription->node->available = FALSE;
				return;
			}

			items = json_get_node (categories, "items");
			if (!items || (JSON_NODE_TYPE (items) != JSON_NODE_ARRAY)) {
				debug0 (DEBUG_UPDATE, "ttrss_subscription_process_update_result(): Failed to get categories list: no 'categories' element found!");
				subscription->node->available = FALSE;
				return;
			}

			/* Process categories tree recursively */
			g_hash_table_remove_all (source->categories);
			ttrss_source_merge_categories (source, source->root, 0, items);

			/* And trigger the actual feed fetching */
			ttrss_source_update_subscription_list (source, subscription);
		} else {
			g_print ("Invalid JSON returned on TinyTinyRSS request! >>>%s<<<", result->data);
		}

		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "ttrss_subscription_process_update_result(): Failed to get categories list!");
	}
}

static gboolean
ttrss_subscription_prepare_update_request (subscriptionPtr subscription, UpdateRequest *request)
{
	nodePtr node = subscription->node;
	ttrssSourcePtr	source = (ttrssSourcePtr) subscription->node->data;
	gchar *source_uri;

	debug0 (DEBUG_UPDATE, "ttrss_subscription_prepare_update_request");

	g_assert (node->source);
	if (node->source->loginState == NODE_SOURCE_STATE_NONE) {
		debug0 (DEBUG_UPDATE, "TinyTinyRSS login");
		ttrss_source_login (source, 0);
		return FALSE;
	}
	debug1 (DEBUG_UPDATE, "TinyTinyRSS updating subscription (node id %s)", node->id);

	/* Updating the TinyTinyRSS subscription means updating the list
	   of categories and the list of feeds in 2 requests and if the
	   installation is not self-updating to run a remote update for
	   each feed before fetching it's items */

	source_uri = g_strdup_printf (TTRSS_URL, source->url);
	update_request_set_source (request, source_uri);
	g_free (source_uri);
	request->postdata = g_strdup_printf (TTRSS_JSON_CATEGORIES_LIST, source->session_id);

	return TRUE;
}

/* OPML subscription type definition */

struct subscriptionType ttrssSourceSubscriptionType = {
	ttrss_subscription_prepare_update_request,
	ttrss_subscription_process_update_result
};
