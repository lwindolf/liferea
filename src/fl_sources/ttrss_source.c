/**
 * @file ttrss_source.c  Tiny Tiny RSS feed list source support
 *
 * Copyright (C) 2010-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "fl_sources/ttrss_source.h"

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
#include "ui/liferea_dialog.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"

/** Initialize a TinyTinyRSS source with given node as root */
static ttrssSourcePtr
ttrss_source_new (nodePtr node)
{
	ttrssSourcePtr source = g_new0 (struct ttrssSource, 1) ;
	source->root = node;
	source->apiLevel = 0;
	source->categories = g_hash_table_new (g_direct_hash, g_direct_equal);
	source->folderToCategory = g_hash_table_new (g_str_hash, g_str_equal);

	return source;
}

static void
ttrss_source_free (ttrssSourcePtr source)
{
	if (!source)
		return;

	update_job_cancel_by_owner (source);

	g_hash_table_destroy (source->categories);
	g_hash_table_destroy (source->folderToCategory);
	g_free (source->session_id);
	g_free (source);
}

static void
ttrss_source_set_login_error (ttrssSourcePtr source, gchar *msg)
{
	g_free (source->root->subscription->updateError);
	source->root->subscription->updateError = msg;
	source->root->available = FALSE;

	g_print ("TinyTinyRSS login failed: error '%s'!\n", msg);

	node_source_set_state (source->root, NODE_SOURCE_STATE_NONE);
}

static void
ttrss_source_login_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	ttrssSourcePtr	source = (ttrssSourcePtr) userdata;
	subscriptionPtr subscription = source->root->subscription;
	JsonParser	*parser;

	debug1 (DEBUG_UPDATE, "TinyTinyRSS login processing... >>>%s<<<", result->data);

	g_assert (!source->session_id);

	if (!(result->data && result->httpstatus == 200)) {
		ttrss_source_set_login_error (source, g_strdup_printf ("Login request failed with HTTP error %d", result->httpstatus));
		return;
	}

	parser = json_parser_new ();
	if (!json_parser_load_from_data (parser, result->data, -1, NULL)) {
		ttrss_source_set_login_error (source, g_strdup ("Invalid JSON returned on login!"));
		return;
	}

	JsonNode *node = json_parser_get_root (parser);

	/* Check for API specified error... */
	if (json_get_string (json_get_node (node, "content"), "error")) {
		const gchar *error = json_get_string (json_get_node (node, "content"), "error");

		ttrss_source_set_login_error (source, g_strdup (error));
		if (g_str_equal (error, "LOGIN_ERROR"))
			auth_dialog_new (source->root->subscription, flags);

		return;
	}

	/* Success! Get SID and API Level. */
	source->apiLevel = json_get_int (json_get_node (node, "content"), "api_level");
	source->session_id = g_strdup (json_get_string (json_get_node (node, "content"), "session_id"));
	if (source->session_id) {
		debug2 (DEBUG_UPDATE, "TinyTinyRSS Found session_id: >>>%s<<< (API level %d)!", source->session_id, source->apiLevel);

		node_source_set_state (subscription->node, NODE_SOURCE_STATE_ACTIVE);

		if (!(flags & NODE_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (subscription, flags);

	} else {
		ttrss_source_set_login_error (source, g_strdup_printf ("No session_id found in response!\n%s", result->data));
	}

	g_object_unref (parser);
}

/**
 * Perform a login to tt-rss, if the login completes the ttrssSource will
 * have a valid sid and will have loginStatus NODE_SOURCE_LOGIN_ACTIVE.
 */
void
ttrss_source_login (ttrssSourcePtr source, guint32 flags)
{
	gchar			*username, *password, *source_uri;
	UpdateRequest		*request;
	subscriptionPtr		subscription = source->root->subscription;

	if (source->root->source->loginState != NODE_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session doesn't expire. */
		debug1 (DEBUG_UPDATE, "Logging in while login state is %d", source->root->source->loginState);
	}

	source->url = metadata_list_get (subscription->metadata, "ttrss-url");
	if (!source->url) {
		ttrss_source_set_login_error (source, g_strdup ("Fatal: We've lost the TinyTinyRSS server URL! Please re-subscribe!"));
		return;
	}

	source_uri = g_strdup_printf (TTRSS_URL, source->url);

	request = update_request_new (
		source_uri,
		NULL,	// updateState
		subscription->updateOptions
	);

	g_free (source_uri);

	/* escape user and password for JSON call */
	username = g_strescape (subscription->updateOptions->username, NULL);
	password = g_strescape (subscription->updateOptions->password, NULL);

	request->postdata = g_strdup_printf (TTRSS_JSON_LOGIN, username, password);

	g_free (username);
	g_free (password);

	node_source_set_state (source->root, NODE_SOURCE_STATE_IN_PROGRESS);

	update_execute_request (source, request, ttrss_source_login_cb, source, flags | FEED_REQ_NO_FEED);
}

/* node source type implementation */

static void
ttrss_source_auto_update (nodePtr node)
{
	if (node->source->loginState == NODE_SOURCE_STATE_NONE) {
		node_source_update (node);
		return;
	}

	if (node->source->loginState == NODE_SOURCE_STATE_IN_PROGRESS)
		return; /* the update will start automatically anyway */

	debug0 (DEBUG_UPDATE, "ttrss_source_auto_update()");
	subscription_auto_update (node->subscription);
}

static void
ttrss_source_init (void)
{
	metadata_type_register ("ttrss-url", METADATA_TYPE_URL);
	metadata_type_register ("ttrss-feed-id", METADATA_TYPE_TEXT);
}

static void ttrss_source_deinit (void) { }

static void
ttrss_source_import (nodePtr node)
{
	opml_source_import (node);

	node->subscription->updateInterval = -1;
	node->subscription->type = node->source->type->sourceSubscriptionType;
	if (!node->data)
		node->data = (gpointer) ttrss_source_new (node);
}

static void
ttrss_source_subscribe_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	subscriptionPtr subscription = (subscriptionPtr) userdata;

	debug2 (DEBUG_UPDATE, "TinyTinyRSS subscribe result processing... status:%d >>>%s<<<", result->httpstatus, result->data);

	if (200 != result->httpstatus) {
		ui_show_error_box (_("TinyTinyRSS HTTP API not reachable!"));
		return;
	}

	/* Result should be {"seq":0,"status":0,"content":{"status":{"code":1}}} */
	// FIXME: poor mans matching
	if (!strstr (result->data, "\"code\":1")) {
		ui_show_error_box (_("TinyTinyRSS subscribing to feed failed! Check if you really passed a feed URL!"));
		return;
	}

	/* As TinyTinyRSS does not return the id of the newly subscribed feed
	   we need to reload the entire feed list. */
	node_source_update (subscription->node->source->root);
}

static nodePtr
ttrss_source_add_subscription (nodePtr root, subscriptionPtr subscription)
{
	nodePtr			parent;
	gchar			*username, *password;
	ttrssSourcePtr		source = (ttrssSourcePtr)root->data;
	UpdateRequest		*request;
	gint			categoryId = 0;
	gchar			*source_uri;

	/* Determine correct category from selected folder name */
	parent = feedlist_get_selected ();
	if (parent) {
		if (parent->subscription)
			parent = parent->parent;
		categoryId = GPOINTER_TO_INT (g_hash_table_lookup (source->folderToCategory, parent->id));
	}

	source_uri = g_strdup_printf (TTRSS_URL, source->url);

	request = update_request_new (
		source_uri,
		NULL,
		root->subscription->updateOptions
	);

	g_free (source_uri);

	/* escape user and password for JSON call */
	username = g_strescape (root->subscription->updateOptions->username, NULL);
	password = g_strescape (root->subscription->updateOptions->password, NULL);

	request->postdata = g_strdup_printf (TTRSS_JSON_SUBSCRIBE, source->session_id, subscription->source, categoryId, username, password);

	g_free (username);
	g_free (password);

	update_execute_request (source, request, ttrss_source_subscribe_cb, source, 0 /* flags */);

	return NULL;
}

static void
ttrss_source_remove_node_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	nodePtr node = (nodePtr) userdata;

	debug2 (DEBUG_UPDATE, "TinyTinyRSS remove node result processing... status:%d >>>%s<<<", result->httpstatus, result->data);

	if (200 != result->httpstatus) {
		ui_show_error_box (_("TinyTinyRSS HTTP API not reachable!"));
		return;
	}

	/* We expect the following {"seq":0,"status":0,"content":{"status":"OK"}} */
	// FIXME: poor mans matching
	if (!strstr (result->data, "\"status\":0")) {
		ui_show_error_box (_("TinyTinyRSS unsubscribing feed failed!"));
		return;
	}

	feedlist_node_removed (node);
}

static void
ttrss_source_remove_node (nodePtr root, nodePtr node)
{
	ttrssSourcePtr	source = (ttrssSourcePtr)root->data;
	UpdateRequest	*request;
	const gchar	*id;
	gchar		*source_uri;

	// FIXME: Check for login?

	if (source->apiLevel < 5) {
		ui_show_info_box (_("This TinyTinyRSS version does not support removing feeds. Upgrade to version %s or later!"), "1.7.6");
		return;
	}

	id = metadata_list_get (node->subscription->metadata, "ttrss-feed-id");
	if (!id) {
		g_print ("Cannot remove node on remote side as ttrss-feed-id is unknown!");
		return;
	}

	source_uri = g_strdup_printf (TTRSS_URL, source->url);

	request = update_request_new (
		source_uri,
		NULL,
		root->subscription->updateOptions
	);

	g_free (source_uri);

	request->postdata = g_strdup_printf (TTRSS_JSON_UNSUBSCRIBE, source->session_id, id);

	update_execute_request (source, request, ttrss_source_remove_node_cb, node, 0 /* flags */);
}

/* GUI callbacks */

static void
on_ttrss_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data)
{
	if (response_id == GTK_RESPONSE_OK) {
		nodePtr		node;

		node = node_new (node_source_get_node_type ());
		node_source_new (node, ttrss_source_get_type (), "");
		node_set_title (node, node->source->type->name);

		/* This is a bit ugly: we need to prevent the tt-rss base
		   URL from being lost by unwanted permanent redirects on
		   the getFeeds call, so we save it as the homepage meta
		   data value... */
		metadata_list_set (&node->subscription->metadata, "ttrss-url", gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "serverUrlEntry"))));

		subscription_set_auth_info (node->subscription,
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))),
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));

		node->data = (gpointer)ttrss_source_new (node);
		feedlist_node_added (node);
		node_source_update (node);

		db_node_update (node);	/* because of metadate_list_set() above */
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ui_ttrss_source_get_account_info (void)
{
	GtkWidget	*dialog;

	dialog = liferea_dialog_new ("ttrss_source");

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_ttrss_source_selected),
			  NULL);
}

static void
ttrss_source_cleanup (nodePtr node)
{
	ttrssSourcePtr source = (ttrssSourcePtr) node->data;
	ttrss_source_free (source);
	node->data = NULL;
}

static void
ttrss_source_remote_update_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	debug2 (DEBUG_UPDATE, "TinyTinyRSS update result processing... status:%d >>>%s<<<", result->httpstatus, result->data);
}

/* FIXME: Only simple synchronous item change requests... Get async! */

static void
ttrss_source_item_set_flag (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr		root = node_source_root_from_node (node);
	ttrssSourcePtr	source = (ttrssSourcePtr)root->data;
	UpdateRequest	*request;
	gchar		*source_uri;

	source_uri = g_strdup_printf (TTRSS_URL, source->url);

	request = update_request_new (
		source_uri,
		NULL,
		root->subscription->updateOptions
	);

	g_free (source_uri);

	request->postdata = g_strdup_printf (TTRSS_JSON_UPDATE_ITEM_FLAG, source->session_id, item_get_id(item), newStatus?1:0 );

	update_execute_request (source, request, ttrss_source_remote_update_cb, source, 0 /* flags */);

	item_flag_state_changed (item, newStatus);
}

static void
ttrss_source_item_mark_read (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr		root = node_source_root_from_node (node);
	ttrssSourcePtr	source = (ttrssSourcePtr)root->data;
	UpdateRequest	*request;
	gchar		*source_uri;

	source_uri = g_strdup_printf (TTRSS_URL, source->url);

	request = update_request_new (
		source_uri,
		NULL,
		root->subscription->updateOptions
	);
	g_free (source_uri);

	request->postdata = g_strdup_printf (TTRSS_JSON_UPDATE_ITEM_UNREAD, source->session_id, item_get_id(item), newStatus?0:1 );

	update_execute_request (source, request, ttrss_source_remote_update_cb, source, 0 /* flags */);

	item_read_state_changed (item, newStatus);
}

/**
* Convert all subscriptions of a google source to local feeds
*
* @param node The node to migrate (not the nodeSource!)
*/
static void
ttrss_source_convert_to_local (nodePtr node)
{
	node_source_set_state (node, NODE_SOURCE_STATE_MIGRATE);
}

/* node source type definition */

extern struct subscriptionType ttrssSourceFeedSubscriptionType;
extern struct subscriptionType ttrssSourceSubscriptionType;

static struct nodeSourceType nst = {
	.id                  = "fl_ttrss",
	.name                = N_("Tiny Tiny RSS"),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
	                       NODE_SOURCE_CAPABILITY_CAN_LOGIN |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
	                       NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
	                       NODE_SOURCE_CAPABILITY_ADD_FEED |
	                       NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL,
	.feedSubscriptionType = &ttrssSourceFeedSubscriptionType,
	.sourceSubscriptionType = &ttrssSourceSubscriptionType,
	.source_type_init    = ttrss_source_init,
	.source_type_deinit  = ttrss_source_deinit,
	.source_new          = ui_ttrss_source_get_account_info,
	.source_delete       = opml_source_remove,
	.source_import       = ttrss_source_import,
	.source_export       = opml_source_export,
	.source_get_feedlist = opml_source_get_feedlist,
	.source_auto_update  = ttrss_source_auto_update,
	.free                = ttrss_source_cleanup,
	.item_set_flag       = ttrss_source_item_set_flag,
	.item_mark_read      = ttrss_source_item_mark_read,
	.add_folder          = NULL,	/* not supported by current tt-rss JSON API (v1.8) */
	.add_subscription    = ttrss_source_add_subscription,
	.remove_node         = ttrss_source_remove_node,
	.convert_to_local    = ttrss_source_convert_to_local
};

nodeSourceTypePtr
ttrss_source_get_type (void)
{
	return &nst;
}
