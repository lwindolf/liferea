/**
 * @file theoldreader_source.c  TheOldReader feed list source support
 *
 * Copyright (C) 2007-2016 Lars Windolf <lars.windolf@gmx.de>
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

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "folder.h"
#include "item_state.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "ui/auth_dialog.h"
#include "ui/liferea_dialog.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/google_reader_api_edit.h"
#include "fl_sources/theoldreader_source_feed_list.h"

/** default TheOldReader subscription list update interval = once a day */
#define THEOLDREADER_SOURCE_UPDATE_INTERVAL 60*60*24

#define BASE_URL "https://theoldreader.com/reader/api/0/"

/** create a source with given node as root */
static TheOldReaderSourcePtr
theoldreader_source_new (nodePtr node)
{
	TheOldReaderSourcePtr source = g_new0 (struct TheOldReaderSource, 1) ;
	source->root = node;
	source->lastTimestampMap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	source->folderToCategory = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	node->source->api.json				= TRUE,
	node->source->api.subscription_list		= g_strdup_printf ("%s/subscription/list?output=json", BASE_URL);
	node->source->api.unread_count			= g_strdup_printf ("%s/unread-count?all=true&client=liferea", BASE_URL);
	node->source->api.token				= g_strdup_printf ("%s/token", BASE_URL);
	node->source->api.add_subscription		= g_strdup_printf ("%s/subscription/edit?client=liferea", BASE_URL);
	node->source->api.add_subscription_post		= g_strdup ("s=feed%%2F%s&ac=subscribe&T=%s");
	node->source->api.remove_subscription		= g_strdup_printf ("%s/subscription/edit?client=liferea", BASE_URL);
	node->source->api.remove_subscription_post	= g_strdup ("s=%s&ac=unsubscribe&T=%s");
	node->source->api.edit_tag			= g_strdup_printf ("%s/edit-tag?client=liferea", BASE_URL);
	node->source->api.edit_tag_add_post		= g_strdup ("i=%s&s=%s%%2F%s&a=%s&ac=edit-tags&T=%s&async=true");
	node->source->api.edit_tag_remove_post		= g_strdup ("i=%s&s=%s%%2F%s&r=%s&ac=edit-tags&T=%s&async=true");
	node->source->api.edit_tag_ar_tag_post		= g_strdup ("i=%s&s=%s%%2F%s&a=%s&r=%s&ac=edit-tags&T=%s&async=true");
	node->source->api.edit_label			= g_strdup_printf ("%s/subscription/edit?client=liferea", BASE_URL);
	node->source->api.edit_add_label_post		= g_strdup ("s=%s&a=%s&ac=edit&T=%s");
	node->source->api.edit_remove_label_post	= g_strdup ("s=%s&r=%s&ac=edit&T=%s");

	return source;
}

static void
theoldreader_source_free (TheOldReaderSourcePtr source)
{
	if (!source)
		return;

	update_job_cancel_by_owner (source);

	g_hash_table_unref (source->lastTimestampMap);
	g_hash_table_destroy (source->folderToCategory);
	g_free (source);
}

static void
theoldreader_source_login_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	nodePtr			node = (nodePtr) userdata;
	gchar			*tmp = NULL;
	subscriptionPtr 	subscription = node->subscription;

	debug1 (DEBUG_UPDATE, "TheOldReader login processing... %s", result->data);

	if (result->data && result->httpstatus == 200)
		tmp = strstr (result->data, "Auth=");

	if (tmp) {
		gchar *ttmp = tmp;
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		node_source_set_auth_token (node, g_strdup_printf ("GoogleLogin auth=%s", ttmp + 5));

		/* now that we are authenticated trigger updating to start data retrieval */
		if (!(flags & NODE_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (subscription, flags);

		/* process any edits waiting in queue */
		google_reader_api_edit_process (node->source);

	} else {
		debug0 (DEBUG_UPDATE, "TheOldReader login failed! no Auth token found in result!");
		subscription->node->available = FALSE;

		g_free (subscription->updateError);
		subscription->updateError = g_strdup (_("Login failed!"));
		node_source_set_state (node, NODE_SOURCE_STATE_NO_AUTH);

		auth_dialog_new (subscription, flags);
	}
}

/**
 * Perform a login to TheOldReader, if the login completes the
 * TheOldReaderSource will have a valid Auth token and will have loginStatus to
 * NODE_SOURCE_LOGIN_ACTIVE.
 */
void
theoldreader_source_login (TheOldReaderSourcePtr source, guint32 flags)
{
	gchar			*username, *password;
	UpdateRequest		*request;
	subscriptionPtr		subscription = source->root->subscription;

	if (source->root->source->loginState != NODE_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session
		 * doesn't expire. */
		debug1(DEBUG_UPDATE, "Logging in while login state is %d\n", source->root->source->loginState);
	}

	request = update_request_new (
		THEOLDREADER_READER_LOGIN_URL,
		NULL,
		subscription->updateOptions
	);

	/* escape user and password as both are passed using an URI */
	username = g_uri_escape_string (subscription->updateOptions->username, NULL, TRUE);
	password = g_uri_escape_string (subscription->updateOptions->password, NULL, TRUE);

	request->postdata = g_strdup_printf (THEOLDREADER_READER_LOGIN_POST, username, password);

	g_free (username);
	g_free (password);

	node_source_set_state (source->root, NODE_SOURCE_STATE_IN_PROGRESS);

	update_execute_request (source, request, theoldreader_source_login_cb, source->root, flags | FEED_REQ_NO_FEED);
}

/* node source type implementation */

static void
theoldreader_source_auto_update (nodePtr node)
{
	if (node->source->loginState == NODE_SOURCE_STATE_NONE) {
		node_source_update (node);
		return;
	}

	if (node->source->loginState == NODE_SOURCE_STATE_IN_PROGRESS)
		return; /* the update will start automatically anyway */

	debug0 (DEBUG_UPDATE, "theoldreader_source_auto_update()");
	subscription_auto_update (node->subscription);
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

	node->subscription->updateInterval = -1;
	node->subscription->type = node->source->type->sourceSubscriptionType;
	if (!node->data)
		node->data = (gpointer) theoldreader_source_new (node);
}

static nodePtr
theoldreader_source_add_subscription (nodePtr root, subscriptionPtr subscription)
{
	nodePtr			parent;
	gchar			*categoryId = NULL;
	TheOldReaderSourcePtr	source = (TheOldReaderSourcePtr)root->data;



	/* Determine correct category from selected folder name */
	parent = feedlist_get_selected ();
	if (parent) {
		if (parent->subscription)
			parent = parent->parent;
		categoryId = g_hash_table_lookup (source->folderToCategory, parent->id);
	}

	google_reader_api_edit_add_subscription (root->source, subscription->source, categoryId);
	// FIXME: leaking subscription?

	// FIXME: somehow the async subscribing doesn't cause the feed list to update

	return NULL;
}

static gchar *
theoldreader_source_get_stream_id_for_node (nodePtr node)
{
	if (!node->subscription)
		return NULL;

	return g_strdup (metadata_list_get (node->subscription->metadata, "theoldreader-feed-id"));
}

static void
theoldreader_source_remove_node (nodePtr node, nodePtr child)
{
	g_autofree gchar	*id = NULL;

	if (child == node) {
		feedlist_node_removed (child);
		return;
	}

	if (IS_FOLDER (child))
		return;

	id = theoldreader_source_get_stream_id_for_node (child);
	if (!id) {
		g_print ("Cannot remove node on remote side as theoldreader-feed-id is unknown!\n");
		return;
	}

	feedlist_node_removed (child);

	google_reader_api_edit_remove_subscription (node->source, id, theoldreader_source_get_stream_id_for_node);
}

/* GUI callbacks */

static void
on_theoldreader_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data)
{
	nodePtr		node;

	if (response_id == GTK_RESPONSE_OK) {
		node = node_new (node_source_get_node_type ());
		node_source_new (node, theoldreader_source_get_type (), "http://theoldreader.com/reader");
		node_set_title (node, node->source->type->name);

		subscription_set_auth_info (node->subscription,
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry"))),
		                            gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry"))));

		node->data = theoldreader_source_new (node);
		feedlist_node_added (node);
		node_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ui_theoldreader_source_get_account_info (void)
{
	GtkWidget	*dialog;

	dialog = liferea_dialog_new ("theoldreader_source");

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
	google_reader_api_edit_mark_starred (node->source, item->sourceId, node->subscription->source, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
theoldreader_source_item_mark_read (nodePtr node, itemPtr item, gboolean newStatus)
{
	google_reader_api_edit_mark_read (node->source, item->sourceId, node->subscription->source, newStatus);
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
	node_source_set_state (node, NODE_SOURCE_STATE_MIGRATE);
}

static void
theoldreader_source_reparent_node (nodePtr node, nodePtr oldParent, nodePtr newParent)
{
	gchar			*categoryId = NULL;
	g_autofree gchar	*id = NULL;
	TheOldReaderSourcePtr	source = (TheOldReaderSourcePtr) node->source->root->data;

	if (oldParent == newParent)
		return;

	id = theoldreader_source_get_stream_id_for_node (node);
	if (!id) {
		g_print ("Cannot sync parent on remote side as theoldreader-feed-id is unknown!\n");
		return;
	}

	if (newParent == node->source->root) {
		categoryId = g_hash_table_lookup (source->folderToCategory, oldParent->id);
		google_reader_api_edit_remove_label (node->source, id, categoryId);
	} else {
		categoryId = g_hash_table_lookup (source->folderToCategory, newParent->id);
		google_reader_api_edit_add_label (node->source, id, categoryId);
	}
}

/* node source type definition */

extern struct subscriptionType theOldReaderSourceFeedSubscriptionType;
extern struct subscriptionType theOldReaderSourceOpmlSubscriptionType;

static struct nodeSourceType nst = {
	.id                  = "fl_theoldreader",
	.name                = N_("TheOldReader"),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
	                       NODE_SOURCE_CAPABILITY_CAN_LOGIN |
	                       NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
	                       NODE_SOURCE_CAPABILITY_ADD_FEED |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
	                       NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL |
	                       NODE_SOURCE_CAPABILITY_GOOGLE_READER_API |
	                       NODE_SOURCE_CAPABILITY_REPARENT_NODE,
	.feedSubscriptionType = &theOldReaderSourceFeedSubscriptionType,
	.sourceSubscriptionType = &theOldReaderSourceOpmlSubscriptionType,
	.source_type_init    = theoldreader_source_init,
	.source_type_deinit  = theoldreader_source_deinit,
	.source_new          = ui_theoldreader_source_get_account_info,
	.source_delete       = opml_source_remove,
	.source_import       = theoldreader_source_import,
	.source_export       = opml_source_export,
	.source_get_feedlist = opml_source_get_feedlist,
	.source_auto_update  = theoldreader_source_auto_update,
	.free                = theoldreader_source_cleanup,
	.item_set_flag       = theoldreader_source_item_set_flag,
	.item_mark_read      = theoldreader_source_item_mark_read,
	.add_folder          = NULL,
	.add_subscription    = theoldreader_source_add_subscription,
	.remove_node         = theoldreader_source_remove_node,
	.convert_to_local    = theoldreader_source_convert_to_local,
	.reparent_node       = theoldreader_source_reparent_node
};

nodeSourceTypePtr
theoldreader_source_get_type (void)
{
	return &nst;
}
