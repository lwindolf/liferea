/**
 * @file google_source.c  Google reader feed list source support
 * 
 * Copyright (C) 2007-2008 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
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

#include "fl_sources/google_source.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <libxml/xpath.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "item_state.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "xml.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_common.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/google_source_edit.h"

/** default Google reader subscription list update interval = once a day */
#define GOOGLE_SOURCE_UPDATE_INTERVAL 60*60*24

/** create a google source with given node as root */ 
GoogleSourcePtr google_source_new(nodePtr node) 
{
	GoogleSourcePtr source = g_new0(struct GoogleSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new(); 
	source->loginState = GOOGLE_SOURCE_STATE_NONE; 
	source->lastTimestampMap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	return source;
}


void google_source_free(GoogleSourcePtr gsource) 
{
	if (!gsource) return ;

	g_queue_free(gsource->actionQueue) ;
	g_hash_table_unref (gsource->lastTimestampMap);
	g_free(gsource);
}


nodePtr
google_source_get_root_from_node (nodePtr node)
{ 
	while (node->parent->source == node->source) 
		node = node->parent;
	return node;
}

static void
google_source_login_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr) userdata;
	gchar		*tmp = NULL;
	GTimeVal	now;
	subscriptionPtr subscription = gsource->root->subscription;
		
	debug0 (DEBUG_UPDATE, "google login processing...");
	
	if (result->returncode != 0) {
		debug0(DEBUG_UPDATE, "GoogleSource: Unable to login, perhaps wrong details?");
		/* @todo Need to inform the user through UI */
		return;
	}
	
	g_assert (!gsource->sid);
	
	if (result->data)
		tmp = strstr (result->data, "SID=");
		
	if (tmp) {
		gchar *ttmp = tmp; 
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		gsource->sid = g_strdup (ttmp);

		debug1 (DEBUG_UPDATE, "google reader SID found: %s", gsource->sid);
		/* now that we are authenticated trigger updating to start data retrieval */
		gsource->loginState = GOOGLE_SOURCE_STATE_ACTIVE;
		if ( ! (flags & GOOGLE_SOURCE_UPDATE_ONLY_LOGIN) ) 
			subscription_update (subscription, flags);

		/* process any edits waiting in queue */
		google_source_edit_process (gsource);

		/* add a timeout for quick uptating. @todo Config option? */
		g_timeout_add_seconds(GOOGLE_SOURCE_QUICK_UPDATE_INTERVAL, google_source_quick_update_timeout, g_strdup(gsource->root->id));

	} else {
		debug0 (DEBUG_UPDATE, "google reader login failed! no SID found in result!");
		subscription->node->available = FALSE;

		g_free(subscription->updateError);
		subscription->updateError = g_strdup (_("Google Reader login failed!"));
		gsource->loginState = GOOGLE_SOURCE_STATE_NONE;
	}
}


/**
 * Perform a login to Google Reader, if the login completes the 
 * GoogleSource will have a valid sid and will have loginStatus to 
 * GOOGLE_SOURCE_LOGIN_ACTIVE.
 */
void
google_source_login (GoogleSourcePtr gsource, guint32 flags) 
{ 
	gchar *source;
	updateRequestPtr request;
	subscriptionPtr subscription = gsource->root->subscription;
	
	if (gsource->loginState != GOOGLE_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session
		 * doesn't expire. */
		debug1(DEBUG_UPDATE, "Logging in while login state is %d\n", 
			     gsource->loginState);
	}

	request = update_request_new();

	update_request_set_source(request, GOOGLE_READER_LOGIN_URL);
	request->postdata = g_strdup_printf (GOOGLE_READER_LOGIN_POST,
	                     	  subscription->updateOptions->username,
	                          subscription->updateOptions->password);
	request->options = update_options_copy(subscription->updateOptions);

	gsource->loginState = GOOGLE_SOURCE_STATE_IN_PROGRESS ;

	update_execute_request(gsource, request, google_source_login_cb, gsource, flags);
}






/** 
 * Shared actions needed during import and when subscribing,
 * Only feedlist_node_added() will be done only when subscribing. (FIXME)
 */
void
google_source_setup (nodePtr parent, nodePtr node)
{
	node->icon = ui_common_create_pixbuf ("fl_google.png");

	if (parent) {
		gint pos;
		ui_feedlist_get_target_folder (&pos);
		node_set_parent (node, parent, pos);
		feedlist_node_added (node);
	}
	node->data = (gpointer) google_source_new(node);
}

/* node source type implementation */

static void
google_source_update (nodePtr node)
{
	subscription_update (node->subscription, 0);  // FIXME: 0 ?
}

static void
google_source_auto_update (nodePtr node)
{
	GTimeVal	now;
	
	g_get_current_time (&now);
	
	/* do daily updates for the feed list and feed updates according to the default interval */
	if (node->subscription->updateState->lastPoll.tv_sec + GOOGLE_SOURCE_UPDATE_INTERVAL <= now.tv_sec)
		google_source_update (node);
}

static void google_source_init (void) { }

static void google_source_deinit (void) { }

void
google_source_import (nodePtr node)
{
	GSList *iter; 
	opml_source_import (node);
	
	node->subscription->type = &googleSourceOpmlSubscriptionType;
	if (!node->data) node->data = (gpointer) google_source_new (node) ;

	for(iter = node->children; iter; iter = g_slist_next(iter) )
		((nodePtr) iter->data)->subscription->type = &googleSourceFeedSubscriptionType; 
	google_source_edit_import(node->data) ;
}

void
google_source_export (nodePtr node)
{
	opml_source_export (node);
}

gchar* 
google_source_get_feedlist (nodePtr node)
{
	return opml_source_get_feedlist (node);
}

void 
google_source_remove (nodePtr node)
{ 
	opml_source_remove (node);
}


static nodePtr
google_source_add_subscription(nodePtr node, nodePtr parent, subscriptionPtr subscription) 
{ 
	debug_enter("google_source_add_subscription") ;
	nodePtr child = node_new (feed_get_node_type ());

	debug0(DEBUG_UPDATE, "GoogleSource: Adding a new subscription"); 
	node_set_data (child, feed_new ());

	node_set_subscription(child, subscription) ;
	child->subscription->type = &googleSourceFeedSubscriptionType;
	
	node_set_title(child, _("New Subscription")) ;

	google_source_edit_add_subscription(google_source_get_root_from_node(node)->data, subscription->source);
	
	debug_exit("google_source_add_subscription") ;
	return child ; 
}

static void
google_source_remove_node(nodePtr node, nodePtr child) 
{ 
	gchar           *source; 
	GoogleSourcePtr gsource = node->data; 
	if (child == node) { 
		feedlist_node_removed(child);
		return; 
	}

	source = g_strdup(child->subscription->source);

	feedlist_node_removed(child);

	/* propagate the removal only if there aren't other copies */
	if (!google_source_get_node_by_source(gsource, source)) 
		google_source_edit_remove_subscription(gsource, source); 
	g_free(source);
}

/* GUI callbacks */

static void
on_google_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data) 
{
	nodePtr		node, parent = (nodePtr) user_data;
	subscriptionPtr	subscription;

	if (response_id == GTK_RESPONSE_OK) {
		subscription = subscription_new ("http://www.google.com/reader", NULL, NULL);
		subscription->updateOptions->username = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))));
		subscription->updateOptions->password = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));
		subscription->type = &googleSourceOpmlSubscriptionType ; 
		node = node_new (node_source_get_node_type ());
		node_set_title (node, "Google Reader");
		node_source_new (node, google_source_get_type ());
		google_source_setup (parent, node);
		node_set_subscription (node, subscription);
		google_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
on_google_source_dialog_destroy (GtkDialog *dialog,
                                 gpointer user_data) 
{
	g_object_unref (user_data);
}

static void
ui_google_source_get_account_info (nodePtr parent)
{
	GtkWidget	*dialog;

	
	dialog = liferea_dialog_new ("google_source.glade", "google_source_dialog");
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_google_source_selected), 
			  (gpointer) parent);
}

static void
google_source_cleanup (nodePtr node)
{
	GoogleSourcePtr reader = (GoogleSourcePtr) node->data;
	google_source_edit_export(reader);
	google_source_free(reader);
	node->data = NULL ;
}

/* node source type definition */

static struct nodeSourceType nst = {
	.api_version         = NODE_SOURCE_TYPE_API_VERSION,
	.id                  = "fl_google",
	.name                = N_("Google Reader"),
	.description         = N_("Integrate the feed list of your Google Reader account. Liferea will "
	   "present your Google Reader subscriptions, and will synchronize your feed list and reading lists."),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION | NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST,
	.source_type_init    = google_source_init,
	.source_type_deinit  = google_source_deinit,
	.source_new          = ui_google_source_get_account_info,
	.source_delete       = google_source_remove,
	.source_import       = google_source_import,
	.source_export       = google_source_export,
	.source_get_feedlist = google_source_get_feedlist,
	.source_update       = google_source_update,
	.source_auto_update  = google_source_auto_update,
	.free                = google_source_cleanup,
	.item_set_flag       = google_source_item_set_flag,
	.item_mark_read      = google_source_item_mark_read,
	.add_folder          = NULL, 
	.add_subscription    = google_source_add_subscription,
	.remove_node         = google_source_remove_node
};

nodeSourceTypePtr
google_source_get_type(void)
{
	return &nst;
}

