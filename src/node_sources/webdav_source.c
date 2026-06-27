/**
 * @file webdav_source.c  WebDAV-based feed list sync source
 *
 * Copyright (C) 2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_sources/webdav_source.h"

#include <glib/gstdio.h>
#include <libsoup/soup.h>
#include <json-glib/json-glib.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "export.h"
#include "feedlist.h"
#include "item.h"
#include "item_state.h"
#include "itemset.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "node_provider.h"
#include "node_source.h"
#include "node_sources/opml_source.h"
#include "node_providers/feed.h"
#include "subscription.h"
#include "update.h"
#include "ui/auth_dialog.h"
#include "ui/liferea_dialog.h"
#include "node_sources/webdav_source_feed_list.h"
#include "node_sources/webdav_source_flows.h"

/*
 * Syncs a Liferea feed list to a "Liferea Sync" collection on a WebDAV
 * server. Expected layout on the server:
 *
 *   <base_url>/Liferea Sync/
 *     index.json              -- compact polling index with per-feed timestamps
 *     <node_id>/
 *       node.json             -- node metadata + embedded items array
 *       state.json            -- per-item read/flagged state
 *
 * Merge strategy:
 *   node.json  : newest file wins (updated on the daily feed-fetch cadence)
 *   state.json : union merge — read/flagged bits are sticky, never cleared by sync
 *
 * Lazy sync:
 *   Item state changes   -> state.json after WEBDAV_STATE_SYNC_DELAY_S  (5 s)
 *   Feed content changes -> node.json  after WEBDAV_LAZY_SYNC_DELAY_S   (30 s)
 *
 * Change detection uses GET index.json (O(1) round-trips per poll cycle)
 * instead of PROPFIND depth-1, which avoids per-feed round-trips.
 *
 * Request flows
 * =============
 *
 * 1. initial import:
 *    - webdav_source_login()
 *    - webdav_request_mkcol_bootstrap()           -> MKCOL {collection_url}
 *    - webdav_bootstrap_mkcol_result()
 *    - webdav_request_get_index_bootstrap()       -> GET {collection_url}/index.json
 *      - webdav_request_get_feed_with_callback()
 *      - webdav_request_get_state_with_callback()
 *
 * 2. user adds new feed:
 *    - webdav_flush_feed_cb() 
 *    - webdav_async_upload_feed()          -> MKCOL {node_id_dir}
 *    - webdav_async_upload_mkcol_result()  -> PUT {collection_url}/{node_id}/node.json
 *    - webdav_async_upload_feed_result()   -> PUT {collection_url}/index.json
 *
 * 3. user removes a feed:
 *    - webdav_source_feed_list_upload()    -> PUT {collection_url}/index.json
 *    - Remote entries not in index are NOT explicitly deleted
 *      (lazy cleanup: server can periodically remove orphaned {node_id}/ folders)
 *
 * 4. user adds/removes a folder:
 *    - During next sync, webdav_async_upload_feed() ensures folder exists -> MKCOL {node_id_dir}
 *    - Deleted folders cleaned by webdav_cleanup_stale_folders()
 *
 * 5. feed updates:
 *    - webdav_subscription_prepare_update_request()  -> GET {collection_url}/index.json
 *    - webdav_subscription_process_update_result()   -> GET {collection_url}/{node_id}/node.json (multiple)
 *    - webdav_async_merge_feed_result()
 *
 * 6. user marks item read/flagged:
 *    - item_state_cb()
 *    - webdav_item_state_changed()
 *    - webdav_flush_state_cb()
 *    - webdav_async_upload_feed()         -> MKCOL {node_id_dir}
 *    - webdav_async_upload_mkcol_result() -> PUT {node_id_dir}/state.json
 */

/* ------------------------------------------------------------------ */
/*  Per-node lazy-upload timers                                         */
/* ------------------------------------------------------------------ */

typedef struct {
	guint feed_timer_id;    /**< pending node.json upload timer (0 = none) */
	guint state_timer_id;   /**< pending state.json upload timer (0 = none) */
} DirtyEntry;

static DirtyEntry *
dirty_entry_get_or_create (Node *root, const gchar *node_id)
{
	GHashTable *dirtyFeeds = (GHashTable *)g_object_get_data (G_OBJECT (root), "dirtyFeeds");

	DirtyEntry *de = g_hash_table_lookup (dirtyFeeds, node_id);
	if (!de) {
		de = g_new0 (DirtyEntry, 1);
		g_hash_table_insert (dirtyFeeds, g_strdup (node_id), de);
	}
	return de;
}

const gchar *
webdav_feed_remote_id (Node *node)
{
	const gchar *remote_id;

	if (!node)
		return NULL;

	if (!node->subscription)
		return node->id;

	remote_id = metadata_list_get (node->subscription->metadata, WEBDAV_REMOTE_FEED_ID_METADATA);
	if (remote_id && *remote_id)
		return remote_id;

	return node->id;
}

void
webdav_feed_set_remote_id (Node *node, const gchar *remote_id)
{
	const gchar *current;

	if (!node || !node->subscription || !remote_id || !*remote_id)
		return;

	current = metadata_list_get (node->subscription->metadata, WEBDAV_REMOTE_FEED_ID_METADATA);
	if (g_strcmp0 (current, remote_id) == 0)
		return;

	metadata_list_set (&node->subscription->metadata, WEBDAV_REMOTE_FEED_ID_METADATA, remote_id);
}

void
webdav_request_set_basic_auth (UpdateRequest *request, Node *root)
{
	if (!root || !root->subscription || !root->subscription->updateOptions)
		return;

	const gchar *username = root->subscription->updateOptions->username;
	const gchar *password = root->subscription->updateOptions->password;
	if (!username || !*username)
		return;

	g_autofree gchar *credentials = g_strdup_printf ("%s:%s", username, password ? password : "");
	g_autofree gchar *encoded = g_base64_encode ((const guchar *)credentials, strlen (credentials));
	request->authValue = g_strdup_printf ("Basic %s", encoded);
}

gboolean
webdav_is_feed_upload_pending (Node *root, const gchar *node_id)
{
	DirtyEntry *de;

	if (!node_id)
		return FALSE;

	de = g_hash_table_lookup (
		(GHashTable *)g_object_get_data (G_OBJECT (root), "dirtyFeeds"),
		node_id);
	return de && de->feed_timer_id;
}

// FIXME: why is this the same as webdav_is_feed_upload_pending()?
gboolean
webdav_is_state_upload_pending (Node *root, const gchar *node_id)
{
	DirtyEntry *de;

	if (!node_id)
		return FALSE;

	de = g_hash_table_lookup (
		(GHashTable *)g_object_get_data (G_OBJECT (root), "dirtyFeeds"),
		node_id);
	return de && de->state_timer_id;
}

/* ------------------------------------------------------------------ */
/*  URL helpers                                                         */
/* ------------------------------------------------------------------ */

gchar *
webdav_feed_dir_url (Node *root, const gchar *node_id)
{
	return g_strdup_printf (
		"%s/%s/",
		(const gchar *)g_object_get_data (G_OBJECT (root), "collectionUrl"),
		node_id
	);
}

gchar *
webdav_feed_json_url (Node *root, const gchar *node_id)
{
	return g_strdup_printf (
		"%s/%s/node.json",
		(const gchar *)g_object_get_data (G_OBJECT (root), "collectionUrl"),
		node_id
	);
}

gchar *
webdav_state_json_url (Node *root, const gchar *node_id)
{
	return g_strdup_printf (
		"%s/%s/state.json",
		(const gchar *)g_object_get_data (G_OBJECT (root), "collectionUrl"),
		node_id
	);
}

gchar *
webdav_index_url (Node *root)
{
	return g_strdup_printf (
		"%s/index.json",
		(const gchar *)g_object_get_data (G_OBJECT (root), "collectionUrl")
	);
}

gboolean
webdav_source_login (Node *root, guint32 flags)
{
	if (root->source->loginState == NODE_SOURCE_STATE_ACTIVE)
		return TRUE;

	if (root->source->loginState == NODE_SOURCE_STATE_IN_PROGRESS)
		return FALSE;

	node_source_set_state (root, NODE_SOURCE_STATE_IN_PROGRESS);

	webdav_source_feed_list_import (root);

	return TRUE;
}

/*  Lazy sync flushing */

typedef struct {
	Node	*root;
	gchar	*node_id;
} FlushCtx;

static gboolean
webdav_flush_feed_cb (gpointer user_data)
{
	FlushCtx   *ctx = (FlushCtx *)user_data;
	GHashTable *dirtyFeeds = (GHashTable *)g_object_get_data (G_OBJECT (ctx->root), "dirtyFeeds");
	DirtyEntry *de  = g_hash_table_lookup (dirtyFeeds, ctx->node_id);
	if (de)
		de->feed_timer_id = 0;

	debug (DEBUG_UPDATE, "webdav_flush_feed_cb: queuing async upload for %s", ctx->node_id);
	webdav_source_flow_upload_feed (ctx->root, ctx->node_id, FALSE, TRUE);

	g_free (ctx->node_id);
	g_free (ctx);

	return G_SOURCE_REMOVE;
}

static gboolean
webdav_flush_state_cb (gpointer user_data)
{
	FlushCtx   *ctx = (FlushCtx *)user_data;
	GHashTable *dirtyFeeds = (GHashTable *)g_object_get_data (G_OBJECT (ctx->root), "dirtyFeeds");
	DirtyEntry *de  = g_hash_table_lookup (dirtyFeeds, ctx->node_id);
	if (de)
		de->state_timer_id = 0;

	debug (DEBUG_UPDATE, "webdav_flush_state_cb: queuing async state upload for %s", ctx->node_id);
	webdav_source_flow_upload_feed (ctx->root, ctx->node_id, TRUE, FALSE);

	g_free (ctx->node_id);
	g_free (ctx);

	return G_SOURCE_REMOVE;
}

static void
webdav_mark_dirty_feed (Node *root, const gchar *node_id)
{
	DirtyEntry *de = dirty_entry_get_or_create (root, node_id);

	if (de->feed_timer_id)
		g_source_remove (de->feed_timer_id);

	FlushCtx *ctx     = g_new0 (FlushCtx, 1);
	ctx->root         = root;
	ctx->node_id      = g_strdup (node_id);
	de->feed_timer_id = g_timeout_add_seconds (WEBDAV_LAZY_SYNC_DELAY_S,
	                                            webdav_flush_feed_cb, ctx);

	debug (DEBUG_UPDATE, "webdav: node %s feed dirty (%ds)", node_id, WEBDAV_LAZY_SYNC_DELAY_S);
}

static void
webdav_mark_dirty_state (Node *root, const gchar *node_id)
{
	DirtyEntry *de = dirty_entry_get_or_create (root, node_id);

	if (de->state_timer_id)
		g_source_remove (de->state_timer_id);

	FlushCtx *ctx     = g_new0 (FlushCtx, 1);
	ctx->root         = root;
	ctx->node_id      = g_strdup (node_id);
	de->state_timer_id = g_timeout_add_seconds (WEBDAV_STATE_SYNC_DELAY_S,
	                                             webdav_flush_state_cb, ctx);

	debug (DEBUG_UPDATE, "webdav: node %s state dirty (%ds)", node_id, WEBDAV_STATE_SYNC_DELAY_S);
}

// FIXME: also implement set_flag and set_read to mark dirty and delay the upload instead of uploading immediately
/* ------------------------------------------------------------------ */
/*  feedlist "node-updated" signal handler                              */
/* ------------------------------------------------------------------ */

extern FeedList *feedlist;

static void
webdav_on_node_updated (FeedList *fl, const gchar *node_id, gpointer user_data)
{
	Node *node = node_from_id (node_id);
	Node *root = (Node *)user_data;

	if (!node || root != node_source_root_from_node (node))
		return;

	if (IS_FOLDER (node)) {
		/* Folders and root only affect hierarchy and must be persisted via index.json only. */
		webdav_source_feed_list_upload (root);
		return;
	}

	if (!IS_FEED (node))
		return;

	/*
	 * State changes (item read/flagged) are frequent: upload state.json
	 * quickly.  Structural feed changes are rare: defer node.json upload.
	 */
	webdav_mark_dirty_state (root, node_id);
	webdav_mark_dirty_feed  (root, node_id);
}

/*  nodeSourceType */

static void
webdav_source_new (Node *root)
{
	const gchar *base_url = subscription_get_source (root->subscription);
	g_autofree gchar *escaped_collection = NULL;
	if (!base_url || !*base_url) {
		root->available = FALSE;
		return;
	}

	gchar *trimmed = g_strdup (base_url);
	while (strlen (trimmed) > 1 && trimmed[strlen (trimmed) - 1] == '/')
		trimmed[strlen (trimmed) - 1] = '\0';
	escaped_collection = g_uri_escape_string (WEBDAV_SYNC_COLLECTION, NULL, FALSE);
	gchar *collection_url = g_strdup_printf ("%s/%s", trimmed, escaped_collection);
	g_free (trimmed);

	g_object_set_data_full (G_OBJECT (root), "collectionUrl", collection_url, g_free);
	g_object_set_data_full (G_OBJECT (root), "dirtyFeeds",
				g_hash_table_new_full (g_str_hash, g_str_equal,
	                                               g_free, g_free),
				(GDestroyNotify)g_hash_table_destroy);
	g_object_set_data_full (G_OBJECT (root), "feedMtimes",
				g_hash_table_new_full (g_str_hash, g_str_equal,
	                                               g_free, g_free),
				(GDestroyNotify)g_hash_table_destroy);
	g_object_set_data_full (G_OBJECT (root), "stateMtimes",
				g_hash_table_new_full (g_str_hash, g_str_equal,
	                                               g_free, g_free),
				(GDestroyNotify)g_hash_table_destroy);

	debug (DEBUG_UPDATE, "webdav_source_import: collection URL = %s", collection_url);

	g_signal_connect (feedlist, "node-updated", G_CALLBACK (webdav_on_node_updated), root);
}

static void
webdav_source_auto_update (Node *node)
{
	if (!webdav_source_login (node, 0))
		return;

	guint64 now = g_get_real_time ();

	if (node->subscription->updateState->lastPoll +
	    (guint64)WEBDAV_SOURCE_UPDATE_INTERVAL * G_USEC_PER_SEC > now) {
		node_foreach_child (node, node_auto_update_subscription);
		return;
	}

	node->subscription->updateState->lastPoll = now;

	subscription_update (node->subscription, 0);
}

static void
webdav_source_free (Node *root)
{
	GHashTable *dirtyFeeds = (GHashTable *)g_object_get_data (G_OBJECT (root), "dirtyFeeds");

	g_signal_handlers_disconnect_by_data (feedlist, root);

	if (dirtyFeeds) {
		/* Cancel all pending timers */
		GHashTableIter iter;
		gpointer key, val;
		g_hash_table_iter_init (&iter, dirtyFeeds);
		while (g_hash_table_iter_next (&iter, &key, &val)) {
			DirtyEntry *de = (DirtyEntry *)val;
			if (de->feed_timer_id)
				g_source_remove (de->feed_timer_id);
			if (de->state_timer_id)
				g_source_remove (de->state_timer_id);
		}
	}
}

static Node *
webdav_source_add_subscription (Node *node, subscriptionPtr subscription)
{
	Node *child = node_new ("feed");
	node_set_title (child, _("New Subscription"));
	node_set_subscription (child, subscription);
	feedlist_node_added (child);
	subscription_update (subscription,
	                     UPDATE_REQUEST_RESET_TITLE | UPDATE_REQUEST_PRIORITY_HIGH);

	/* Ensure payload files get uploaded; index upload is done after flush/update. */
	webdav_mark_dirty_feed  (node->source->root, child->id);
	webdav_mark_dirty_state (node->source->root, child->id);

	webdav_source_feed_list_upload (node->source->root);

	return child;
}

static void
webdav_source_item_read_changed (Node *node, itemPtr item, gboolean newStatus)
{
	item_read_state_changed (item, newStatus);
	webdav_mark_dirty_state (node->source->root, node->id);
}

static void
webdav_source_item_flag_changed (Node *node, itemPtr item, gboolean newStatus)
{
	item_flag_state_changed (item, newStatus);
	webdav_mark_dirty_state (node->source->root, node->id);
}

static Node *
webdav_source_add_folder (Node *node, const gchar *title)
{
	Node *child = node_new ("folder");
	node_set_title (child, title);
	feedlist_node_added (child);

	webdav_source_feed_list_upload (node->source->root);

	return child;
}

static void
webdav_source_remove_node (Node *node, Node *child)
{
	feedlist_node_removed (child);
}

static void
on_webdav_source_apply  (GtkButton *btn, gpointer user_data)
{
        GtkWidget *dialog = GTK_WIDGET (user_data);
        const gchar *username = liferea_dialog_entryrow_get (dialog, "usernameEntry");
        const gchar *password = liferea_dialog_entryrow_get (dialog, "passwordEntry");
        const gchar *url = liferea_dialog_entryrow_get (dialog, "sourceEntry");

        g_assert (username);
        g_assert (password);
        g_assert (url);

        Node *node = node_new ("source");
        node_source_new (node, webdav_source_get_type (), url);
        node_set_title (node, node->source->type->name);
        subscription_set_auth_info (node->subscription, username, password);

        feedlist_node_added (node);
        node_source_update (node);

        adw_dialog_close (ADW_DIALOG (dialog));
}

static void
ui_webdav_source_get_url (void)
{
	GtkWidget *dialog = liferea_dialog_new ("webdav_source");
	g_signal_connect (liferea_dialog_lookup (dialog, "applyBtn"), "clicked",
			  G_CALLBACK (on_webdav_source_apply), dialog);
	g_signal_connect_swapped (liferea_dialog_lookup (dialog, "cancelBtn"), "clicked",
			  G_CALLBACK (adw_dialog_close), dialog);
}

static struct nodeSourceType nst = {
	.id                  = "fl_webdav",
	.name                = N_("WebDAV Sync"),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
	                       NODE_SOURCE_CAPABILITY_CAN_LOGIN |
	                       NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
	                       NODE_SOURCE_CAPABILITY_ADD_FEED |
	                       NODE_SOURCE_CAPABILITY_ADD_FOLDER |
	                       NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST,
	.sourceSubscriptionType = &webdavSourceSubscriptionType,
	.source_new	     = webdav_source_new,
	.source_create       = ui_webdav_source_get_url,
	.source_delete       = opml_source_remove,
	.source_auto_update  = webdav_source_auto_update,
	.source_free         = webdav_source_free,
	.item_set_flag       = webdav_source_item_flag_changed,
	.item_mark_read      = webdav_source_item_read_changed,
	.add_folder          = webdav_source_add_folder,
	.add_subscription    = webdav_source_add_subscription,
	.remove_node         = webdav_source_remove_node,
	.convert_to_local    = NULL
};

nodeSourceTypePtr
webdav_source_get_type (void)
{
	nst.feedSubscriptionType = feed_get_subscription_type ();
	return &nst;
}
