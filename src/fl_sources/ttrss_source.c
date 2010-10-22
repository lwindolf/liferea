/**
 * @file ttrss_source.c  tt-rss feed list source support
 * 
 * Copyright (C) 2010 Lars Lindner <lars.lindner@gmail.com>
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

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "item_state.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "json.h"
#include "ui/auth_dialog.h"
#include "ui/liferea_dialog.h"
#include "ui/ui_common.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"

/** default tt-rss subscription list update interval = once a day */
#define TTRSS_SOURCE_UPDATE_INTERVAL 60*60*24

/** create a google source with given node as root */ 
static TtRssSourcePtr
ttrss_source_new (nodePtr node) 
{
	TtRssSourcePtr source = g_new0 (struct TtRssSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new (); 
	source->loginState = TTRSS_SOURCE_STATE_NONE; 
	source->lastTimestampMap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	
	return source;
}

static void
ttrss_source_free (TtRssSourcePtr source) 
{
	if (!source)
		return;

	update_job_cancel_by_owner (source);
	
	g_free (source->authHeaderValue);
	g_queue_free (source->actionQueue) ;
	g_hash_table_unref (source->lastTimestampMap);
	g_free (source);
}


/* node source type implementation */

static void
ttrss_source_update (nodePtr node)
{
	subscription_update (node->subscription, 0);  // FIXME: 0 ?
}

static void
ttrss_source_auto_update (nodePtr node)
{
	g_warning ("FIXME: ttrss_source_auto_update(): Implement me!");
}

static void ttrss_source_init (void) { }

static void ttrss_source_deinit (void) { }

static void
ttrss_source_import (nodePtr node)
{
//	GSList *iter; 
	opml_source_import (node);
	
//	node->subscription->type = &googleSourceOpmlSubscriptionType;
	if (!node->data)
		node->data = (gpointer) ttrss_source_new (node);

/*	for (iter = node->children; iter; iter = g_slist_next(iter))
		((nodePtr) iter->data)->subscription->type = &googleSourceFeedSubscriptionType; 
*/
	g_warning ("FIXME: ttrss_source_import(): Implement me!");		
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

static nodePtr
ttrss_source_add_subscription (nodePtr node, subscriptionPtr subscription) 
{ 
	g_warning ("FIXME: ttrss_source_add_subscription(): Implement me!");
	return NULL;
}

static void
ttrss_source_remove_node (nodePtr node, nodePtr child) 
{ 
	g_warning ("FIXME: ttrss_source_remove_node(): Implement me!");
}

/* GUI callbacks */

/*static void
on_google_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data) 
{
	nodePtr		node;
	subscriptionPtr	subscription;

	if (response_id == GTK_RESPONSE_OK) {
		subscription = subscription_new ("http://www.google.com/reader", NULL, NULL);
		subscription->updateOptions->username = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))));
		subscription->updateOptions->password = g_strdup (gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));
		subscription->type = &googleSourceOpmlSubscriptionType ; 
		node = node_new (node_source_get_node_type ());
		node_set_title (node, "Google Reader");
		node_source_new (node, google_source_get_type ());
		node_set_subscription (node, subscription);
		node->data = google_source_new (node);
		feedlist_node_added (node);
		google_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}
*/
static void
ui_ttrss_source_get_account_info (void)
{
	GtkWidget	*dialog;

	
	dialog = liferea_dialog_new ("ttrss_source.ui", "ttrss_source_dialog");
	
	g_warning ("FIXME: ui_ttrss_source_get_account_info(): Implement me!");
/*	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_google_source_selected), 
			  NULL);*/
}

static void
ttrss_source_cleanup (nodePtr node)
{
	TtRssSourcePtr source = (TtRssSourcePtr) node->data;
	ttrss_source_free (source);
	node->data = NULL ;
}

/* node source type definition */

static struct nodeSourceType nst = {
	.id                  = "fl_ttrss",
	.name                = N_("Tiny Tiny RSS"),
	.description         = N_("Integrate the feed list of your Tiny Tiny RSS account. Liferea will "
	   "present your tt-rss subscriptions, and will synchronize your feed list and reading lists."),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION | NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST,
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
	.item_set_flag       = NULL, // FIXME
	.item_mark_read      = NULL, // FIXME
	.add_folder          = NULL, 
	.add_subscription    = ttrss_source_add_subscription,
	.remove_node         = ttrss_source_remove_node
};

nodeSourceTypePtr
ttrss_source_get_type (void)
{
	return &nst;
}
