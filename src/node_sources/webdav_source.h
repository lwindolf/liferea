/**
 * @file webdav_source.h  WebDAV-based feed list sync source
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

#ifndef _WEBDAV_SOURCE_H
#define _WEBDAV_SOURCE_H

#include "node.h"
#include "node_source.h"

/** Name of the collection created/used on the WebDAV server. */
#define WEBDAV_SYNC_COLLECTION  "Liferea Sync"

/**
 * Delay before uploading feed.json after a structural feed change (seconds).
 * Coalesces rapid successive changes into a single write.
 */
#define WEBDAV_LAZY_SYNC_DELAY_S   30

/**
 * Delay before uploading state.json after an item read/flag change (seconds).
 * Shorter than WEBDAV_LAZY_SYNC_DELAY_S because state.json is small.
 */
#define WEBDAV_STATE_SYNC_DELAY_S   5

/**
 * How often to poll the WebDAV server for remote changes (seconds).
 */
#define WEBDAV_SOURCE_UPDATE_INTERVAL  (60 * 15)   /* 15 minutes */

/** Metadata key that stores the stable remote feed id for WebDAV sync. */
#define WEBDAV_REMOTE_FEED_ID_METADATA "remote-feed-id"

/**
 * Per-source instance data attached to the node via g_object_set_data_full().
 *
 * Server layout:
 *   <collectionUrl>/
 *     index.json          -- { "feeds": [{ "id", "feed_mtime", "state_mtime" }] }
 *     <node_id>/
 *       feed.json         -- node metadata + embedded items array
 *       state.json        -- { "<sourceId>": { "read": bool, "flagged": bool } }
 * 
 * State is kept using GObject data fields:
 *
 * initialImportDone boolean
 * collectionUrl     string
 * dirtyFeeds  maps node-id (gchar*) -> DirtyEntry* (two lazy-upload timers).
 * feedMtimes  maps node-id (gchar*) -> gint64*     (last feed.json upload time).
 * stateMtimes maps node-id (gchar*) -> gint64*     (last state.json upload time).
 */

gboolean webdav_source_login (Node *root, guint32 flags);
const gchar *webdav_feed_remote_id (Node *node);
void webdav_feed_set_remote_id (Node *node, const gchar *remote_id);
void webdav_upload_feed (Node *root, const gchar *node_id);
void webdav_upload_state (Node *root, const gchar *node_id);

gchar * webdav_feed_json_url (Node *root, const gchar *node_id);
gchar * webdav_state_json_url (Node *root, const gchar *node_id);
gchar * webdav_index_url (Node *root);
gchar * webdav_feed_dir_url (Node *root, const gchar *node_id);

/* Synchronous helpers (internal use only for now, to be deprecated) */
gchar * webdav_get (Node *root, const gchar *url, gint64 if_modified_since, gint64 *out_last_modified, guint *out_http_status);
gboolean webdav_put (Node *root, const gchar *url, const gchar *data, const gchar *content_type);

gboolean webdav_is_feed_upload_pending (Node *root, const gchar *node_id);
gboolean webdav_is_state_upload_pending (Node *root, const gchar *node_id);

/**
 * webdav_source_get_type:
 *
 * Returns the nodeSourceType descriptor for the WebDAV source.
 */
nodeSourceTypePtr webdav_source_get_type (void);

#endif /* _WEBDAV_SOURCE_H */
