/**
 * @file theoldreader_source.c  TheOldReader feed list source support
 * 
 * Copyright (C) 2007-2013 Lars Windolf <lars.lindner@gmail.com>
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2011 Peter Oliver
 * Copyright (C) 2011 Sergey Snitsaruk <narren96c@gmail.com>
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

#include "fl_sources/theoldreader_source.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <libxml/xpath.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "item_state.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "xml.h"
#include "ui/auth_dialog.h"
#include "ui/liferea_dialog.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/theoldreader_source_edit.h"
#include "fl_sources/theoldreader_source_feed_list.h"

/** default TheOldReader subscription list update interval = once a day */
#define THEOLDREADER_SOURCE_UPDATE_INTERVAL 60*60*24

/** create a source with given node as root */ 
static TheOldReaderSourcePtr
theoldreader_source_new (nodePtr node) 
{
	TheOldReaderSourcePtr source = g_new0 (struct TheOldReaderSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new (); 
	source->loginState = THEOLDREADER_SOURCE_STATE_NONE; 
	source->lastTimestampMap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	
	return source;
}

static void
theoldreader_source_free (TheOldReaderSourcePtr source) 
{
	if (!source)
		return;

	update_job_cancel_by_owner (source);
	
	g_free (source->authHeaderValue);
	g_queue_free (source->actionQueue) ;
	g_hash_table_unref (source->lastTimestampMap);
	g_free (source);
}

static void
theoldreader_source_login_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	TheOldReaderSourcePtr	source = (TheOldReaderSourcePtr) userdata;
	gchar			*tmp = NULL;
	subscriptionPtr 	subscription = source->root->subscription;
		
	debug1 (DEBUG_UPDATE, "TheOldReader login processing... %s", result->data);
	
	g_assert (!source->authHeaderValue);
	
	if (result->data && result->httpstatus == 200)
		tmp = strstr (result->data, "Auth=");
		
	if (tmp) {
		gchar *ttmp = tmp; 
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		source->authHeaderValue = g_strdup_printf ("GoogleLogin auth=%s", ttmp + 5);

		debug1 (DEBUG_UPDATE, "TheOldReader Auth token found: %s", source->authHeaderValue);

		source->loginState = THEOLDREADER_SOURCE_STATE_ACTIVE;
		source->authFailures = 0;

		/* now that we are authenticated trigger updating to start data retrieval */
		if (!(flags & THEOLDREADER_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (subscription, flags);

		/* process any edits waiting in queue */
		theoldreader_source_edit_process (source);

	} else {
		debug0 (DEBUG_UPDATE, "TheOldReader login failed! no Auth token found in result!");
		subscription->node->available = FALSE;

		g_free (subscription->updateError);
		subscription->updateError = g_strdup (_("TheOldReader login failed!"));
		source->authFailures++;

		if (source->authFailures < THEOLDREADER_SOURCE_MAX_AUTH_FAILURES)
			source->loginState = THEOLDREADER_SOURCE_STATE_NONE;
		else
			source->loginState = THEOLDREADER_SOURCE_STATE_NO_AUTH;
		
		auth_dialog_new (subscription, flags);
	}
}

/**
 * Perform a login to TheOldReader, if the login completes the 
 * TheOldReaderSource will have a valid Auth token and will have loginStatus to 
 * THEOLDREADER_SOURCE_LOGIN_ACTIVE.
 */
void
theoldreader_source_login (TheOldReaderSourcePtr source, guint32 flags) 
{ 
	gchar			*username, *password;
	updateRequestPtr	request;
	subscriptionPtr		subscription = source->root->subscription;
	
	if (source->loginState != THEOLDREADER_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session
		 * doesn't expire. */
		debug1(DEBUG_UPDATE, "Logging in while login state is %d\n", source->loginState);
	}

	request = update_request_new ();

	update_request_set_source (request, THEOLDREADER_READER_LOGIN_URL);

	/* escape user and password as both are passed using an URI */
	username = g_uri_escape_string (subscription->updateOptions->username, NULL, TRUE);
	password = g_uri_escape_string (subscription->updateOptions->password, NULL, TRUE);

	request->postdata = g_strdup_printf (THEOLDREADER_READER_LOGIN_POST, username, password);
	request->options = update_options_copy (subscription->updateOptions);
	
	g_free (username);
	g_free (password);

	source->loginState = THEOLDREADER_SOURCE_STATE_IN_PROGRESS ;

	update_execute_request (source, request, theoldreader_source_login_cb, source, flags);
}

/* node source type implementation */

static void
theoldreader_source_update (nodePtr node)
{
	TheOldReaderSourcePtr source = (TheOldReaderSourcePtr) node->data;

	/* Reset THEOLDREADER_SOURCE_STATE_NO_AUTH as this is a manual
	   user interaction and no auto-update so we can query
	   for credentials again. */
	if (source->loginState == THEOLDREADER_SOURCE_STATE_NO_AUTH)
		source->loginState = THEOLDREADER_SOURCE_STATE_NONE;

	subscription_update (node->subscription, 0);
}

static void
theoldreader_source_auto_update (nodePtr node)
{
	GTimeVal	now;
	TheOldReaderSourcePtr source = (TheOldReaderSourcePtr) node->data;

	if (source->loginState == THEOLDREADER_SOURCE_STATE_NONE) {
		theoldreader_source_update (node);
		return;
	}

	if (source->loginState == THEOLDREADER_SOURCE_STATE_IN_PROGRESS) 
		return; /* the update will start automatically anyway */

	g_get_current_time (&now);
	
	/* do daily updates for the feed list and feed updates according to the default interval */
/*	if (node->subscription->updateState->lastPoll.tv_sec + THEOLDREADER_SOURCE_UPDATE_INTERVAL <= now.tv_sec) {
		subscription_update (node->subscription, 0);
		g_get_current_time (&source->lastQuickUpdate);
	}
	else if (source->lastQuickUpdate.tv_sec + THEOLDREADER_SOURCE_QUICK_UPDATE_INTERVAL <= now.tv_sec) {
		theoldreader_source_opml_quick_update (source);
		theoldreader_source_edit_process (source);
		g_get_current_time (&source->lastQuickUpdate);
	}*/

	// FIXME: Don't do below, but above logic!
	if (source->lastQuickUpdate.tv_sec + THEOLDREADER_SOURCE_QUICK_UPDATE_INTERVAL <= now.tv_sec) {
		subscription_update (node->subscription, 0);
		g_get_current_time (&source->lastQuickUpdate);
	}
}

static
void theoldreader_source_init (void)
{
	metadata_type_register ("theoldreader-feed-id", METADATA_TYPE_TEXT);
}

static void theoldreader_source_deinit (void) { }

static void
theoldreader_source_import (nodePtr node)
{
	opml_source_import (node);
	
	node->subscription->type = &theOldReaderSourceOpmlSubscriptionType;
	if (!node->data)
		node->data = (gpointer) theoldreader_source_new (node);
}

static nodePtr
theoldreader_source_add_subscription (nodePtr node, subscriptionPtr subscription) 
{ 
	debug_enter ("theoldreader_source_add_subscription");
	nodePtr child = node_new (feed_get_node_type ());

	debug0 (DEBUG_UPDATE, "TheOldReaderSource: Adding a new subscription"); 
	node_set_data (child, feed_new ());

	node_set_subscription (child, subscription);
	child->subscription->type = &theOldReaderSourceFeedSubscriptionType;
	
	node_set_title (child, _("New Subscription"));

	theoldreader_source_edit_add_subscription (node_source_root_from_node (node)->data, subscription->source);
	
	debug_exit ("theoldreader_source_add_subscription");
	
	return child;
}

static void
theoldreader_source_remove_node (nodePtr node, nodePtr child) 
{ 
	gchar           	*url; 
	TheOldReaderSourcePtr	source = (TheOldReaderSourcePtr) node->data;
	
	if (child == node) { 
		feedlist_node_removed (child);
		return; 
	}

	url = g_strdup (child->subscription->source);

	feedlist_node_removed (child);

	/* propagate the removal only if there aren't other copies */
	if (!theoldreader_source_opml_get_node_by_source (source, url)) 
		theoldreader_source_edit_remove_subscription (source, url);
	
	g_free (url);
}

/* GUI callbacks */

static void
on_theoldreader_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data) 
{
	nodePtr		node;
	subscriptionPtr	subscription;

	if (response_id == GTK_RESPONSE_OK) {
		subscription = subscription_new ("http://theoldreader.com/reader", NULL, NULL);
		node = node_new (node_source_get_node_type ());
		node_set_title (node, "TheOldReader");
		node_source_new (node, theoldreader_source_get_type ());
		node_set_subscription (node, subscription);

		subscription_set_auth_info (subscription,
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))),
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));

		subscription->type = &theOldReaderSourceOpmlSubscriptionType ; 

		node->data = theoldreader_source_new (node);
		feedlist_node_added (node);
		theoldreader_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ui_theoldreader_source_get_account_info (void)
{
	GtkWidget	*dialog;
	
	dialog = liferea_dialog_new ("theoldreader_source.ui", "theoldreader_source_dialog");
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_theoldreader_source_selected), 
			  NULL);
}

static void
theoldreader_source_cleanup (nodePtr node)
{
	TheOldReaderSourcePtr reader = (TheOldReaderSourcePtr) node->data;
	theoldreader_source_free(reader);
	node->data = NULL;
}

static void 
theoldreader_source_item_set_flag (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr root = node_source_root_from_node (node);
	theoldreader_source_edit_mark_starred ((TheOldReaderSourcePtr)root->data, item->sourceId, node->subscription->source, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
theoldreader_source_item_mark_read (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr root = node_source_root_from_node (node);
	theoldreader_source_edit_mark_read ((TheOldReaderSourcePtr)root->data, item->sourceId, node->subscription->source, newStatus);
	item_read_state_changed (item, newStatus);
}

/**
 * Convert all subscriptions of a google source to local feeds
 *
 * @param node The node to migrate (not the nodeSource!)
 */
static void
theoldreader_source_convert_to_local (nodePtr node)
{
	TheOldReaderSourcePtr source = node->data; 

	source->loginState = THEOLDREADER_SOURCE_STATE_MIGRATE;	
}

/* node source type definition */

static struct nodeSourceType nst = {
	.id                  = "fl_theoldreader",
	.name                = N_("TheOldReader"),
	.description         = N_("Integrate the feed list of your TheOldReader account. Liferea will "
	                          "present your TheOldReader subscriptions, and will synchronize your feed list and reading lists."),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION | 
	                       NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
	                       NODE_SOURCE_CAPABILITY_ADD_FEED |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
	                       NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL,
	.subscriptionType    = &theOldReaderSourceFeedSubscriptionType,
	.source_type_init    = theoldreader_source_init,
	.source_type_deinit  = theoldreader_source_deinit,
	.source_new          = ui_theoldreader_source_get_account_info,
	.source_delete       = opml_source_remove,
	.source_import       = theoldreader_source_import,
	.source_export       = opml_source_export,
	.source_get_feedlist = opml_source_get_feedlist,
	.source_update       = theoldreader_source_update,
	.source_auto_update  = theoldreader_source_auto_update,
	.free                = theoldreader_source_cleanup,
	.item_set_flag       = theoldreader_source_item_set_flag,
	.item_mark_read      = theoldreader_source_item_mark_read,
	.add_folder          = NULL, 
	.add_subscription    = theoldreader_source_add_subscription,
	.remove_node         = theoldreader_source_remove_node,
	.convert_to_local    = theoldreader_source_convert_to_local
};

nodeSourceTypePtr
theoldreader_source_get_type (void)
{
	return &nst;
}
