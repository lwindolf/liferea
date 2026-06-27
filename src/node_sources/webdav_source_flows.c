/**
 * @file webdav_source.c  WebDAV source request flows
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

#include "webdav_source_flows.h"

#include "debug.h"
#include "update.h"
#include "node_providers/feed.h"
#include "webdav_source.h"

/* data helpers */

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
static gchar *
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
static gchar *
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

/* feed upload flow */

typedef enum {
        UPLOAD_FEED_START     = 0,
        UPLOAD_FEED_MKCOL     = 1,
        UPLOAD_FEED_PUT_NODE  = 2,
        UPLOAD_FEED_PUT_STATE = 3,
        UPLOAD_FEED_FINISH    = 4
} StepUploadFeed;

typedef struct {
        Node *root;
        gchar *node_id;
        gchar *remote_id;
        gboolean feed_dirty;
        gboolean state_dirty;
        StepUploadFeed step;
} FlowUploadFeed;

static gboolean
webdav_source_flow_upload_feed_step (UpdateJob *job)
{
        UpdateResult    *result = job->result;
        FlowUploadFeed  *flow = (FlowUploadFeed *) job->user_data;
        Node            *node = node_from_id (flow->node_id);

	debug (DEBUG_UPDATE, "webdav_source_flow_upload_feed: step=%d feed=%d state=%d for node '%s' (%s)", flow->step, flow->feed_dirty, flow->state_dirty, flow->node_id, node?node->title:"NULL");

        if (!node)
                return TRUE;

        /* result processing */
        if (result && result->httpstatus == 401) {
                node_source_set_state (flow->root, NODE_SOURCE_STATE_NO_AUTH);
                return TRUE;
        }

        switch (flow->step) {
                case UPLOAD_FEED_START:
                        break;
                case UPLOAD_FEED_MKCOL:
                        if (!((result->httpstatus >= 200 && result->httpstatus < 300) || result->httpstatus == 405)) {
                                // FIXME: provide better error for user
                                flow->root->available = FALSE;
                                flow->root->subscription->error = FETCH_ERROR_NET;
                                return TRUE;
                        }
                        break;
                default:
                        if (!(result->httpstatus >= 200 && result->httpstatus < 400)) {
                                // FIXME: provide better error for user
                                flow->root->available = FALSE;
                                flow->root->subscription->error = FETCH_ERROR_NET;
                                return TRUE;
                        }
                        break;
        }
        
        /* state machine */
        switch (flow->step) {
                case UPLOAD_FEED_START:
                        /* Initially there is no result, skip to next step */
                        flow->step = UPLOAD_FEED_MKCOL;
                        break;
                default:
                        if (flow->feed_dirty)
                                flow->step = UPLOAD_FEED_PUT_NODE;
                        else if (flow->state_dirty)
                                flow->step = UPLOAD_FEED_PUT_STATE;
                        else
                                flow->step = UPLOAD_FEED_FINISH;
                        break;
        }

        /* request preparation */
        switch (flow->step) {
                case UPLOAD_FEED_START:
                case UPLOAD_FEED_MKCOL: {
                        g_autofree gchar *url = webdav_feed_dir_url (flow->root, flow->remote_id);

                        g_assert (!job->request);
                        job->request = update_request_new ("MKCOL", url, NULL, NULL);
                        webdav_request_set_basic_auth (job->request, flow->root);
                        break;
                }
                case UPLOAD_FEED_PUT_NODE: {
                        g_autofree gchar *json = webdav_build_feed_json (node);
	                g_autofree gchar *url = webdav_feed_json_url (flow->root, flow->node_id);

                        update_request_set_method (job->request, "PUT");
                        update_request_set_source (job->request, url);
        	        update_request_set_postdata (job->request, json, "application/json");
                        
                        flow->feed_dirty = FALSE;
                        break;
                }
                case UPLOAD_FEED_PUT_STATE: {
                        g_autofree gchar *json = webdav_build_state_json (node);
	                g_autofree gchar *url = webdav_state_json_url (flow->root, flow->node_id);

                        update_request_set_method (job->request, "PUT");
                        update_request_set_source (job->request, url);
        	        update_request_set_postdata (job->request, json, "application/json");
                        
                        flow->state_dirty = FALSE;
                        break;
                }
                case UPLOAD_FEED_FINISH: {
                        // FIXME: this mtime handling would be better in a callback and in webdav_source_feed_list.c
                        gint64 *ts = g_new (gint64, 1);
                        *ts = (gint64)(g_get_real_time () / G_USEC_PER_SEC);
                        g_hash_table_insert (
                                (GHashTable *)g_object_get_data (G_OBJECT (flow->root), "feedMtimes"),
                                g_strdup (flow->remote_id),
                                ts
                        );

                        return TRUE;
                        break;
                }
        }

        return FALSE;   /* continue the flow */
}

static void
webdav_source_flow_upload_feed_free (gpointer data)
{
        FlowUploadFeed *flow = (FlowUploadFeed *)data;

        g_free (flow->node_id);
        g_free (flow->remote_id);
        g_free (flow);
}

void
webdav_source_flow_upload_feed (Node *root, const gchar *node_id, gboolean upload_state, gboolean upload_feed)
{
	Node *node = node_from_id (node_id);
	if (!node) {
		debug (DEBUG_UPDATE, "webdav_source_flow_upload_feed: node %s no longer exists", node_id);
		return;
	}

	if (!IS_FEED (node)) {
		debug (DEBUG_UPDATE, "webdav_source_flow_upload_feed: skip non-feed node %s", node_id);
		return;
	}

	const gchar *remote_id = webdav_feed_remote_id (node);
	if (!remote_id || !*remote_id)
		return;

        FlowUploadFeed *flow = g_new0(FlowUploadFeed, 1);
        flow->step = UPLOAD_FEED_START;
        flow->root = root;
	flow->node_id = g_strdup (node_id);
        flow->remote_id = g_strdup (remote_id);
        flow->state_dirty = upload_state;
	flow->feed_dirty = upload_feed;

        update_job_new_flow (
                root,
                webdav_source_flow_upload_feed_step,
                flow,
                webdav_source_flow_upload_feed_free,
                0);
}

/* index bootstrap flow */

typedef enum {
        BOOTSTRAP_STEP_START     = 0,
	BOOTSTRAP_STEP_MKCOL     = 1,
	BOOTSTRAP_STEP_GET_INDEX = 2,
	BOOTSTRAP_STEP_FINISH    = 3
} StepBootstrap;

typedef struct {
	Node           *root;           /* Source root node */
	StepBootstrap   step;           /* Current operation step */
        indexFetchCallback callback;       /* Callback for index.json result */
} FlowBootstrap;

static gboolean
webdav_source_flow_bootstrap_step (UpdateJob *job)
{
        UpdateResult    *result = job->result;
        FlowBootstrap   *flow = (FlowBootstrap *) job->user_data;

	debug (DEBUG_UPDATE, "webdav_source_flow_bootstrap: step=%d for node '%s' (%s)", flow->step, flow->root->id, flow->root->title);

        /* result processing */
        if (result && result->httpstatus == 401) {
                node_source_set_state (flow->root, NODE_SOURCE_STATE_NO_AUTH);
                return TRUE;
        }

        switch (flow->step) {
                case BOOTSTRAP_STEP_START:
                        break;
                case BOOTSTRAP_STEP_MKCOL:
                        if (!((result->httpstatus >= 200 && result->httpstatus < 300) || result->httpstatus == 405)) {
                                // FIXME: provide better error for user
                                flow->root->available = FALSE;
                                flow->root->subscription->error = FETCH_ERROR_NET;
                                return TRUE;
                        }
                        break;

                case BOOTSTRAP_STEP_GET_INDEX:
                        // For index fetch we accept a 404 and a normal result
                        if (!((result->httpstatus >= 200 && result->httpstatus < 400) || result->httpstatus == 404)) {
                                // FIXME: provide better error for user
                                flow->root->available = FALSE;
                                flow->root->subscription->error = FETCH_ERROR_NET;
                                return TRUE;
                        }
                        break;
                default:
                        break;
        }
        
        /* state machine */
        switch (flow->step) {
                case BOOTSTRAP_STEP_START:
                        /* Initially there is no result, skip to next step */
                        flow->step = BOOTSTRAP_STEP_MKCOL;
                        break;
                case BOOTSTRAP_STEP_MKCOL:
                        flow->step = BOOTSTRAP_STEP_GET_INDEX;
                        break;
                case BOOTSTRAP_STEP_GET_INDEX:
                        flow->step = BOOTSTRAP_STEP_FINISH;
                        break;
                default:
                        break;
        }

        /* request preparation */
        switch (flow->step) {
                case BOOTSTRAP_STEP_START:
                case BOOTSTRAP_STEP_MKCOL: {
                       	g_autofree gchar *url = g_strdup_printf (
	                	"%s/",
		                (const gchar *)g_object_get_data (G_OBJECT (flow->root), "collectionUrl")
	                );

                        g_assert(!job->request);
                        job->request = update_request_new ("MKCOL", url, NULL, NULL);
                        webdav_request_set_basic_auth (job->request, flow->root);
                        break;
                }
                case BOOTSTRAP_STEP_GET_INDEX: {
	                g_autofree gchar *url = webdav_index_url (flow->root);
	                update_request_set_method (job->request, "GET");
                        update_request_set_source (job->request, url);
                        break;
                }
                case BOOTSTRAP_STEP_FINISH: {
                        (flow->callback) (flow->root, result->data);
                        return TRUE;
                        break;
                }
        }

        return FALSE;   /* continue the flow */
}

static void
webdav_source_flow_bootstrap_free (gpointer data)
{
        g_free (data);
}

void
webdav_source_flow_bootstrap_index (Node *root, indexFetchCallback cb)
{
        FlowBootstrap *flow = g_new0(FlowBootstrap, 1);
        flow->step = BOOTSTRAP_STEP_START;
        flow->root = root;
        flow->callback = cb;

        update_job_new_flow (
                root,
                webdav_source_flow_bootstrap_step,
                flow,
                webdav_source_flow_bootstrap_free,
                0);
}