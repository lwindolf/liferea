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

/* ------------------------------------------------------------------ */
/*  Module-level libsoup session                                        */
/* ------------------------------------------------------------------ */

static SoupSession *webdav_soup_session = NULL;

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

static SoupSession *
webdav_get_session (void)
{
	if (!webdav_soup_session)
		webdav_soup_session = soup_session_new ();
	return webdav_soup_session;
}

static void
webdav_add_auth_header (Node *root, SoupMessage *msg)
{
	const gchar *username;
	const gchar *password;
	g_autofree gchar *credentials = NULL;
	g_autofree gchar *encoded = NULL;
	g_autofree gchar *auth = NULL;

	if (!msg)
		return;

	if (!root->subscription->updateOptions)
		return;

	username = root->subscription->updateOptions->username;
	password = root->subscription->updateOptions->password;
	if (!username || !*username)
		return;

	credentials = g_strdup_printf ("%s:%s", username, password ? password : "");
	encoded = g_base64_encode ((const guchar *)credentials, strlen (credentials));
	auth = g_strdup_printf ("Basic %s", encoded);

	soup_message_headers_replace (soup_message_get_request_headers (msg), "Authorization", auth);
}

static void
webdav_handle_auth_401 (Node *root, guint flags)
{
	root->subscription->error = FETCH_ERROR_AUTH;
	root->available = FALSE;
	node_source_set_state (root, NODE_SOURCE_STATE_NO_AUTH);
	auth_dialog_new (root->subscription, flags);
}

/* ------------------------------------------------------------------ */
/*  URL helpers                                                         */
/* ------------------------------------------------------------------ */

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

/* ------------------------------------------------------------------ */
/*  Libsoup-3 RFC-1123 date helpers                                     */
/* ------------------------------------------------------------------ */

static gint64
parse_http_date (const gchar *date_str)
{
	if (!date_str)
		return 0;
	GDateTime *dt = soup_date_time_new_from_http_string (date_str);
	if (!dt)
		return 0;
	gint64 ts = g_date_time_to_unix (dt);
	g_date_time_unref (dt);
	return ts;
}

static gchar *
format_http_date (gint64 unix_ts)
{
	GDateTime *dt = g_date_time_new_from_unix_utc (unix_ts);
	if (!dt)
		return NULL;
	gchar *s = soup_date_time_to_string (dt, SOUP_DATE_HTTP);
	g_date_time_unref (dt);
	return s;
}

/* ------------------------------------------------------------------ */
/*  Synchronous PUT helper                                              */
/* ------------------------------------------------------------------ */

gboolean
webdav_put (Node *root, const gchar *url, const gchar *data, const gchar *content_type)
{
	SoupMessage *msg;
	GBytes      *bytes;
	GError      *err = NULL;
	gboolean     ok  = FALSE;

	msg = soup_message_new (SOUP_METHOD_PUT, url);
	if (!msg) {
		debug (DEBUG_UPDATE, "webdav_put: invalid URL %s", url);
		return FALSE;
	}

	webdav_add_auth_header (root, msg);

	bytes = g_bytes_new_static (data, strlen (data));
	soup_message_set_request_body_from_bytes (msg, content_type, bytes);
	g_bytes_unref (bytes);

	g_autoptr(GBytes) response = soup_session_send_and_read (webdav_get_session (), msg, NULL, &err);
	guint status = soup_message_get_status (msg);

	if (err) {
		debug (DEBUG_UPDATE, "webdav_put(%s): network error: %s", url, err->message);
		g_error_free (err);
	} else if (status == 401) {
		debug (DEBUG_UPDATE, "webdav_put(%s): HTTP 401 Unauthorized", url);
		webdav_handle_auth_401 (root, 0);
	} else if (status >= 200 && status < 300) {
		ok = TRUE;
		debug (DEBUG_UPDATE, "webdav_put(%s): HTTP %u OK", url, status);
	} else {
		debug (DEBUG_UPDATE, "webdav_put(%s): HTTP %u", url, status);
	}

	g_object_unref (msg);
	return ok;
}

/* ------------------------------------------------------------------ */
/*  Synchronous MKCOL helper                                            */
/* ------------------------------------------------------------------ */

static void
webdav_mkcol (Node *root, const gchar *url)
{
	SoupMessage *msg;
	GError      *err = NULL;

	msg = soup_message_new ("MKCOL", url);
	if (!msg)
		return;

	webdav_add_auth_header (root, msg);

	g_autoptr(GBytes) response = soup_session_send_and_read (webdav_get_session (), msg, NULL, &err);
	guint status = soup_message_get_status (msg);

	if (err) {
		debug (DEBUG_UPDATE, "webdav_mkcol(%s): %s", url, err->message);
		g_error_free (err);
	} else if (status == 401) {
		debug (DEBUG_UPDATE, "webdav_mkcol(%s): HTTP 401 Unauthorized", url);
		webdav_handle_auth_401 (root, 0);
	} else {
		debug (DEBUG_UPDATE, "webdav_mkcol(%s): HTTP %u", url, status);
	}

	g_object_unref (msg);
}

static void
webdav_ensure_collection (Node *root)
{
	g_autofree gchar *url = g_strdup_printf (
		"%s/",
		(const gchar *)g_object_get_data (G_OBJECT (root), "collectionUrl")
	);
	webdav_mkcol (root, url);
}

gboolean
webdav_source_login (Node *root, guint32 flags)
{
	if (root->source->loginState == NODE_SOURCE_STATE_ACTIVE)
		return TRUE;

	if (root->source->loginState == NODE_SOURCE_STATE_IN_PROGRESS)
		return FALSE;

	node_source_set_state (root, NODE_SOURCE_STATE_IN_PROGRESS);

	/* Phase 3: Async bootstrap sequence - MKCOL -> index fetch -> import.
	 * Create async operation context to chain dependent requests without blocking. */
	if (!GPOINTER_TO_INT (g_object_get_data (G_OBJECT (root), "initialImportDone"))) {
		WebDAVAsyncOp *ctx = g_new0 (WebDAVAsyncOp, 1);
		ctx->root = root;
		ctx->flags = flags;
		ctx->step = BOOTSTRAP_STEP_MKCOL;
		ctx->pending_feeds = 0;
		ctx->parsed_index = NULL;

		/* Start async bootstrap sequence */
		webdav_request_mkcol_bootstrap (root, ctx);
	} else {
		/* Already imported, just set active and trigger updates */
		node_source_set_state (root, NODE_SOURCE_STATE_ACTIVE);
		
		/* Trigger update via subscription callback (which now uses queued requests) */
		if (!(flags & NODE_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (root->subscription, flags);
	}

	return TRUE;
}

/* ------------------------------------------------------------------ */
/*  Synchronous GET helper with optional If-Modified-Since              */
/* ------------------------------------------------------------------ */

/**
 * GET @url and return the response body as a newly-allocated string,
 * or NULL on error or when the server returns 304 Not Modified.
 *
 * Pass @if_modified_since > 0 to send an If-Modified-Since header; a 304
 * response is treated as "nothing changed" and returns NULL.
 *
 * If @out_last_modified is non-NULL it is filled with the response
 * Last-Modified Unix timestamp (0 if the header is absent).
 */
gchar *
webdav_get (Node *root, const gchar *url, gint64 if_modified_since, gint64 *out_last_modified, guint *out_http_status)
{
	SoupMessage *msg;
	GError      *err = NULL;
	gchar       *result = NULL;

	msg = soup_message_new (SOUP_METHOD_GET, url);
	if (!msg)
		return NULL;

	webdav_add_auth_header (root, msg);

	if (if_modified_since > 0) {
		g_autofree gchar *hdr = format_http_date (if_modified_since);
		if (hdr)
			soup_message_headers_append (soup_message_get_request_headers (msg),
			                             "If-Modified-Since", hdr);
	}

	g_autoptr(GBytes) response = soup_session_send_and_read (webdav_get_session (), msg, NULL, &err);
	guint status = soup_message_get_status (msg);
	if (out_http_status)
		*out_http_status = status;

	if (err) {
		debug (DEBUG_UPDATE, "webdav_get(%s): %s", url, err->message);
		g_error_free (err);
	} else if (status == 401) {
		debug (DEBUG_UPDATE, "webdav_get(%s): HTTP 401 Unauthorized", url);
		webdav_handle_auth_401 (root, 0);
	} else if (status == 304) {
		debug (DEBUG_UPDATE, "webdav_get(%s): 304 Not Modified", url);
		/* result stays NULL — caller treats as "no change" */
	} else if (status == 200 && response) {
		gsize len = 0;
		const gchar *data = g_bytes_get_data (response, &len);
		result = g_strndup (data, len);

		if (out_last_modified) {
			const gchar *lm = soup_message_headers_get_one (
			    soup_message_get_response_headers (msg), "Last-Modified");
			*out_last_modified = parse_http_date (lm);
		}
	} else {
		debug (DEBUG_UPDATE, "webdav_get(%s): HTTP %u", url, status);
	}

	g_object_unref (msg);
	return result;
}

/* ------------------------------------------------------------------ */
/*  Build combined node.json (node metadata + items array)              */
/* ------------------------------------------------------------------ */

/**
 * Build a JSON string containing the node's metadata and all its items.
 *
 * Format:
 *   {
 *     "node":  { ...node_to_json() output... },
 *     "items": [ ...item_to_json() per item... ]
 *   }
 *
 * This replaces the previous per-item file approach.  Items are read once
 * per upload (daily cadence) so the O(n) cost is acceptable.
 */
gchar *
webdav_build_feed_json (Node *node)
{
	JsonBuilder *b = json_builder_new ();
	json_builder_begin_object (b);

	/* Embed node_to_json() output as a nested "node" object */
	json_builder_set_member_name (b, "node");
	g_autofree gchar *node_json = node_to_json (node);
	if (node_json) {
		JsonParser *np  = json_parser_new ();
		GError     *err = NULL;
		if (json_parser_load_from_data (np, node_json, -1, &err)) {
			json_builder_add_value (b, json_node_copy (json_parser_get_root (np)));
		} else {
			g_warning ("webdav: node_to_json parse error: %s", err->message);
			g_error_free (err);
			json_builder_add_null_value (b);
		}
		g_object_unref (np);
	} else {
		json_builder_add_null_value (b);
	}

	/* Embed all items as a JSON array */
	json_builder_set_member_name (b, "items");
	json_builder_begin_array (b);

	itemSetPtr itemset = node_get_itemset (node);
	if (itemset) {
		for (GList *iter = itemset->ids; iter; iter = g_list_next (iter)) {
			gulong       item_id  = GPOINTER_TO_UINT (iter->data);
			LifereaItem *item     = item_load (item_id);
			if (!item)
				continue;

			g_autofree gchar *item_json = item_to_json (item);
			if (item_json) {
				JsonParser *ip  = json_parser_new ();
				GError     *err = NULL;
				if (json_parser_load_from_data (ip, item_json, -1, &err)) {
					json_builder_add_value (b, json_node_copy (json_parser_get_root (ip)));
				} else {
					g_warning ("webdav: item_to_json parse error: %s", err->message);
					g_error_free (err);
				}
				g_object_unref (ip);
			}
			g_object_unref (item);
		}
		itemset_free (itemset);
	}

	json_builder_end_array (b);
	json_builder_end_object (b);

	gchar *result = json_dump (b);
	g_object_unref (b);
	return result;
}

/* ------------------------------------------------------------------ */
/*  Build state.json (compact read/flagged state for all items)         */
/* ------------------------------------------------------------------ */

/**
 * Build a JSON string mapping each item's sourceId to its read/flagged state.
 *
 * Format:
 *   { "<sourceId>": { "read": true, "flagged": false }, ... }
 *
 * This file is written on every item-state change (O(n) items, small output).
 * Merge strategy on import is union: a bit can only be set, never cleared.
 */
gchar *
webdav_build_state_json (Node *node)
{
	JsonBuilder *b = json_builder_new ();
	json_builder_begin_object (b);

	itemSetPtr itemset = node_get_itemset (node);
	if (itemset) {
		for (GList *iter = itemset->ids; iter; iter = g_list_next (iter)) {
			gulong       item_id = GPOINTER_TO_UINT (iter->data);
			LifereaItem *item    = item_load (item_id);
			if (!item)
				continue;

			const gchar *src_id = item_get_id (item);
			if (src_id && *src_id) {
				json_builder_set_member_name (b, src_id);
				json_builder_begin_object (b);
				json_builder_set_member_name (b, "read");
				json_builder_add_boolean_value (b, item->readStatus);
				json_builder_set_member_name (b, "flagged");
				json_builder_add_boolean_value (b, item->flagStatus);
				json_builder_end_object (b);
			}
			g_object_unref (item);
		}
		itemset_free (itemset);
	}

	json_builder_end_object (b);
	gchar *result = json_dump (b);
	g_object_unref (b);
	return result;
}

/* ------------------------------------------------------------------ */
/*  Upload helpers                                                      */
/* ------------------------------------------------------------------ */

void
webdav_upload_feed (Node *root, const gchar *node_id)
{
	Node *node = node_from_id (node_id);
	const gchar *remote_id;
	if (!node) {
		debug (DEBUG_UPDATE, "webdav_upload_feed: node %s no longer exists", node_id);
		return;
	}

	if (!IS_FEED (node)) {
		debug (DEBUG_UPDATE, "webdav_upload_feed: skip non-feed node %s", node_id);
		return;
	}

	remote_id = webdav_feed_remote_id (node);
	if (!remote_id || !*remote_id)
		return;

	debug (DEBUG_UPDATE, "webdav_upload_feed: uploading local=%s remote=%s (%s)",
	       node_id, remote_id, node_get_title (node));

	/* Retry creating top-level collection in case startup MKCOL previously failed. */
	webdav_ensure_collection (root);

	/* Ensure the per-feed directory exists */
	g_autofree gchar *feed_dir = webdav_feed_dir_url (root, remote_id);
	webdav_mkcol (root, feed_dir);

	g_autofree gchar *json     = webdav_build_feed_json (node);
	if (json) {
		g_autofree gchar *feed_url = webdav_feed_json_url (root, remote_id);
		if (webdav_put (root, feed_url, json, "application/json")) {
			gint64 *ts = g_new (gint64, 1);
			*ts = (gint64)(g_get_real_time () / G_USEC_PER_SEC);
			g_hash_table_insert (
				(GHashTable *)g_object_get_data (G_OBJECT (root), "feedMtimes"),
				g_strdup (remote_id), ts
			);
		}
	}
}

void
webdav_upload_state (Node *root, const gchar *node_id)
{
	Node *node = node_from_id (node_id);
	const gchar *remote_id;

	if (!node) {
		debug (DEBUG_UPDATE, "webdav_upload_state: node %s no longer exists", node_id);
		return;
	}

	if (!IS_FEED (node)) {
		debug (DEBUG_UPDATE, "webdav_upload_state: skip non-feed node %s", node_id);
		return;
	}

	remote_id = webdav_feed_remote_id (node);
	if (!remote_id || !*remote_id)
		return;

	debug (DEBUG_UPDATE, "webdav_upload_state: uploading state for local=%s remote=%s", node_id, remote_id);

	/* Retry creating top-level collection in case startup MKCOL previously failed. */
	webdav_ensure_collection (root);

	g_autofree gchar *json = webdav_build_state_json (node);
	if (json) {
		g_autofree gchar *state_url = webdav_state_json_url (root, remote_id);
		if (webdav_put (root, state_url, json, "application/json")) {
			gint64 *ts = g_new (gint64, 1);
			*ts = (gint64)(g_get_real_time () / G_USEC_PER_SEC);

			g_hash_table_insert (
				(GHashTable *)g_object_get_data (G_OBJECT (root), "stateMtimes"),
				g_strdup (remote_id),
				ts
			);
		}
	}
}

/* ------------------------------------------------------------------ */
/*  Lazy sync flush callbacks (Phase 4: enqueue async jobs)             */
/* ------------------------------------------------------------------ */

typedef struct {
	Node	*root;
	gchar	*node_id;
} FlushCtx;

static void
flush_ctx_free (FlushCtx *ctx)
{
	g_free (ctx->node_id);
	g_free (ctx);
}

/**
 * Phase 4: Async flush feed callback.
 * Instead of calling webdav_upload_feed (blocking), enqueue async upload.
 */
static gboolean
webdav_flush_feed_cb (gpointer user_data)
{
	FlushCtx   *ctx = (FlushCtx *)user_data;
	GHashTable *dirtyFeeds = (GHashTable *)g_object_get_data (G_OBJECT (ctx->root), "dirtyFeeds");
	DirtyEntry *de  = g_hash_table_lookup (dirtyFeeds, ctx->node_id);
	if (de)
		de->feed_timer_id = 0;

	/* Phase 4: Queue async feed upload instead of blocking PUT */
	debug (DEBUG_UPDATE, "webdav_flush_feed_cb: queuing async upload for %s", ctx->node_id);
	webdav_async_upload_feed (ctx->root, ctx->node_id, FALSE, TRUE);

	flush_ctx_free (ctx);
	return G_SOURCE_REMOVE;
}

/**
 * Phase 4: Async flush state callback.
 * Instead of calling webdav_upload_state (blocking), enqueue async upload.
 */
static gboolean
webdav_flush_state_cb (gpointer user_data)
{
	FlushCtx   *ctx = (FlushCtx *)user_data;
	GHashTable *dirtyFeeds = (GHashTable *)g_object_get_data (G_OBJECT (ctx->root), "dirtyFeeds");
	DirtyEntry *de  = g_hash_table_lookup (dirtyFeeds, ctx->node_id);
	if (de)
		de->state_timer_id = 0;

	/* Phase 4: Queue async state upload instead of blocking PUT */
	debug (DEBUG_UPDATE, "webdav_flush_state_cb: queuing async upload for %s", ctx->node_id);
	webdav_async_upload_feed (ctx->root, ctx->node_id, TRUE, FALSE);

	flush_ctx_free (ctx);
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

// FIXME: nonsense implement set_flag and set_read to mark dirty and delay the upload instead of uploading immediately
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

/* ------------------------------------------------------------------ */
/*  nodeSourceType callbacks                                            */
/* ------------------------------------------------------------------ */

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

	g_object_set_data      (G_OBJECT (root), "initialImportDone", FALSE);
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

static void
webdav_source_deinit (void)
{
	if (webdav_soup_session) {
		g_object_unref (webdav_soup_session);
		webdav_soup_session = NULL;
	}
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
	.source_type_init    = NULL,
	.source_type_deinit  = webdav_source_deinit,
	.source_new	     = webdav_source_new,
	.source_create       = ui_webdav_source_get_url,
	.source_delete       = opml_source_remove,
	.source_auto_update  = webdav_source_auto_update,
	.source_free         = webdav_source_free,
	.item_set_flag       = NULL,
	.item_mark_read      = NULL,
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
