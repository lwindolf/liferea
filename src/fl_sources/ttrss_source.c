/**
 * @file ttrss_source.c  tt-rss feed list source support
 * 
 * Copyright (C) 2010-2012 Lars Lindner <lars.lindner@gmail.com>
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
#include "ui/auth_dialog.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_common.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"

// FIXME: Avoid doing requests when we are not logged in yet!

/** default tt-rss subscription list update interval = once a day */
#define TTRSS_SOURCE_UPDATE_INTERVAL 60*60*24

/** create a tt-rss source with given node as root */ 
static ttrssSourcePtr
ttrss_source_new (nodePtr node) 
{
	ttrssSourcePtr source = g_new0 (struct ttrssSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new (); 
	source->loginState = TTRSS_SOURCE_STATE_NONE; 
	
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
ttrss_source_get_config_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	ttrssSourcePtr	source = (ttrssSourcePtr) userdata;
	subscriptionPtr subscription = source->root->subscription;

	debug1 (DEBUG_UPDATE, "tt-rss getConfig processing... >>>%s<<<", result->data);

	/* We expect something like:

		 {"icons_dir":"icons","icons_url":"icons","daemon_is_running":true,"num_feeds":71}

	   And are only interested in the "daemon_is_running" value... */
	
	if (result->data && result->httpstatus == 200) {
		JsonParser *parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonNode *node = json_parser_get_root (parser);
			const gchar *result = json_get_string (node, "daemon_is_running");
			if (result && g_str_equal ("true", result)) {
				source->selfUpdating = TRUE;
				debug0 (DEBUG_UPDATE, "This tt-rss source is self-updating!");
			} else {
				debug0 (DEBUG_UPDATE, "This tt-rss source is not self-updating!");
			}

			g_object_unref (parser);
		}
	}

	/* now that we are authenticated and know the config trigger updating to start data retrieval */
	source->loginState = TTRSS_SOURCE_STATE_ACTIVE;

	if (!(flags & TTRSS_SOURCE_UPDATE_ONLY_LOGIN) && !source->selfUpdating)
		subscription_update (subscription, flags);
}

static void
ttrss_source_login_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	ttrssSourcePtr	source = (ttrssSourcePtr) userdata;
	subscriptionPtr subscription = source->root->subscription;
		
	debug1 (DEBUG_UPDATE, "tt-rss login processing... >>>%s<<<", result->data);
	
	g_assert (!source->session_id);
	
	if (result->data && result->httpstatus == 200) {
		JsonParser *parser = json_parser_new ();
		
		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonNode *node = json_parser_get_root (parser);

			if (json_get_string (node, "error"))
				g_warning ("tt-rss login failed: error '%s'!\n", json_get_string (node, "error"));
			
			source->session_id = g_strdup (json_get_string (json_get_node (node, "content"), "session_id"));
			if (source->session_id) {
				debug1 (DEBUG_UPDATE, "Found session_id: >>>%s<<<!", source->session_id);
			} else {
				g_warning ("No tt-rss session_id found in response!\n%s", result->data);
			}
			
			g_object_unref (parser);
		} else {
			g_warning ("Invalid JSON returned on tt-rss login! >>>%s<<<", result->data);
		}
	} else {
		g_warning ("tt-rss login failed: HTTP %d\n", result->httpstatus);
		source->root->available = FALSE;
	}

	if (source->session_id) {
		updateRequestPtr request;

		/* Check for remote update daemon running. This needs to be known
		   before we start updating to decide wether to actively update
		   remote feeds or just fetch them. */
		request = update_request_new ();
		request->options = update_options_copy (subscription->updateOptions);

		update_request_set_source (request, g_strdup_printf (TTRSS_GET_CONFIG, metadata_list_get (subscription->metadata, "ttrss-url"), source->session_id));
		update_execute_request (source, request, ttrss_source_get_config_cb, source, flags);
	}
}

/**
 * Perform a login to tt-rss, if the login completes the ttrssSource will 
 * have a valid sid and will have loginStatus TTRSS_SOURCE_LOGIN_ACTIVE.
 */
void
ttrss_source_login (ttrssSourcePtr source, guint32 flags) 
{ 
	gchar			*username, *password;
	updateRequestPtr	request;
	subscriptionPtr		subscription = source->root->subscription;
	
	if (source->loginState != TTRSS_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session doesn't expire. */
		debug1(DEBUG_UPDATE, "Logging in while login state is %d", source->loginState);
	}

	request = update_request_new ();

	/* escape user and password as both are passed using an URI */
	username = g_uri_escape_string (subscription->updateOptions->username, NULL, TRUE);
	password = g_uri_escape_string (subscription->updateOptions->password, NULL, TRUE);
	
	update_request_set_source (request, g_strdup_printf (TTRSS_LOGIN_URL, metadata_list_get (subscription->metadata, "ttrss-url"), username, password));

	request->options = update_options_copy (subscription->updateOptions);
	
	g_free (username);
	g_free (password);

	source->loginState = TTRSS_SOURCE_STATE_IN_PROGRESS ;

	update_execute_request (source, request, ttrss_source_login_cb, source, flags);
}

/* node source type implementation */

static void
ttrss_source_update (nodePtr node)
{
	debug0(DEBUG_UPDATE, "ttrss_source_update()");
	subscription_update (node->subscription, 0);
}

static void
ttrss_source_auto_update (nodePtr node)
{
	GTimeVal	now;
	ttrssSourcePtr	source = (ttrssSourcePtr) node->data;

	if (source->loginState == TTRSS_SOURCE_STATE_IN_PROGRESS) 
		return; /* the update will start automatically anyway */

	g_get_current_time (&now);

	ttrss_source_update (node);
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
	GSList *iter; 
	opml_source_import (node);
	
	node->subscription->type = &ttrssSourceSubscriptionType;
	if (!node->data)
		node->data = (gpointer) ttrss_source_new (node);

	for (iter = node->children; iter; iter = g_slist_next(iter))
		((nodePtr) iter->data)->subscription->type = &ttrssSourceFeedSubscriptionType;
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
		
		/* The is a bit ugly: we need to prevent the tt-rss base
		   URL from being lost by unwanted permanent redirects on
		   the getFeeds call, so we save it as the homepage meta
		   data value... */
		metadata_list_set (&subscription->metadata, "ttrss-url", gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "serverUrlEntry"))));
		
		subscription_set_auth_info (subscription,
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))),
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));
		subscription->type = &ttrssSourceSubscriptionType;
		
		node = node_new (node_source_get_node_type ());
		node_set_title (node, "Tiny Tiny RSS");
		node_source_new (node, ttrss_source_get_type ());
		node_set_subscription (node, subscription);
		node->data = ttrss_source_new (node);
		feedlist_node_added (node);
		ttrss_source_update (node);
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
	ttrss_source_free (source);
	node->data = NULL;
}

static void
ttrss_source_remote_update_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	debug2 (DEBUG_UPDATE, "tt-rss result processing... status:%d >>>%s<<<", result->httpstatus, result->data);
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

	update_request_set_source (request, g_strdup_printf (TTRSS_UPDATE_ITEM_FLAG,
	                                       metadata_list_get (root->subscription->metadata, "ttrss-url"),
	                                       source->session_id, 
	                                       item_get_id (item), newStatus?1:0));

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

	update_request_set_source (request, g_strdup_printf (TTRSS_UPDATE_ITEM_UNREAD,
	                                       metadata_list_get (root->subscription->metadata, "ttrss-url"),
	                                       source->session_id, 
	                                       item_get_id (item), newStatus?0:1));

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
	.remove_node         = NULL	/* not supported by current tt-rss JSON API (v1.5) */
};

nodeSourceTypePtr
ttrss_source_get_type (void)
{
	return &nst;
}
