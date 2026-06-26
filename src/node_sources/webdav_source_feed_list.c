/**
 * @file webdav_source_feed_list.c  WebDAV feed list handling routines.
 *
 * Copyright (C) 2026  Lars Windolf <lars.windolf@gmx.de>
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

#include "webdav_source_feed_list.h"

#include <json-glib/json-glib.h>

#include "debug.h"
#include "feedlist.h"
#include "item.h"
#include "item_state.h"
#include "itemset.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "node_provider.h"
#include "subscription.h"
#include "update.h"
#include "ui/auth_dialog.h"

typedef struct {
	gboolean is_folder;
	gchar  *node_id;
	gchar  *parent_id;
	gchar  *title;
	gchar  *source;
	gint64  feed_mtime;
	gint64  state_mtime;
} IndexEntry;

typedef struct {
	Node           *root;
	GHashTable     *remote_feed_ids;
	GHashTable     *remote_folder_ids;
	gboolean        uploaded_missing;
} MissingSyncCtx;

typedef struct {
	Node       *root;
	GHashTable *remote_folder_ids;
} FolderCleanupCtx;

typedef struct {
	Node       *root;
	GHashTable *entries_by_id;
	GHashTable *ensured_folders;
	GHashTable *visiting;
} FolderResolveCtx;

static void
index_entry_free (IndexEntry *e)
{
	g_free (e->source);
	g_free (e->title);
	g_free (e->parent_id);
	g_free (e->node_id);
	g_free (e);
}

static void
webdav_upload_missing_remote_entries (Node *parent, MissingSyncCtx *ctx)
{
	for (GSList *iter = parent->children; iter; iter = g_slist_next (iter)) {
		Node *node = (Node *)iter->data;

		if (IS_FOLDER (node)) {
			if (!g_hash_table_contains (ctx->remote_folder_ids, node->id)) {
				debug (DEBUG_UPDATE, "webdav_initial_sync: folder %s missing remotely", node->id);
				ctx->uploaded_missing = TRUE;
			}
			webdav_upload_missing_remote_entries (node, ctx);
			continue;
		}

		if (!IS_FEED (node) || !node->subscription)
			continue;

		const gchar *remote_id = webdav_feed_remote_id (node);
		if (remote_id && g_hash_table_contains (ctx->remote_feed_ids, remote_id))
			continue;

		debug (DEBUG_UPDATE, "webdav_initial_sync: uploading missing remote feed local=%s remote=%s", node->id, remote_id ? remote_id : "(null)");
		webdav_upload_feed (ctx->root, node->id);
		webdav_upload_state (ctx->root, node->id);
		ctx->uploaded_missing = TRUE;
	}
}

static Node *
webdav_find_feed_by_remote_id (Node *parent, const gchar *remote_id)
{
	if (!parent || !remote_id || !*remote_id)
		return NULL;

	for (GSList *iter = parent->children; iter; iter = g_slist_next (iter)) {
		Node *node = (Node *)iter->data;

		if (IS_FEED (node) && node->subscription) {
			const gchar *node_remote_id = webdav_feed_remote_id (node);
			if (node_remote_id && g_str_equal (node_remote_id, remote_id))
				return node;
		}

		if (IS_FOLDER (node)) {
			Node *found = webdav_find_feed_by_remote_id (node, remote_id);
			if (found)
				return found;
		}
	}

	return NULL;
}

static GList *
webdav_parse_index_json (const gchar *json_str)
{
	JsonParser *parser;
	JsonNode   *root;
	JsonObject *obj;
	JsonNode   *feeds_node;
	JsonArray  *feeds;
	GError     *err = NULL;
	GList      *result = NULL;
	guint       n;

	if (!json_str)
		return NULL;

	parser = json_parser_new ();
	if (!json_parser_load_from_data (parser, json_str, -1, &err)) {
		debug (DEBUG_UPDATE, "webdav_read_index: parse error: %s", err->message);
		g_error_free (err);
		g_object_unref (parser);
		return NULL;
	}

	root = json_parser_get_root (parser);
	if (!JSON_NODE_HOLDS_OBJECT (root)) {
		g_object_unref (parser);
		return NULL;
	}

	obj = json_node_get_object (root);
	if (!json_object_has_member (obj, "feeds") && !json_object_has_member (obj, "nodes")) {
		g_object_unref (parser);
		return NULL;
	}

	feeds_node = json_object_has_member (obj, "nodes")
	             ? json_object_get_member (obj, "nodes")
	             : json_object_get_member (obj, "feeds");
	if (!JSON_NODE_HOLDS_ARRAY (feeds_node)) {
		g_object_unref (parser);
		return NULL;
	}

	feeds = json_node_get_array (feeds_node);
	n = json_array_get_length (feeds);

	for (guint i = 0; i < n; i++) {
		JsonNode *en = json_array_get_element (feeds, i);
		JsonObject *entry;
		const gchar *id = NULL;
		const gchar *type = "feed";
		const gchar *parent_id = NULL;
		const gchar *title = NULL;
		const gchar *source = NULL;
		IndexEntry *e;

		if (!JSON_NODE_HOLDS_OBJECT (en))
			continue;

		entry = json_node_get_object (en);
		if (json_object_has_member (entry, "id"))
			id = json_object_get_string_member (entry, "id");
		if (!id)
			continue;
		if (json_object_has_member (entry, "type"))
			type = json_object_get_string_member (entry, "type");
		if (json_object_has_member (entry, "parent"))
			parent_id = json_object_get_string_member (entry, "parent");
		if (json_object_has_member (entry, "title"))
			title = json_object_get_string_member (entry, "title");
		if (json_object_has_member (entry, "source"))
			source = json_object_get_string_member (entry, "source");

		e = g_new0 (IndexEntry, 1);
		e->is_folder = type && g_str_equal (type, "folder");
		e->node_id = g_strdup (id);
		e->parent_id = parent_id ? g_strdup (parent_id) : NULL;
		e->title = title ? g_strdup (title) : NULL;
		e->source = source ? g_strdup (source) : NULL;
		e->feed_mtime = json_object_has_member (entry, "feed_mtime")
		                ? json_object_get_int_member (entry, "feed_mtime") : 0;
		e->state_mtime = json_object_has_member (entry, "state_mtime")
		                 ? json_object_get_int_member (entry, "state_mtime") : 0;
		result = g_list_prepend (result, e);
	}

	g_object_unref (parser);
	return g_list_reverse (result);
}

static GList *
webdav_read_index (Node *root)
{
	g_autofree gchar *url = webdav_index_url (root);
	guint http_status = 0;
	g_autofree gchar *json = webdav_get (root, url, 0, NULL, &http_status);

	if (!json) {
		if (http_status == 404) {
			debug (DEBUG_UPDATE, "webdav_read_index: index.json missing, creating empty index");
			webdav_put (root, url, "{\"nodes\":[]}", "application/json");
		}
		return NULL;
	}

	return webdav_parse_index_json (json);
}

static Node *
webdav_node_from_feed_json (const gchar *json_str, Node *parent, const gchar *remote_id)
{
	JsonParser  *parser;
	JsonNode    *root;
	const gchar *src_url = NULL;
	const gchar *title   = NULL;
	const gchar *node_id = NULL;
	Node        *node    = NULL;
	GError      *err     = NULL;

	parser = json_parser_new ();
	if (!json_parser_load_from_data (parser, json_str, -1, &err)) {
		debug (DEBUG_UPDATE, "webdav: failed to parse feed.json: %s", err->message);
		g_error_free (err);
		g_object_unref (parser);
		return NULL;
	}

	root = json_parser_get_root (parser);
	if (!JSON_NODE_HOLDS_OBJECT (root)) {
		g_object_unref (parser);
		return NULL;
	}

	JsonObject *obj      = json_node_get_object (root);
	JsonObject *node_obj = obj;

	if (json_object_has_member (obj, "node")) {
		JsonNode *nn = json_object_get_member (obj, "node");
		if (JSON_NODE_HOLDS_OBJECT (nn))
			node_obj = json_node_get_object (nn);
	}

	if (json_object_has_member (node_obj, "source"))
		src_url = json_object_get_string_member (node_obj, "source");
	if (json_object_has_member (node_obj, "title"))
		title   = json_object_get_string_member (node_obj, "title");
	if (json_object_has_member (node_obj, "id"))
		node_id = json_object_get_string_member (node_obj, "id");

	if (!src_url) {
		g_object_unref (parser);
		return NULL;
	}

	Node *source_root = node_source_root_from_node (parent);
	if (remote_id)
		node = webdav_find_feed_by_remote_id (source_root, remote_id);
	if (!node && node_id)
		node = node_is_used_id (node_id);
	if (!node)
		node = feedlist_find_node (source_root, NODE_BY_URL, src_url);
	if (!node) {
		node = node_new ("feed");
		node_set_subscription (node, subscription_new (src_url, NULL, NULL));
		if (title)
			node_set_title (node, title);
		node_set_parent (node, parent, -1);
		feedlist_node_imported (node);
		subscription_update (node->subscription,
		                     UPDATE_REQUEST_RESET_TITLE | UPDATE_REQUEST_PRIORITY_HIGH);
	} else if (node->parent != parent) {
		node_reparent (node, parent);
	}

	webdav_feed_set_remote_id (node, remote_id ? remote_id : node_id);

	g_object_unref (parser);
	return node;
}

static void
webdav_merge_feed (Node *root, const gchar *node_id,
	               gint64 remote_mtime, Node *target_parent)
{
	gint64  local_mtime = 0;
	gint64 *stored;
	gint64  actual_mtime = 0;
	g_autofree gchar *feed_url = NULL;
	g_autofree gchar *json = NULL;
	gint64 *ts;
	Node *local_node = webdav_find_feed_by_remote_id (root, node_id);

	if (local_node && webdav_is_feed_upload_pending (root, local_node->id)) {
		debug (DEBUG_UPDATE, "webdav_merge_feed: %s has pending upload, skipping", node_id);
		return;
	}

	GHashTable *feedMtimes = (GHashTable *)g_object_get_data (G_OBJECT (root), "feedMtimes");
	stored = g_hash_table_lookup (feedMtimes, node_id);
	if (stored)
		local_mtime = *stored;

	if (remote_mtime > 0 && remote_mtime <= local_mtime) {
		debug (DEBUG_UPDATE, "webdav_merge_feed: %s local same/newer, skipping", node_id);
		return;
	}

	feed_url = webdav_feed_json_url (root, node_id);
	json = webdav_get (root, feed_url, local_mtime, &actual_mtime, NULL);
	if (!json)
		return;

	if (!webdav_node_from_feed_json (json, target_parent, node_id))
		return;

	ts = g_new (gint64, 1);
	*ts = actual_mtime ? actual_mtime : remote_mtime;
	g_hash_table_insert (feedMtimes, g_strdup (node_id), ts);
}

static void
webdav_merge_state (Node *root, const gchar *node_id, gint64 remote_mtime)
{
	gint64  local_mtime = 0;
	gint64 *stored;
	gint64  actual_mtime = 0;
	g_autofree gchar *state_url = NULL;
	g_autofree gchar *json = NULL;
	Node *node;
	JsonParser *parser;
	JsonNode *jroot;
	JsonObject *obj;
	itemSetPtr itemset;
	GError *err = NULL;
	gint64 *ts;

	node = webdav_find_feed_by_remote_id (root, node_id);

	if (node && webdav_is_state_upload_pending (root, node->id)) {
		debug (DEBUG_UPDATE, "webdav_merge_state: %s has pending state upload, skipping", node_id);
		return;
	}

	GHashTable *stateMtimes = (GHashTable *)g_object_get_data (G_OBJECT (root), "stateMtimes");
	stored = g_hash_table_lookup (stateMtimes, node_id);
	if (stored)
		local_mtime = *stored;

	if (remote_mtime > 0 && remote_mtime <= local_mtime) {
		debug (DEBUG_UPDATE, "webdav_merge_state: %s local same/newer, skipping", node_id);
		return;
	}

	state_url = webdav_state_json_url (root, node_id);
	json = webdav_get (root, state_url, local_mtime, &actual_mtime, NULL);
	if (!json)
		return;

	if (!node)
		return;

	parser = json_parser_new ();
	if (!json_parser_load_from_data (parser, json, -1, &err)) {
		debug (DEBUG_UPDATE, "webdav_merge_state(%s): parse error: %s", node_id, err->message);
		g_error_free (err);
		g_object_unref (parser);
		return;
	}

	jroot = json_parser_get_root (parser);
	if (!JSON_NODE_HOLDS_OBJECT (jroot)) {
		g_object_unref (parser);
		return;
	}

	obj = json_node_get_object (jroot);
	itemset = node_get_itemset (node);
	if (itemset) {
		for (GList *iter = itemset->ids; iter; iter = g_list_next (iter)) {
			gulong item_id = GPOINTER_TO_UINT (iter->data);
			LifereaItem *item = item_load (item_id);
			if (!item)
				continue;

			const gchar *src_id = item_get_id (item);
			if (src_id && *src_id && json_object_has_member (obj, src_id)) {
				JsonObject *state = json_object_get_object_member (obj, src_id);
				if (state) {
					if (!item->readStatus &&
					    json_object_has_member (state, "read") &&
					    json_object_get_boolean_member (state, "read"))
						item_set_read_state (item, TRUE);

					if (!item->flagStatus &&
					    json_object_has_member (state, "flagged") &&
					    json_object_get_boolean_member (state, "flagged"))
						item_set_flag_state (item, TRUE);
				}
			}
			g_object_unref (item);
		}
		itemset_free (itemset);
	}

	g_object_unref (parser);

	ts = g_new (gint64, 1);
	*ts = actual_mtime ? actual_mtime : remote_mtime;
	g_hash_table_insert (stateMtimes, g_strdup (node_id), ts);
}

typedef struct {
	JsonBuilder     *builder;
	Node		*root;
} IndexBuildCtx;

static void
build_index_entry (IndexBuildCtx *ctx, Node *node, const gchar *parent_id)
{
	gint64  feed_mtime  = 0;
	gint64  state_mtime = 0;
	gint64 *fm;
	gint64 *sm;

	if (!IS_FOLDER (node) && !IS_FEED (node))
		return;

	json_builder_begin_object (ctx->builder);
	json_builder_set_member_name (ctx->builder, "type");
	json_builder_add_string_value (ctx->builder, IS_FOLDER (node) ? "folder" : "feed");
	json_builder_set_member_name (ctx->builder, "id");
	json_builder_add_string_value (ctx->builder, IS_FEED (node) ? webdav_feed_remote_id (node) : node->id);
	if (parent_id) {
		json_builder_set_member_name (ctx->builder, "parent");
		json_builder_add_string_value (ctx->builder, parent_id);
	}
	json_builder_set_member_name (ctx->builder, "title");
	json_builder_add_string_value (ctx->builder, node_get_title (node));

	if (IS_FEED (node) && node->subscription) {
		const gchar *remote_id = webdav_feed_remote_id (node);
		GHashTable *feedMtimes = (GHashTable *)g_object_get_data (G_OBJECT (ctx->root), "feedMtimes");
		GHashTable *stateMtimes = (GHashTable *)g_object_get_data (G_OBJECT (ctx->root), "stateMtimes");
		fm = g_hash_table_lookup (feedMtimes, remote_id);
		sm = g_hash_table_lookup (stateMtimes, remote_id);
		if (fm)
			feed_mtime = *fm;
		if (sm)
			state_mtime = *sm;

		json_builder_set_member_name (ctx->builder, "source");
		json_builder_add_string_value (ctx->builder, subscription_get_source (node->subscription));
		json_builder_set_member_name (ctx->builder, "feed_mtime");
		json_builder_add_int_value (ctx->builder, feed_mtime);
		json_builder_set_member_name (ctx->builder, "state_mtime");
		json_builder_add_int_value (ctx->builder, state_mtime);
	}
	json_builder_end_object (ctx->builder);

	if (IS_FOLDER (node)) {
		for (GSList *iter = node->children; iter; iter = g_slist_next (iter))
			build_index_entry (ctx, (Node *)iter->data, node->id);
	}
}

void
webdav_source_feed_list_upload (Node *root)
{
	g_assert (root->source->root == root);

	IndexBuildCtx ctx = { json_builder_new (), root };

	json_builder_begin_object (ctx.builder);
	json_builder_set_member_name (ctx.builder, "nodes");
	json_builder_begin_array (ctx.builder);
	for (GSList *iter = root->children; iter; iter = g_slist_next (iter))
		build_index_entry (&ctx, (Node *)iter->data, NULL);
	json_builder_end_array (ctx.builder);
	json_builder_end_object (ctx.builder);

	g_autofree gchar *json = json_dump (ctx.builder);
	g_object_unref (ctx.builder);

	if (json) {
		g_autofree gchar *url = webdav_index_url (root);
		webdav_put (root, url, json, "application/json");
	}
}

static Node *
webdav_resolve_folder_node (FolderResolveCtx *ctx, const gchar *folder_id)
{
	IndexEntry *entry;
	Node *folder;
	Node *parent = ctx->root;

	if (!folder_id)
		return ctx->root;

	folder = g_hash_table_lookup (ctx->ensured_folders, folder_id);
	if (folder)
		return folder;

	entry = g_hash_table_lookup (ctx->entries_by_id, folder_id);
	if (!entry || !entry->is_folder)
		return ctx->root;

	if (g_hash_table_contains (ctx->visiting, folder_id))
		return ctx->root;

	g_hash_table_insert (ctx->visiting, (gpointer)folder_id, GINT_TO_POINTER (1));

	if (entry->parent_id)
		parent = webdav_resolve_folder_node (ctx, entry->parent_id);

	folder = node_is_used_id (entry->node_id);
	if (folder && !IS_FOLDER (folder)) {
		debug (DEBUG_UPDATE, "webdav: cannot create folder %s, id already used by non-folder node", entry->node_id);
		g_hash_table_remove (ctx->visiting, folder_id);
		return parent;
	}

	if (!folder) {
		folder = node_new ("folder");
		node_set_id (folder, entry->node_id);
		node_set_title (folder, entry->title ? entry->title : entry->node_id);
		node_set_parent (folder, parent, -1);
		feedlist_node_imported (folder);
	} else {
		if (entry->title && *entry->title && g_strcmp0 (entry->title, node_get_title (folder)))
			node_set_title (folder, entry->title);
		if (folder->parent != parent)
			node_reparent (folder, parent);
	}

	g_hash_table_insert (ctx->ensured_folders, (gpointer)entry->node_id, folder);
	g_hash_table_remove (ctx->visiting, folder_id);

	return folder;
}

static void
webdav_cleanup_stale_folders (Node *node, gpointer user_data)
{
	FolderCleanupCtx *ctx = (FolderCleanupCtx *)user_data;

	if (!IS_FOLDER (node))
		return;

	node_foreach_child_data (node, webdav_cleanup_stale_folders, user_data);

	if (node == ctx->root)
		return;

	if (node->children)
		return;

	if (g_hash_table_contains (ctx->remote_folder_ids, node->id))
		return;

	debug (DEBUG_UPDATE, "webdav: removing stale local folder %s", node->id);
	feedlist_node_removed (node);
}

static void
webdav_merge_index (Node *root, GList *index)
{
	FolderResolveCtx resolve = {
		.root = root,
		.entries_by_id = g_hash_table_new (g_str_hash, g_str_equal),
		.ensured_folders = g_hash_table_new (g_str_hash, g_str_equal),
		.visiting = g_hash_table_new (g_str_hash, g_str_equal)
	};
	FolderCleanupCtx cleanup = {
		.root = root,
		.remote_folder_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL)
	};

	for (GList *l = index; l; l = g_list_next (l)) {
		IndexEntry *e = (IndexEntry *)l->data;
		if (!e->is_folder)
			continue;

		g_hash_table_insert (resolve.entries_by_id, e->node_id, e);
		g_hash_table_insert (cleanup.remote_folder_ids, g_strdup (e->node_id), GINT_TO_POINTER (1));
	}

	for (GList *l = index; l; l = g_list_next (l)) {
		IndexEntry *e = (IndexEntry *)l->data;
		if (e->is_folder)
			webdav_resolve_folder_node (&resolve, e->node_id);
	}

	for (GList *l = index; l; l = g_list_next (l)) {
		IndexEntry *e = (IndexEntry *)l->data;
		Node *target_parent;

		if (e->is_folder)
			continue;

		target_parent = webdav_resolve_folder_node (&resolve, e->parent_id);
		webdav_merge_feed (root, e->node_id, e->feed_mtime, target_parent);
		webdav_merge_state (root, e->node_id, e->state_mtime);
	}

	node_foreach_child_data (root, webdav_cleanup_stale_folders, &cleanup);

	g_hash_table_destroy (cleanup.remote_folder_ids);
	g_hash_table_destroy (resolve.visiting);
	g_hash_table_destroy (resolve.ensured_folders);
	g_hash_table_destroy (resolve.entries_by_id);
}

void
webdav_source_feed_list_import (Node *root)
{
	GList *index = webdav_read_index (root);
	MissingSyncCtx ctx = { 0 };

	ctx.root = root;
	ctx.remote_feed_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	ctx.remote_folder_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (GList *l = index; l; l = g_list_next (l)) {
		IndexEntry *e = (IndexEntry *)l->data;
		if (e->is_folder)
			g_hash_table_insert (ctx.remote_folder_ids, g_strdup (e->node_id), GINT_TO_POINTER (1));
		else
			g_hash_table_insert (ctx.remote_feed_ids, g_strdup (e->node_id), GINT_TO_POINTER (1));
	}

	webdav_upload_missing_remote_entries (root, &ctx);

	if (ctx.uploaded_missing)
		webdav_source_feed_list_upload (root);

	webdav_merge_index (root, index);

	g_list_free_full (index, (GDestroyNotify)index_entry_free);
	g_hash_table_destroy (ctx.remote_feed_ids);
	g_hash_table_destroy (ctx.remote_folder_ids);
}

gboolean
webdav_subscription_prepare_update_request (subscriptionPtr subscription, UpdateRequest *request)
{
	g_autofree gchar *index_url = NULL;

	if (subscription->node->source->loginState != NODE_SOURCE_STATE_ACTIVE) {
		if (!webdav_source_login (subscription->node, NODE_SOURCE_UPDATE_ONLY_LOGIN))
			return FALSE;
	}

	index_url = webdav_index_url (subscription->node);
	update_request_set_source (request, index_url);
	return TRUE;
}

void
webdav_subscription_process_update_result (subscriptionPtr subscription, const UpdateResult * const result, updateFlags flags)
{
	GList *index;

	if (!subscription || !subscription->node)
		return;

	debug (DEBUG_UPDATE, "webdav_subscription_process_update_result");

	if (!(result->data && result->httpstatus == 200)) {
		subscription->node->available = FALSE;
		if (result->httpstatus == 401) {
			node_source_set_state (subscription->node, NODE_SOURCE_STATE_NO_AUTH);
			auth_dialog_new (subscription, flags);
		}
		debug (DEBUG_UPDATE, "webdav_subscription_process_update_result(): failed to fetch index.json");
		return;
	}

	index = webdav_parse_index_json (result->data);
	if (!index) {
		subscription->node->available = FALSE;
		debug (DEBUG_UPDATE, "webdav_subscription_process_update_result(): invalid index.json");
		return;
	}

	webdav_merge_index (subscription->node, index);

	g_list_free_full (index, (GDestroyNotify)index_entry_free);

	/* Keep index.json in sync with freshly merged/uploaded timestamps. */
	webdav_source_feed_list_upload (subscription->node);

	subscription->node->available = TRUE;

	if (!(flags & NODE_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child (subscription->node, node_auto_update_subscription);
}

struct subscriptionType webdavSourceSubscriptionType = {
	webdav_subscription_prepare_update_request,
	webdav_subscription_process_update_result
};
