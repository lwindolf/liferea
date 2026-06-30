/**
 * @file ttrss_source.c  Tiny Tiny RSS feed list source support
 *
 * Copyright (C) 2010-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_sources/ttrss_source.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <stdarg.h>

#include "common.h"
#include "debug.h"
#include "db.h"
#include "feedlist.h"
#include "item_state.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "ui/auth_dialog.h"
#include "ui/ui_common.h"
#include "node_source.h"

static void
ttrss_source_new (Node *root)
{
	g_object_set_data_full (
		G_OBJECT (root),
		"categories",
		g_hash_table_new_full (g_direct_hash, g_direct_equal, g_free, g_free),
		(GDestroyNotify)g_hash_table_destroy
	);
	g_object_set_data_full (
		G_OBJECT (root),
		"folderToCategory",
		g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free),
		(GDestroyNotify)g_hash_table_destroy
	);

	// FIXME: document why
	root->subscription->updateInterval = -1;
}

static void
ttrss_source_set_login_error (UpdateJob *job, gchar *msg)
{
	Node *root = (Node *)job->user_data;

	g_free (root->subscription->updateError);
	root->subscription->updateError = msg;

	debug (DEBUG_UPDATE, "ttrss_source: login failed: error '%s'!\n", msg);

	node_source_set_auth_failed (root, job->flags & UPDATE_REQUEST_PRIORITY_HIGH);
}

static gboolean
ttrss_source_login_cb (UpdateJob *job)
{
	UpdateResult	*result = job->result;
	Node		*root = (Node *)job->user_data;
	
	debug (DEBUG_UPDATE, "ttrss_source: %s |%s| login processing... >>>%s<<<", root->id, root->title, result->data);

	if (!(result->data && result->httpstatus == 200)) {
		ttrss_source_set_login_error (job, g_strdup_printf ("Login request failed with HTTP error %d", result->httpstatus));
		return TRUE;
	}

	g_autoptr(JsonParser) parser = json_parser_new ();
	if (!json_parser_load_from_data (parser, result->data, -1, NULL)) {
		ttrss_source_set_login_error (job, g_strdup ("Invalid JSON returned on login!"));
		return TRUE;
	}

	JsonNode *node = json_parser_get_root (parser);

	/* Check for API specified error... */
	if (json_get_string (json_get_node (node, "content"), "error")) {
		const gchar *error = json_get_string (json_get_node (node, "content"), "error");
		debug (DEBUG_UPDATE, "ttrss_source: %s |%s| API error %s", root->id, root->title, error);
		ttrss_source_set_login_error (job, g_strdup (error));
		return TRUE;
	}

	/* Success! Get SID and API Level. */
	g_autofree gchar *session_id = g_strdup (json_get_string (json_get_node (node, "content"), "session_id"));
	if (session_id) {
		debug (DEBUG_UPDATE, "ttrss_source: %s |%s| found session_id: >>>%s<<<!", root->id, root->title, session_id);
		node_source_set_auth_token (root, session_id);
		subscription_update (root->subscription, job->flags);
	} else {
		ttrss_source_set_login_error (job, g_strdup_printf ("No session_id found in response!\n%s", result->data));
	}

	return TRUE;
}

void
ttrss_source_login (Node *root, updateFlags flags)
{
	UpdateRequest		*request;
	subscriptionPtr		subscription = root->subscription;

	if (root->source->loginState != NODE_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session doesn't expire. */
		debug (DEBUG_UPDATE, "Logging in while login state is %d", root->source->loginState);
	}

	g_autofree gchar *source_uri = g_strdup_printf (TTRSS_URL, root->subscription->origSource);
	request = update_request_new (
		"POST",
		source_uri,
		NULL,	// updateState
		subscription->updateOptions
	);

	g_autofree gchar *postdata = g_strdup_printf (
		TTRSS_JSON_LOGIN,
		subscription->updateOptions->username,
		subscription->updateOptions->password
	);
	update_request_set_postdata (request, postdata, "application/json; charset=utf-8");

	update_job_new (root, request, ttrss_source_login_cb, root, flags | UPDATE_REQUEST_NO_FEED);
}

/* node source type implementation */

static void
ttrss_source_init (void)
{
	metadata_type_register ("ttrss-feed-id", METADATA_TYPE_TEXT);
}

static gboolean
ttrss_source_subscribe_cb (UpdateJob *job)
{
	UpdateResult *result = job->result;
	gpointer userdata = job->user_data;
	subscriptionPtr subscription = (subscriptionPtr) userdata;

	debug (DEBUG_UPDATE, "TinyTinyRSS subscribe result processing... status:%d >>>%s<<<", result->httpstatus, result->data);

	if (200 != result->httpstatus) {
		ui_show_error_box (_("TinyTinyRSS HTTP API not reachable!"));
		return TRUE;
	}

	/* Result should be {"seq":0,"status":0,"content":{"status":{"code":1}}} */
	// FIXME: poor mans matching
	if (!strstr (result->data, "\"code\":1")) {
		ui_show_error_box (_("TinyTinyRSS subscribing to feed failed! Check if you really passed a feed URL!"));
		return TRUE;
	}

	/* As TinyTinyRSS does not return the id of the newly subscribed feed
	   we need to reload the entire feed list. */
	node_source_update (subscription->node->source->root);

	return TRUE;
}

static Node *
ttrss_source_add_subscription (Node *root, subscriptionPtr subscription)
{
	Node			*parent;
	UpdateRequest		*request;
	gint			categoryId = 0;
	GHashTable		*h = g_object_get_data (G_OBJECT (root), "folderToCategory");

	/* Determine correct category from selected folder name */
	parent = feedlist_get_selected ();
	if (parent) {
		if (parent->subscription)
			parent = parent->parent;
		categoryId = GPOINTER_TO_INT (g_hash_table_lookup (h, parent->id));
	}

	g_autofree gchar *source_uri = g_strdup_printf (TTRSS_URL, root->subscription->origSource);
	request = update_request_new (
		"POST",
		source_uri,
		NULL,
		root->subscription->updateOptions
	);

	g_autofree gchar *postdata = g_strdup_printf (
		TTRSS_JSON_SUBSCRIBE,
		root->source->authToken,
		subscription->origSource,
		categoryId,
		subscription->updateOptions->username,
		subscription->updateOptions->password
	);
	update_request_set_postdata (request, postdata, "application/json; charset=utf-8");

	update_job_new (root, request, ttrss_source_subscribe_cb, root, 0 /* flags */);

	return NULL;
}

static gboolean
ttrss_source_remove_node_cb (UpdateJob *job)
{
	UpdateResult *result = job->result;
	gpointer userdata = job->user_data;
	Node *node = (Node *) userdata;

	debug (DEBUG_UPDATE, "TinyTinyRSS remove node result processing... status:%d >>>%s<<<", result->httpstatus, result->data);

	if (200 != result->httpstatus) {
		ui_show_error_box (_("TinyTinyRSS HTTP API not reachable!"));
		return TRUE;
	}

	/* We expect the following {"seq":0,"status":0,"content":{"status":"OK"}} */
	// FIXME: poor mans matching
	if (!strstr (result->data, "\"status\":0")) {
		ui_show_error_box (_("TinyTinyRSS unsubscribing feed failed!"));
		return TRUE;
	}

	feedlist_node_removed (node);
	return TRUE;
}

static void
ttrss_source_remove_node (Node *root, Node *node)
{
	UpdateRequest	*request;
	const gchar	*id;

	id = metadata_list_get (node->subscription->metadata, "ttrss-feed-id");
	if (!id) {
		g_print ("Cannot remove node on remote side as ttrss-feed-id is unknown!");
		return;
	}

	g_autofree gchar *source_uri = g_strdup_printf (TTRSS_URL, root->subscription->origSource);
	request = update_request_new (
		"POST",
		source_uri,
		NULL,
		root->subscription->updateOptions
	);

	g_autofree gchar *postdata = g_strdup_printf (
		TTRSS_JSON_UNSUBSCRIBE,
		(const gchar *)g_object_get_data (G_OBJECT (root), "session_id"),
		id
	);
	update_request_set_postdata (request, postdata, "application/json; charset=utf-8");

	update_job_new (root, request, ttrss_source_remove_node_cb, node, 0 /* flags */);
}

static gboolean
ttrss_source_remote_update_cb (UpdateJob *job)
{
	UpdateResult *result = job->result;
	debug (DEBUG_UPDATE, "TinyTinyRSS update result processing... status:%d >>>%s<<<", result->httpstatus, result->data);
	return TRUE;
}

/* FIXME: Only simple synchronous item change requests... Get async! */

static void
ttrss_source_item_set_flag (Node *node, itemPtr item, gboolean newStatus)
{
	Node		*root = node->source->root;
	UpdateRequest	*request;

	g_autofree gchar *source_uri = g_strdup_printf (TTRSS_URL, root->subscription->origSource);
	request = update_request_new (
		"POST",
		source_uri,
		NULL,
		root->subscription->updateOptions
	);

	g_autofree gchar *postdata = g_strdup_printf (
		TTRSS_JSON_UPDATE_ITEM_FLAG,
		root->source->authToken,
		item_get_id(item),
		newStatus?1:0
	);
	update_request_set_postdata (request, postdata, "application/json; charset=utf-8");

	(void)update_job_new (root, request, ttrss_source_remote_update_cb, root, 0 /* flags */);

	item_flag_state_changed (item, newStatus);
}

static void
ttrss_source_item_mark_read (Node *node, itemPtr item, gboolean newStatus)
{
	Node		*root = node->source->root;
	UpdateRequest	*request;

	g_autofree gchar *source_uri = g_strdup_printf (TTRSS_URL, root->subscription->origSource);
	request = update_request_new (
		"POST",
		source_uri,
		NULL,
		root->subscription->updateOptions
	);

	g_autofree gchar *postdata = g_strdup_printf (
		TTRSS_JSON_UPDATE_ITEM_UNREAD,
		root->source->authToken,
		item_get_id (item),
		newStatus?0:1
	);
	update_request_set_postdata (request, postdata, "application/json; charset=utf-8");

	(void)update_job_new (root, request, ttrss_source_remote_update_cb, root, 0 /* flags */);

	item_read_state_changed (item, newStatus);
}

extern struct subscriptionType ttrssSourceFeedSubscriptionType;
extern struct subscriptionType ttrssSourceSubscriptionType;

static struct nodeSourceType nst = {
	.id                  = "fl_ttrss",
	.name                = N_("Tiny Tiny RSS"),
	.addInfo             = N_("Please provide your TinyTinyRSS instance and credentials."),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
	                       NODE_SOURCE_CAPABILITY_CAN_LOGIN |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
	                       NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
	                       NODE_SOURCE_CAPABILITY_ADD_FEED |
	                       NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL,
	.feedSubscriptionType = &ttrssSourceFeedSubscriptionType,
	.sourceSubscriptionType = &ttrssSourceSubscriptionType,
	.source_type_init    = ttrss_source_init,
	.source_type_deinit  = NULL,
	.source_new          = ttrss_source_new,
	.source_login        = ttrss_source_login,
	.item_set_flag       = ttrss_source_item_set_flag,
	.item_mark_read      = ttrss_source_item_mark_read,
	.add_folder          = NULL,	/* not supported by current tt-rss JSON API (v1.8) */
	.add_subscription    = ttrss_source_add_subscription,
	.remove_node         = ttrss_source_remove_node
};

nodeSourceTypePtr
ttrss_source_get_type (void)
{
	return &nst;
}
