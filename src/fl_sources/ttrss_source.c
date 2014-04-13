/**
 * @file ttrss_source.c  tt-rss feed list source support
 * 
 * Copyright (C) 2010-2013 Lars Windolf <lars.lindner@gmail.com>
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
#include "feedlist.h"
#include "item_state.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
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
	source->actionQueue = g_queue_new (); 
	source->loginState = TTRSS_SOURCE_STATE_NONE;
	source->categories = g_hash_table_new (g_direct_hash, g_direct_equal);
	source->categoryNodes = g_hash_table_new (g_direct_hash, g_direct_equal);
	
	return source;
}

static void
ttrss_source_free (ttrssSourcePtr source) 
{
	if (!source)
		return;

	update_job_cancel_by_owner (source);
	
	g_free (source->session_id);
	g_queue_free (source->actionQueue) ;
	g_free (source);
}

static void
ttrss_source_set_login_error (ttrssSourcePtr source, gchar *msg)
{
	g_free (source->root->subscription->updateError);
	source->root->subscription->updateError = msg;
	source->root->available = FALSE;

	g_warning ("TinyTinyRSS login failed: error '%s'!\n", msg);

	if (++source->authFailures < TTRSS_SOURCE_MAX_AUTH_FAILURES)
		source->loginState = TTRSS_SOURCE_STATE_NONE;
	else
		source->loginState = TTRSS_SOURCE_STATE_NO_AUTH;
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

		source->loginState = TTRSS_SOURCE_STATE_ACTIVE;

		if (!(flags & TTRSS_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (subscription, flags);
	} else {
		ttrss_source_set_login_error (source, g_strdup_printf ("No session_id found in response!\n%s", result->data));
	}
	
	g_object_unref (parser);
}

/**
 * Perform a login to tt-rss, if the login completes the ttrssSource will 
 * have a valid sid and will have loginStatus TTRSS_SOURCE_LOGIN_ACTIVE.
 */
void
ttrss_source_login (ttrssSourcePtr source, guint32 flags) 
{ 
	gchar			*username, *password, *source_uri;
	updateRequestPtr	request;
	subscriptionPtr		subscription = source->root->subscription;
	
	if (source->loginState != TTRSS_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session doesn't expire. */
		debug1 (DEBUG_UPDATE, "Logging in while login state is %d", source->loginState);
	}

	request = update_request_new ();

	/* escape user and password for JSON call */
	username = g_strescape (subscription->updateOptions->username, NULL);
	password = g_strescape (subscription->updateOptions->password, NULL);

	source->url = metadata_list_get (subscription->metadata, "ttrss-url");
	if (!source->url) {
		ttrss_source_set_login_error (source, g_strdup ("Fatal: We've lost the TinyTinyRSS server URL! Please re-subscribe!"));
		return;
	}

	source_uri = g_strdup_printf (TTRSS_URL, source->url);
	update_request_set_source (request, source_uri);
	g_free (source_uri);

	request->options = update_options_copy (subscription->updateOptions);
	request->postdata = g_strdup_printf (TTRSS_JSON_LOGIN, username, password);

	g_free (username);
	g_free (password);

	source->loginState = TTRSS_SOURCE_STATE_IN_PROGRESS ;

	update_execute_request (source, request, ttrss_source_login_cb, source, flags);
}

/* node source type implementation */

static void
ttrss_source_update (nodePtr node)
{
	debug0 (DEBUG_UPDATE, "ttrss_source_update()");
	subscription_update (node->subscription, 0);
}

static void
ttrss_source_auto_update (nodePtr node)
{
	ttrssSourcePtr	source = (ttrssSourcePtr) node->data;

	if (source->loginState == TTRSS_SOURCE_STATE_IN_PROGRESS) 
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
ttrss_source_set_subscription_type (nodePtr folder)
{
	GSList *iter;

	for (iter = folder->children; iter; iter = g_slist_next(iter)) {
		nodePtr node = (nodePtr) iter->data;

		if (node->subscription)
			node->subscription->type = &ttrssSourceFeedSubscriptionType;
		else
			ttrss_source_set_subscription_type (node);
	}
}

static void
ttrss_source_import (nodePtr node)
{
	opml_source_import (node);

	node->subscription->updateInterval = -1;
	node->subscription->type = &ttrssSourceSubscriptionType;
	if (!node->data)
		node->data = (gpointer) ttrss_source_new (node);

	ttrss_source_set_subscription_type (node);
}

static void
ttrss_source_export (nodePtr node)
{
	opml_source_export (node);
}

static gchar *
ttrss_source_get_feedlist (nodePtr node)
{
	return opml_source_get_feedlist (node);
}

static void 
ttrss_source_remove (nodePtr node)
{ 
	opml_source_remove (node);
}

/* GUI callbacks */

static void
on_ttrss_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data) 
{
	if (response_id == GTK_RESPONSE_OK) {
		nodePtr		node;
		subscriptionPtr subscription = subscription_new ("", NULL, NULL);
		
		/* This is a bit ugly: we need to prevent the tt-rss base
		   URL from being lost by unwanted permanent redirects on
		   the getFeeds call, so we save it as the homepage meta
		   data value... */
		metadata_list_set (&subscription->metadata, "ttrss-url", gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "serverUrlEntry"))));

		node = node_new (node_source_get_node_type ());
		node_set_title (node, "Tiny Tiny RSS");
		node_source_new (node, ttrss_source_get_type ());
		node_set_subscription (node, subscription);
		
		subscription_set_auth_info (subscription,
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))),
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));
		subscription->type = &ttrssSourceSubscriptionType;		

		node->data = (gpointer)ttrss_source_new (node);
		feedlist_node_added (node);
		ttrss_source_update (node);
		db_node_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ui_ttrss_source_get_account_info (void)
{
	GtkWidget	*dialog;

	
	dialog = liferea_dialog_new ("ttrss_source.ui", "ttrss_source_dialog");
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_ttrss_source_selected), 
			  NULL);
}

static void
ttrss_source_cleanup (nodePtr node)
{
	ttrssSourcePtr source = (ttrssSourcePtr) node->data;
	g_hash_table_destroy (source->categories);
	g_hash_table_destroy (source->categoryNodes);
	ttrss_source_free (source);
	node->data = NULL;
}

static void
ttrss_source_remote_update_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	debug2 (DEBUG_UPDATE, "TinyTinyRSS result processing... status:%d >>>%s<<<", result->httpstatus, result->data);
}

/* FIXME: Only simple synchronous item change requests... Get async! */

static void 
ttrss_source_item_set_flag (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr			root = node_source_root_from_node (node);
	ttrssSourcePtr		source = (ttrssSourcePtr)root->data;
	updateRequestPtr	request;

	request = update_request_new ();
	request->options = update_options_copy (root->subscription->updateOptions);
	request->postdata = g_strdup_printf (TTRSS_JSON_UPDATE_ITEM_FLAG, source->session_id, item_get_id(item), newStatus?1:0 );

	update_request_set_source (request, g_strdup_printf (TTRSS_URL, source->url));
	update_execute_request (source, request, ttrss_source_remote_update_cb, source, 0 /* flags */);

	item_flag_state_changed (item, newStatus);
}

static void
ttrss_source_item_mark_read (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr			root = node_source_root_from_node (node);
	ttrssSourcePtr		source = (ttrssSourcePtr)root->data;
	updateRequestPtr	request;

	request = update_request_new ();
	request->options = update_options_copy (root->subscription->updateOptions);
	request->postdata = g_strdup_printf (TTRSS_JSON_UPDATE_ITEM_UNREAD, source->session_id, item_get_id(item), newStatus?0:1 );

	update_request_set_source (request, g_strdup_printf (TTRSS_URL, source->url));
	update_execute_request (source, request, ttrss_source_remote_update_cb, source, 0 /* flags */);

	item_read_state_changed (item, newStatus);
}

/* node source type definition */

static struct nodeSourceType nst = {
	.id                  = "fl_ttrss",
	.name                = N_("Tiny Tiny RSS"),
	.description         = N_("Integrate the feed list of your Tiny Tiny RSS 1.5+ account. Liferea will "
	   "present your tt-rss subscriptions, and will synchronize your feed list and reading lists."),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC,
	.source_type_init    = ttrss_source_init,
	.source_type_deinit  = ttrss_source_deinit,
	.source_new          = ui_ttrss_source_get_account_info,
	.source_delete       = ttrss_source_remove,
	.source_import       = ttrss_source_import,
	.source_export       = ttrss_source_export,
	.source_get_feedlist = ttrss_source_get_feedlist,
	.source_update       = ttrss_source_update,
	.source_auto_update  = ttrss_source_auto_update,
	.free                = ttrss_source_cleanup,
	.item_set_flag       = ttrss_source_item_set_flag,
	.item_mark_read      = ttrss_source_item_mark_read,
	.add_folder          = NULL,	/* not supported by current tt-rss JSON API (v1.5) */
	.add_subscription    = NULL,	/* not supported by current tt-rss JSON API (v1.5) */
	.remove_node         = NULL,	/* not supported by current tt-rss JSON API (v1.5) */
	.convert_to_local    = NULL	/* FIXME: implement me to allow data migration from tt-rss! */
};

nodeSourceTypePtr
ttrss_source_get_type (void)
{
	return &nst;
}
