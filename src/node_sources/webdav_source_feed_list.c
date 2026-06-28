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
#include "net.h"
#include "webdav_source_flows.h"

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
	Node       *root;
	GHashTable *remote_folder_ids;
} FolderCleanupCtx;

typedef struct {
	Node       *root;
	GHashTable *entries_by_id;
	GHashTable *ensured_folders;
	GHashTable *visiting;
} FolderResolveCtx;

typedef struct {
	Node       *root;
	updateFlags flags;
	gint        pending_requests;
	gboolean    any_error;
	GHashTable *entries_by_id;
	GHashTable *ensured_folders;
	GHashTable *visiting;
	GHashTable *remote_folder_ids;
} WebDAVMergeOp;

typedef struct {
	WebDAVMergeOp *op;
	gchar         *feed_id;
	gchar         *parent_id;
	gint64         remote_mtime;
} WebDAVFeedFetchCtx;

typedef struct {
	WebDAVMergeOp *op;
	gchar         *feed_id;
	gint64         remote_mtime;
} WebDAVStateFetchCtx;

typedef struct {
	Node       *root;
	GHashTable *remote_feed_ids;
} InitialImportUploadCtx;

static void webdav_request_put_index (Node *root, const gchar *json, gpointer callback_data);

static void
index_entry_free (IndexEntry *e)
{
	g_free (e->source);
	g_free (e->title);
	g_free (e->parent_id);
	g_free (e->node_id);
	g_free (e);
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

	if (json)
		webdav_request_put_index (root, json, NULL);
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

	node_foreach_child_data (root, webdav_cleanup_stale_folders, &cleanup);

	g_hash_table_destroy (cleanup.remote_folder_ids);
	g_hash_table_destroy (resolve.visiting);
	g_hash_table_destroy (resolve.ensured_folders);
	g_hash_table_destroy (resolve.entries_by_id);
}

static void
webdav_queue_initial_import_upload (Node *node, gpointer user_data)
{
	if (!IS_FEED (node))
		return;

	if (!(node->syncState & NODE_SYNC_STATE_INITIAL_IMPORT))
		return;

	webdav_source_mark_feed_dirty (node);
	webdav_source_mark_items_dirty (node);
}

static void
webdav_request_get_feed_with_callback (Node *root, const gchar *feed_id, update_flow_cb callback, gpointer callback_data)
{
	UpdateRequest *request;
	g_autofree gchar *url = NULL;

	url = webdav_feed_json_url (root, feed_id);
	request = update_request_new ("GET", url, NULL, NULL);
	webdav_request_set_basic_auth (request, root);

	(void)update_job_new (root, request, callback, callback_data, 0);
}

static void
webdav_request_get_state_with_callback (Node *root, const gchar *feed_id, update_flow_cb callback, gpointer callback_data)
{
	UpdateRequest *request;
	g_autofree gchar *url = NULL;

	url = webdav_state_json_url (root, feed_id);
	request = update_request_new ("GET", url, NULL, NULL);
	webdav_request_set_basic_auth (request, root);

	(void)update_job_new (root, request, callback, callback_data, 0);
}

static Node *
webdav_merge_op_resolve_parent (WebDAVMergeOp *op, const gchar *parent_id)
{
	FolderResolveCtx resolve = {
		.root = op->root,
		.entries_by_id = op->entries_by_id,
		.ensured_folders = op->ensured_folders,
		.visiting = op->visiting
	};

	return webdav_resolve_folder_node (&resolve, parent_id);
}

static void
webdav_merge_op_finalize (WebDAVMergeOp *op)
{
	FolderCleanupCtx cleanup = {
		.root = op->root,
		.remote_folder_ids = op->remote_folder_ids
	};

	node_foreach_child_data (op->root, webdav_cleanup_stale_folders, &cleanup);

	if (!op->any_error)
		webdav_source_feed_list_upload (op->root);

	op->root->available = !op->any_error;

	if (!op->any_error && !(op->flags & NODE_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child (op->root, node_auto_update_subscription);

	g_hash_table_destroy (op->remote_folder_ids);
	g_hash_table_destroy (op->visiting);
	g_hash_table_destroy (op->ensured_folders);
	g_hash_table_destroy (op->entries_by_id);
	g_free (op);
}

static void
webdav_merge_op_complete_one (WebDAVMergeOp *op)
{
	op->pending_requests--;
	if (op->pending_requests <= 0)
		webdav_merge_op_finalize (op);
}

static gboolean
webdav_async_merge_feed_result (UpdateJob *job)
{
	UpdateResult *result = job->result;
	gpointer user_data = job->user_data;
	WebDAVFeedFetchCtx *ctx = (WebDAVFeedFetchCtx *)user_data;
	WebDAVMergeOp *op = ctx ? ctx->op : NULL;

	if (!ctx || !op)
		return TRUE;

	if (result->httpstatus == 200 && result->data && result->size > 0) {
		Node *target_parent = webdav_merge_op_resolve_parent (op, ctx->parent_id);
		if (webdav_node_from_feed_json (result->data, target_parent, ctx->feed_id)) {
			gint64 *ts = g_new (gint64, 1);
			*ts = ctx->remote_mtime;
			g_hash_table_insert (
				(GHashTable *)g_object_get_data (G_OBJECT (op->root), "feedMtimes"),
				g_strdup (ctx->feed_id),
				ts
			);
		} else {
			op->any_error = TRUE;
		}
	} else if (result->httpstatus != 304 && result->httpstatus != 404) {
		op->any_error = TRUE;
	}

	webdav_merge_op_complete_one (op);
	g_free (ctx->feed_id);
	g_free (ctx->parent_id);
	g_free (ctx);

	return TRUE;
}

static gboolean
webdav_async_merge_state_result (UpdateJob *job)
{
	UpdateResult *result = job->result;
	gpointer user_data = job->user_data;
	WebDAVStateFetchCtx *ctx = (WebDAVStateFetchCtx *)user_data;
	WebDAVMergeOp *op = ctx ? ctx->op : NULL;

	if (!ctx || !op)
		return TRUE;

	if (result->httpstatus == 200 && result->data && result->size > 0) {
		Node *node = webdav_find_feed_by_remote_id (op->root, ctx->feed_id);
		if (node) {
			JsonParser *parser;
			JsonNode *jroot;
			JsonObject *obj;
			itemSetPtr itemset;
			GError *err = NULL;

			parser = json_parser_new ();
			if (json_parser_load_from_data (parser, result->data, -1, &err)) {
				jroot = json_parser_get_root (parser);
				if (JSON_NODE_HOLDS_OBJECT (jroot)) {
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

					gint64 *ts = g_new (gint64, 1);
					*ts = ctx->remote_mtime;
					g_hash_table_insert (
						(GHashTable *)g_object_get_data (G_OBJECT (op->root), "stateMtimes"),
						g_strdup (ctx->feed_id),
						ts
					);
				} else {
					op->any_error = TRUE;
				}
			} else {
				debug (DEBUG_UPDATE, "webdav_async_merge_state_result(%s): parse error: %s", ctx->feed_id, err->message);
				g_error_free (err);
				op->any_error = TRUE;
			}
			g_object_unref (parser);
		}
	} else if (result->httpstatus != 304 && result->httpstatus != 404) {
		op->any_error = TRUE;
	}

	webdav_merge_op_complete_one (op);
	g_free (ctx->feed_id);
	g_free (ctx);

	return TRUE;
}

static gboolean
webdav_update_result_wrapper (UpdateJob *job)
{
	UpdateResult *result = job->result;
	gpointer user_data = job->user_data;
	subscriptionPtr subscription = (subscriptionPtr)user_data;
	if (!subscription)
		return TRUE;

	/* Call the subscription's process_update_result callback */
	subscription->type->process_update_result (subscription, result, job->flags);

	return TRUE;
}

/**
 * Bootstrap index fetch callback.
 * Parse index and import data then transition to ACTIVE.
 */
static void
webdav_source_feed_list_index_fetch_cb (Node *root, const gchar *jsonStr)
{
	if (jsonStr) {
		GList *index = webdav_parse_index_json (jsonStr);
		if (index) {
			webdav_merge_index (root, index);
			/* GList is cleaned up by webdav_merge_index */
		} else {
			debug (DEBUG_UPDATE, "webdav_bootstrap: failed to parse index.json");
		}
	} else {
		/* even if we got no "index.json" from remote the subscription_update()
		   below will trigger an initial sync */
	}
	
	node_source_set_state (root, NODE_SOURCE_STATE_ACTIVE);
	subscription_update (root->subscription, 0);
}

void
webdav_source_feed_list_import (Node *root)
{
	webdav_source_flow_bootstrap_index (root, webdav_source_feed_list_index_fetch_cb);
}

void
webdav_source_feed_list_update_feed_mtime (Node *root, const gchar *remote_id)
{
	gint64 now = (gint64)(g_get_real_time () / G_USEC_PER_SEC);
	gint64 *ts = g_new (gint64, 1);
	*ts = now;
	g_hash_table_insert (
		(GHashTable *)g_object_get_data (G_OBJECT (root), "feedMtimes"),
		g_strdup (remote_id),
		ts
	);
}

void
webdav_source_feed_list_update_state_mtime (Node *root, const gchar *remote_id)
{
	gint64 now = (gint64)(g_get_real_time () / G_USEC_PER_SEC);
	gint64 *ts = g_new (gint64, 1);
	*ts = now;
	g_hash_table_insert (
		(GHashTable *)g_object_get_data (G_OBJECT (root), "stateMtimes"),
		g_strdup (remote_id),
		ts
	);
}

static void
webdav_request_put_index (Node *root, const gchar *json, gpointer callback_data)
{
	UpdateRequest *request;
	g_autofree gchar *url = NULL;

	url = webdav_index_url (root);
	request = update_request_new ("PUT", url, NULL, NULL);
	update_request_set_postdata (request, json, "application/json");
	webdav_request_set_basic_auth (request, root);

	debug (DEBUG_UPDATE, "webdav_request_put_index: queued PUT %s (%zu bytes)", url, json ? strlen (json) : 0);
	(void)update_job_new (root, request, webdav_update_result_wrapper, callback_data, 0);
}

gboolean
webdav_subscription_prepare_update_request (subscriptionPtr subscription, UpdateRequest *request)
{
	g_autofree gchar *index_url = NULL;

	// FIXME: is this necessary? Isn't this handled by node_source.c
	/* Only prepare request if login is already active; otherwise skip this round */
	if (subscription->node->source->loginState != NODE_SOURCE_STATE_ACTIVE) {
		debug (DEBUG_UPDATE, "webdav_subscription_prepare_update_request: login not active, skipping");
		return FALSE;
	}

	index_url = webdav_index_url (subscription->node);
	update_request_set_source (request, index_url);
	debug (DEBUG_UPDATE, "webdav_subscription_prepare_update_request: queued index fetch from %s", index_url);
	return TRUE;
}

void
webdav_subscription_process_update_result (subscriptionPtr subscription, const UpdateResult * const result, updateFlags flags)
{
	GList *index;
	WebDAVMergeOp *op;

	if (!subscription || !subscription->node)
		return;

	debug (DEBUG_UPDATE, "webdav_subscription_process_update_result");

	if (!(result->data && result->httpstatus == 200)) {
		subscription->node->available = FALSE;
		if (result->httpstatus == 401) {
			node_source_set_state (subscription->node, NODE_SOURCE_STATE_NO_AUTH);
			auth_dialog_new (subscription, flags);
		}
		debug (DEBUG_UPDATE, "webdav_subscription_process_update_result(): failed to fetch index.json (HTTP %d)", result->httpstatus);
		return;
	}

	index = webdav_parse_index_json (result->data);
	if (!index) {
		subscription->node->available = FALSE;
		debug (DEBUG_UPDATE, "webdav_subscription_process_update_result(): invalid index.json");
		return;
	}

	op = g_new0 (WebDAVMergeOp, 1);
	op->root = subscription->node;
	op->flags = flags;
	op->entries_by_id = g_hash_table_new (g_str_hash, g_str_equal);
	op->ensured_folders = g_hash_table_new (g_str_hash, g_str_equal);
	op->visiting = g_hash_table_new (g_str_hash, g_str_equal);
	op->remote_folder_ids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

	for (GList *l = index; l; l = g_list_next (l)) {
		IndexEntry *e = (IndexEntry *)l->data;
		if (!e->is_folder)
			continue;

		g_hash_table_insert (op->entries_by_id, e->node_id, e);
		g_hash_table_insert (op->remote_folder_ids, g_strdup (e->node_id), GINT_TO_POINTER (1));
	}

	for (GList *l = index; l; l = g_list_next (l)) {
		IndexEntry *e = (IndexEntry *)l->data;
		if (e->is_folder)
			webdav_merge_op_resolve_parent (op, e->node_id);
	}

	for (GList *l = index; l; l = g_list_next (l)) {
		IndexEntry *e = (IndexEntry *)l->data;
		gint64 local_mtime = 0;
		gint64 *stored;

		if (e->is_folder)
			continue;

		Node *local_node = webdav_find_feed_by_remote_id (op->root, e->node_id);

		if (local_node && (local_node->syncState & NODE_SYNC_STATE_INITIAL_IMPORT))
			local_node->syncState &= ~NODE_SYNC_STATE_INITIAL_IMPORT;

		if (!local_node || !webdav_is_feed_upload_pending (op->root, local_node->id)) {
			GHashTable *feed_mtimes = (GHashTable *)g_object_get_data (G_OBJECT (op->root), "feedMtimes");
			stored = g_hash_table_lookup (feed_mtimes, e->node_id);
			if (stored)
				local_mtime = *stored;

			if (e->feed_mtime <= 0 || e->feed_mtime > local_mtime) {
				WebDAVFeedFetchCtx *feed_ctx = g_new0 (WebDAVFeedFetchCtx, 1);
				feed_ctx->op = op;
				feed_ctx->feed_id = g_strdup (e->node_id);
				feed_ctx->parent_id = g_strdup (e->parent_id);
				feed_ctx->remote_mtime = e->feed_mtime;

				op->pending_requests++;
				webdav_request_get_feed_with_callback (op->root, e->node_id, webdav_async_merge_feed_result, feed_ctx);
			}
		}

		if (!local_node || !webdav_is_state_upload_pending (op->root, local_node->id)) {
			GHashTable *state_mtimes = (GHashTable *)g_object_get_data (G_OBJECT (op->root), "stateMtimes");
			local_mtime = 0;
			stored = g_hash_table_lookup (state_mtimes, e->node_id);
			if (stored)
				local_mtime = *stored;

			if (e->state_mtime <= 0 || e->state_mtime > local_mtime) {
				WebDAVStateFetchCtx *state_ctx = g_new0 (WebDAVStateFetchCtx, 1);
				state_ctx->op = op;
				state_ctx->feed_id = g_strdup (e->node_id);
				state_ctx->remote_mtime = e->state_mtime;

				op->pending_requests++;
				webdav_request_get_state_with_callback (op->root, e->node_id, webdav_async_merge_state_result, state_ctx);
			}
		}
	}

	node_foreach_child (op->root, webdav_queue_initial_import_upload);

	g_list_free_full (index, (GDestroyNotify)index_entry_free);

	if (op->pending_requests == 0)
		webdav_merge_op_finalize (op);
}

struct subscriptionType webdavSourceSubscriptionType = {
	webdav_subscription_prepare_update_request,
	webdav_subscription_process_update_result
};
