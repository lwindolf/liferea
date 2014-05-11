/**
 * @file inoreader_source.c  InoReader source support
 * 
 * Copyright (C) 2007-2014 Lars Windolf <lars.lindner@gmail.com>
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

#include "fl_sources/inoreader_source.h"

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
#include "fl_sources/inoreader_source_edit.h"
#include "fl_sources/inoreader_source_feed_list.h"

/** default reader subscription list update interval = once a day */
#define INOREADER_SOURCE_UPDATE_INTERVAL 60*60*24

/** create a source with given node as root */ 
static InoreaderSourcePtr
inoreader_source_new (nodePtr node) 
{
	InoreaderSourcePtr source = g_new0 (struct InoreaderSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new (); 
	source->loginState = INOREADER_SOURCE_STATE_NONE; 
	source->lastTimestampMap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	
	return source;
}

static void
inoreader_source_free (InoreaderSourcePtr gsource) 
{
	if (!gsource)
		return;

	update_job_cancel_by_owner (gsource);
	
	g_free (gsource->authHeaderValue);
	g_queue_free (gsource->actionQueue) ;
	g_hash_table_unref (gsource->lastTimestampMap);
	g_free (gsource);
}

static void
inoreader_source_login_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	InoreaderSourcePtr	gsource = (InoreaderSourcePtr) userdata;
	gchar		*tmp = NULL;
	subscriptionPtr subscription = gsource->root->subscription;
		
	debug1 (DEBUG_UPDATE, "InoReader login processing... %s", result->data);
	
	g_assert (!gsource->authHeaderValue);
	
	if (result->data && result->httpstatus == 200)
		tmp = strstr (result->data, "Auth=");
		
	if (tmp) {
		gchar *ttmp = tmp; 
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		gsource->authHeaderValue = g_strdup_printf ("GoogleLogin auth=%s", ttmp + 5);

		debug1 (DEBUG_UPDATE, "InoReader Auth token found: %s", gsource->authHeaderValue);

		gsource->loginState = INOREADER_SOURCE_STATE_ACTIVE;
		gsource->authFailures = 0;

		/* now that we are authenticated trigger updating to start data retrieval */
		if (!(flags & INOREADER_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (subscription, flags);

		/* process any edits waiting in queue */
		inoreader_source_edit_process (gsource);

	} else {
		debug0 (DEBUG_UPDATE, "InoReader login failed! no Auth token found in result!");
		subscription->node->available = FALSE;

		g_free (subscription->updateError);
		subscription->updateError = g_strdup (_("InoReader login failed!"));
		gsource->authFailures++;

		if (gsource->authFailures < INOREADER_SOURCE_MAX_AUTH_FAILURES)
			gsource->loginState = INOREADER_SOURCE_STATE_NONE;
		else
			gsource->loginState = INOREADER_SOURCE_STATE_NO_AUTH;
		
		auth_dialog_new (subscription, flags);
	}
}

/**
 * Perform a login to InoReader, if the login completes the 
 * InoreaderSource will have a valid Auth token and will have loginStatus to 
 * INOREADER_SOURCE_LOGIN_ACTIVE.
 */
void
inoreader_source_login (InoreaderSourcePtr gsource, guint32 flags) 
{ 
	gchar			*username, *password;
	updateRequestPtr	request;
	subscriptionPtr		subscription = gsource->root->subscription;
	
	if (gsource->loginState != INOREADER_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session
		 * doesn't expire. */
		debug1(DEBUG_UPDATE, "Logging in while login state is %d\n", gsource->loginState);
	}

	request = update_request_new ();

	update_request_set_source (request, INOREADER_LOGIN_URL);

	/* escape user and password as both are passed using an URI */
	username = g_uri_escape_string (subscription->updateOptions->username, NULL, TRUE);
	password = g_uri_escape_string (subscription->updateOptions->password, NULL, TRUE);

	request->postdata = g_strdup_printf (INOREADER_LOGIN_POST, username, password);
	request->options = update_options_copy (subscription->updateOptions);
	
	g_free (username);
	g_free (password);

	gsource->loginState = INOREADER_SOURCE_STATE_IN_PROGRESS ;

	update_execute_request (gsource, request, inoreader_source_login_cb, gsource, flags);
}

/* node source type implementation */

static void
inoreader_source_update (nodePtr node)
{
	InoreaderSourcePtr source = (InoreaderSourcePtr) node->data;

	debug0 (DEBUG_UPDATE, "inoreader_source_update()");

	/* Reset INOREADER_SOURCE_STATE_NO_AUTH as this is a manual
	   user interaction and no auto-update so we can query
	   for credentials again. */
	if (source->loginState == INOREADER_SOURCE_STATE_NO_AUTH)
		source->loginState = INOREADER_SOURCE_STATE_NONE;

	subscription_update (node->subscription, 0);
}

static void
inoreader_source_auto_update (nodePtr node)
{
	GTimeVal	now;
	InoreaderSourcePtr source = (InoreaderSourcePtr) node->data;

	if (source->loginState == INOREADER_SOURCE_STATE_NONE) {
		inoreader_source_update (node);
		return;
	}

	if (source->loginState == INOREADER_SOURCE_STATE_IN_PROGRESS) 
		return; /* the update will start automatically anyway */

	debug0 (DEBUG_UPDATE, "inoreader_source_auto_update()");

	g_get_current_time (&now);
	
	/* do daily updates for the feed list and feed updates according to the default interval */
	if (node->subscription->updateState->lastPoll.tv_sec + INOREADER_SOURCE_UPDATE_INTERVAL <= now.tv_sec) {
		subscription_update (node->subscription, 0);
		g_get_current_time (&source->lastQuickUpdate);
	}
	else if (source->lastQuickUpdate.tv_sec + INOREADER_SOURCE_QUICK_UPDATE_INTERVAL <= now.tv_sec) {
		inoreader_source_opml_quick_update (source);
		inoreader_source_edit_process (source);
		g_get_current_time (&source->lastQuickUpdate);
	}
}

static void
inoreader_source_init (void)
{
	metadata_type_register ("inoreader-feed-id", METADATA_TYPE_TEXT);
}

static void inoreader_source_deinit (void) { }

static void
inoreader_source_import (nodePtr node)
{
	opml_source_import (node);
	
	node->subscription->updateInterval = -1;
	node->subscription->type = node->source->type->sourceSubscriptionType;
	if (!node->data)
		node->data = (gpointer) inoreader_source_new (node);
}

static nodePtr
inoreader_source_add_subscription (nodePtr node, subscriptionPtr subscription) 
{ 
	debug_enter ("inoreader_source_add_subscription");
	nodePtr child = node_new (feed_get_node_type ());

	debug0 (DEBUG_UPDATE, "InoreaderSource: Adding a new subscription"); 
	node_set_data (child, feed_new ());

	node_set_subscription (child, subscription);
	child->subscription->type = node->source->type->feedSubscriptionType;
	
	node_set_title (child, _("New Subscription"));

	inoreader_source_edit_add_subscription (node_source_root_from_node (node)->data, subscription->source);
	
	debug_exit ("inoreader_source_add_subscription");
	
	return child;
}

static void
inoreader_source_remove_node (nodePtr node, nodePtr child) 
{ 
	gchar           *source; 
	InoreaderSourcePtr gsource = node->data;
	
	if (child == node) { 
		feedlist_node_removed (child);
		return; 
	}

	source = g_strdup (child->subscription->source);

	feedlist_node_removed (child);

	/* propagate the removal only if there aren't other copies */
	if (!inoreader_source_opml_get_node_by_source (gsource, source)) 
		inoreader_source_edit_remove_subscription (gsource, source);
	
	g_free (source);
}

/* GUI callbacks */

static void
on_inoreader_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data) 
{
	nodePtr		node;

	if (response_id == GTK_RESPONSE_OK) {
		node = node_new (node_source_get_node_type ());
		node_source_new (node, inoreader_source_get_type (), "http://www.inoreader.com/reader");

		subscription_set_auth_info (node->subscription,
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))),
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));

		node->data = inoreader_source_new (node);
		feedlist_node_added (node);
		inoreader_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ui_inoreader_source_get_account_info (void)
{
	GtkWidget	*dialog;
	
	dialog = liferea_dialog_new ("inoreader_source.ui", "inoreader_source_dialog");
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_inoreader_source_selected), 
			  NULL);
}

static void
inoreader_source_cleanup (nodePtr node)
{
	InoreaderSourcePtr reader = (InoreaderSourcePtr) node->data;
	inoreader_source_free(reader);
	node->data = NULL ;
}

static void 
inoreader_source_item_set_flag (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr root = node_source_root_from_node (node);
	inoreader_source_edit_mark_starred ((InoreaderSourcePtr)root->data, item->sourceId, node->subscription->source, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
inoreader_source_item_mark_read (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr root = node_source_root_from_node (node);
	inoreader_source_edit_mark_read ((InoreaderSourcePtr)root->data, item->sourceId, node->subscription->source, newStatus);
	item_read_state_changed (item, newStatus);
}

/**
 * Convert all subscriptions of a google source to local feeds
 *
 * @param node The node to migrate (not the nodeSource!)
 */
static void
inoreader_source_convert_to_local (nodePtr node)
{
	InoreaderSourcePtr gsource = node->data; 

	gsource->loginState = INOREADER_SOURCE_STATE_MIGRATE;	
}

/* node source type definition */

extern struct subscriptionType inoreaderSourceFeedSubscriptionType;
extern struct subscriptionType inoreaderSourceOpmlSubscriptionType;

static struct nodeSourceType nst = {
	.id                  = "fl_inoreader",
	.name                = N_("InoReader"),
	.description         = N_("InoReader is a free online aggregator (http://inoreader.com)."),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION | 
	                       NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
	                       NODE_SOURCE_CAPABILITY_ADD_FEED |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
	                       NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL,
	.feedSubscriptionType = &inoreaderSourceFeedSubscriptionType,
	.sourceSubscriptionType = &inoreaderSourceOpmlSubscriptionType,
	.source_type_init    = inoreader_source_init,
	.source_type_deinit  = inoreader_source_deinit,
	.source_new          = ui_inoreader_source_get_account_info,
	.source_delete       = opml_source_remove,
	.source_import       = inoreader_source_import,
	.source_export       = opml_source_export,
	.source_get_feedlist = opml_source_get_feedlist,
	.source_update       = inoreader_source_update,
	.source_auto_update  = inoreader_source_auto_update,
	.free                = inoreader_source_cleanup,
	.item_set_flag       = inoreader_source_item_set_flag,
	.item_mark_read      = inoreader_source_item_mark_read,
	.add_folder          = NULL, 
	.add_subscription    = inoreader_source_add_subscription,
	.remove_node         = inoreader_source_remove_node,
	.convert_to_local    = inoreader_source_convert_to_local
};

nodeSourceTypePtr
inoreader_source_get_type (void)
{
	return &nst;
}
