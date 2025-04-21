/**
 * @file theoldreader_source.c  TheOldReader feed list source support
 *
 * Copyright (C) 2007-2024 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_sources/theoldreader_source.h"

#include <glib.h>
#include <gtk/gtk.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "node_providers/folder.h"
#include "item_state.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "ui/auth_dialog.h"
#include "ui/liferea_dialog.h"
#include "node_source.h"
#include "node_sources/opml_source.h"
#include "node_sources/google_reader_api_edit.h"
#include "node_sources/theoldreader_source_feed_list.h"

/** default TheOldReader subscription list update interval = once a day */
#define THEOLDREADER_SOURCE_UPDATE_INTERVAL 60*60*24

#define BASE_URL "https://theoldreader.com/reader/api/0/"

#define SOURCE_ID "fl_theoldreader"

/** create a source with given node as root */
static TheOldReaderSourcePtr
theoldreader_source_new (Node *node)
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
theoldreader_source_login_cb (const UpdateResult * const result, gpointer userdata, updateFlags flags)
{
	Node			*node = (Node *) userdata;
	gchar			*tmp = NULL;
	subscriptionPtr 	subscription = node->subscription;

	debug (DEBUG_UPDATE, "TheOldReader login processing... %s", result->data);

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
		debug (DEBUG_UPDATE, "TheOldReader login failed! no Auth token found in result!");
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
		debug (DEBUG_UPDATE, "Logging in while login state is %d", source->root->source->loginState);
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

	update_job_new (source, request, theoldreader_source_login_cb, source->root, flags | UPDATE_REQUEST_NO_FEED);
}

/* node source type implementation */

static void
theoldreader_source_auto_update (Node *node)
{
	if (node->source->loginState == NODE_SOURCE_STATE_NONE) {
		node_source_update (node);
		return;
	}

	if (node->source->loginState == NODE_SOURCE_STATE_IN_PROGRESS)
		return; /* the update will start automatically anyway */

	debug (DEBUG_UPDATE, "theoldreader_source_auto_update()");
	subscription_auto_update (node->subscription);
}

static void
theoldreader_source_import (Node *node)
{
	opml_source_import (node);

	node->subscription->updateInterval = -1;
	node->subscription->type = node->source->type->sourceSubscriptionType;
	if (!node->data)
		node->data = (gpointer) theoldreader_source_new (node);
}

static Node *
theoldreader_source_add_subscription (Node *root, subscriptionPtr subscription)
{
	Node			*parent;
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
theoldreader_source_get_stream_id_for_node (Node *node)
{
	if (!node->subscription)
		return NULL;

	return g_strdup (metadata_list_get (node->subscription->metadata, "theoldreader-feed-id"));
}

static void
theoldreader_source_remove_node (Node *node, Node *child)
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
	if (response_id == GTK_RESPONSE_OK) {
		Node *node = node_new ("node_source");
		node_source_new (node, SOURCE_ID, "http://theoldreader.com/reader");
		node_set_title (node, node->source->type->name);

		subscription_set_auth_info (node->subscription,
		                            liferea_dialog_entry_get (GTK_WIDGET(dialog), "userEntry"),
		                            liferea_dialog_entry_get (GTK_WIDGET(dialog), "passwordEntry"));

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
theoldreader_source_cleanup (Node *node)
{
	TheOldReaderSourcePtr reader = (TheOldReaderSourcePtr) node->data;
	theoldreader_source_free(reader);
	node->data = NULL;
}

static void
theoldreader_source_item_set_flag (Node *node, itemPtr item, gboolean newStatus)
{
	google_reader_api_edit_mark_starred (node->source, item->sourceId, node->subscription->source, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
theoldreader_source_item_mark_read (Node *node, itemPtr item, gboolean newStatus)
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
theoldreader_source_convert_to_local (Node *node)
{
	node_source_set_state (node, NODE_SOURCE_STATE_MIGRATE);
}

static void
theoldreader_source_reparent_node (Node *node, Node *oldParent, Node *newParent)
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

/* node source provider definition */

extern struct subscriptionType theOldReaderSourceFeedSubscriptionType;
extern struct subscriptionType theOldReaderSourceOpmlSubscriptionType;

typedef struct {
	GObject parent_instance;
} TheoldreaderSourceProvider;

typedef struct {
	GObjectClass parent_class;
} TheoldreaderSourceProviderClass;

static void theoldreader_source_provider_init(TheoldreaderSourceProvider *self) { }
static void theoldreader_source_provider_class_init(TheoldreaderSourceProviderClass *klass) { }
static void theoldreader_source_provider_interface_init(NodeSourceProviderInterface *iface) {
	iface->id                  = SOURCE_ID;
	iface->name                = N_("TheOldReader");
	iface->capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
				     NODE_SOURCE_CAPABILITY_CAN_LOGIN |
				     NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
				     NODE_SOURCE_CAPABILITY_ADD_FEED |
				     NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
				     NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL |
				     NODE_SOURCE_CAPABILITY_GOOGLE_READER_API |
				     NODE_SOURCE_CAPABILITY_REPARENT_NODE;
	iface->feedSubscriptionType = &theOldReaderSourceFeedSubscriptionType;
	iface->sourceSubscriptionType = &theOldReaderSourceOpmlSubscriptionType;
	iface->source_new          = ui_theoldreader_source_get_account_info;
	iface->source_delete       = opml_source_remove;
	iface->source_import       = theoldreader_source_import;
	iface->source_export       = opml_source_export;
	iface->source_get_feedlist = opml_source_get_feedlist;
	iface->source_auto_update  = theoldreader_source_auto_update;
	iface->free                = theoldreader_source_cleanup;
	iface->item_set_flag       = theoldreader_source_item_set_flag;
	iface->item_mark_read      = theoldreader_source_item_mark_read;
	iface->add_subscription    = theoldreader_source_add_subscription;
	iface->remove_node         = theoldreader_source_remove_node;
	iface->convert_to_local    = theoldreader_source_convert_to_local;
	iface->reparent_node       = theoldreader_source_reparent_node;
}

#define THEOLDREADER_TYPE_SOURCE_PROVIDER (theoldreader_source_provider_get_type())

G_DEFINE_TYPE_WITH_CODE(TheoldreaderSourceProvider, theoldreader_source_provider, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(NODE_TYPE_SOURCE_PROVIDER, theoldreader_source_provider_interface_init))

void
theoldreader_source_register (void)
{
	metadata_type_register ("theoldreader-feed-id", METADATA_TYPE_TEXT);

	NodeSourceProviderInterface *iface = NODE_SOURCE_PROVIDER_GET_IFACE (g_object_new (THEOLDREADER_TYPE_SOURCE_PROVIDER, NULL));
	node_source_type_register (iface);
}