/**
 * @file reedah_source.c  Reedah source support
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

#include "node_sources/reedah_source.h"

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
#include "node_sources/google_reader_api_edit.h"
#include "node_source.h"
#include "node_sources/opml_source.h"
#include "node_sources/reedah_source_feed_list.h"

/** default Reedah subscription list update interval = once a day */
#define NODE_SOURCE_UPDATE_INTERVAL (guint64)(60*60*24) * (guint64)G_USEC_PER_SEC

#define BASE_URL "http://www.reedah.com/reader/api/0/"

#define SOURCE_ID "fl_reedah"

/** create a Reedah source with given node as root */
static ReedahSourcePtr
reedah_source_new (Node *node)
{
	ReedahSourcePtr source = g_new0 (struct ReedahSource, 1) ;
	source->root = node;
	source->lastTimestampMap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	node->source->api.subscription_list		= g_strdup_printf ("%s/subscription/list", BASE_URL);
	node->source->api.unread_count			= g_strdup_printf ("%s/unread-count?all=true&client=liferea", BASE_URL);
	node->source->api.token				= g_strdup_printf ("%s/token", BASE_URL);
	node->source->api.add_subscription		= g_strdup_printf ("%s/subscription/edit?client=liferea", BASE_URL);
	node->source->api.add_subscription_post		= g_strdup ("s=feed%%2F%s&i=null&ac=subscribe&T=%s");
	node->source->api.remove_subscription		= g_strdup_printf ("%s/subscription/edit?client=liferea", BASE_URL);
	node->source->api.remove_subscription_post	= g_strdup ("s=%s&i=null&ac=unsubscribe&T=%s");
	node->source->api.edit_tag			= g_strdup_printf ("%s/edit-tag?client=liferea", BASE_URL);
	node->source->api.edit_tag_add_post		= g_strdup ("i=%s&s=%s%%2F%s&a=%s&ac=edit-tags&T=%s&async=true");
	node->source->api.edit_tag_remove_post		= g_strdup ("i=%s&s=%s%%2F%s&r=%s&ac=edit-tags&T=%s&async=true");
	node->source->api.edit_tag_ar_tag_post		= g_strdup ("i=%s&s=%s%%2F%s&a=%s&r=%s&ac=edit-tags&T=%s&async=true");
	node->source->api.edit_label			= g_strdup_printf("%s/subscription/edit?client=liferea", BASE_URL);
	node->source->api.edit_add_label_post		= g_strdup ("s=%s&a=%s&ac=edit&T=%s&async=true");
	node->source->api.edit_remove_label_post	= g_strdup ("s=%s&r=%s&ac=edit&T=%s&async=true");

	return source;
}

static void
reedah_source_free (ReedahSourcePtr source)
{
	if (!source)
		return;

	update_job_cancel_by_owner (source);

	g_hash_table_unref (source->lastTimestampMap);
	g_free (source);
}

static void
reedah_source_login_cb (const UpdateResult * const result, gpointer userdata, updateFlags flags)
{
	Node		*node = (Node *) userdata;
	gchar		*tmp = NULL;
	subscriptionPtr subscription = node->subscription;

	debug (DEBUG_UPDATE, "Reedah login processing... %s", result->data);

	if (result->data && result->httpstatus == 200)
		tmp = strstr (result->data, "Auth=");

	if (tmp) {
		gchar *ttmp = tmp;
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		node_source_set_auth_token (node, g_strdup_printf ("ReedahLogin auth=%s", ttmp + 5));

		/* now that we are authenticated trigger updating to start data retrieval */
		if (!(flags & NODE_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (subscription, flags);

		/* process any edits waiting in queue */
		google_reader_api_edit_process (node->source);

	} else {
		debug (DEBUG_UPDATE, "Reedah login failed! no Auth token found in result!");

		g_free (subscription->updateError);
		subscription->updateError = g_strdup (_("Login failed!"));
		node_source_set_state (node, NODE_SOURCE_STATE_NO_AUTH);

		auth_dialog_new (subscription, flags);
	}
}

/**
 * Perform a login to Reedah, if the login completes the
 * ReedahSource will have a valid Auth token and will have loginStatus to
 * NODE_SOURCE_LOGIN_ACTIVE.
 */
void
reedah_source_login (ReedahSourcePtr source, guint32 flags)
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
		REEDAH_READER_LOGIN_URL,
		subscription->updateState,
		NULL	// auth is done via POST below!
	);

	/* escape user and password as both are passed using an URI */
	username = g_uri_escape_string (subscription->updateOptions->username, NULL, TRUE);
	password = g_uri_escape_string (subscription->updateOptions->password, NULL, TRUE);

	request->postdata = g_strdup_printf (REEDAH_READER_LOGIN_POST, username, password);

	g_free (username);
	g_free (password);

	node_source_set_state (source->root, NODE_SOURCE_STATE_IN_PROGRESS);

	update_job_new (source, request, reedah_source_login_cb, source->root, flags | UPDATE_REQUEST_NO_FEED);
}

/* node source type implementation */

static void
reedah_source_auto_update (Node *node)
{
	guint64	now;
	ReedahSourcePtr source = (ReedahSourcePtr) node->data;

	if (node->source->loginState == NODE_SOURCE_STATE_NONE) {
		node_source_update (node);
		return;
	}

	if (node->source->loginState == NODE_SOURCE_STATE_IN_PROGRESS)
		return; /* the update will start automatically anyway */

	debug (DEBUG_UPDATE, "reedah_source_auto_update()");

	now = g_get_real_time();

	/* do daily updates for the feed list and feed updates according to the default interval */
	if (node->subscription->updateState->lastPoll + NODE_SOURCE_UPDATE_INTERVAL <= now) {
		subscription_update (node->subscription, 0);
		source->lastQuickUpdate = g_get_real_time();
	}
	else if (source->lastQuickUpdate + REEDAH_SOURCE_QUICK_UPDATE_INTERVAL <= now) {
		reedah_source_opml_quick_update (source);
		google_reader_api_edit_process (node->source);
		source->lastQuickUpdate = g_get_real_time();
	}
}

static void
reedah_source_import (Node *node)
{
	opml_source_import (node);

	node->subscription->updateInterval = -1;
	node->subscription->type = node->source->type->sourceSubscriptionType;
	if (!node->data)
		node->data = (gpointer) reedah_source_new (node);
}

static Node *
reedah_source_add_subscription (Node *node, subscriptionPtr subscription)
{
	// FIXME: determine correct category from parent folder name
	google_reader_api_edit_add_subscription (node->source, subscription->source, NULL);

	subscription_free (subscription);

	return NULL;
}

static gchar *
reedah_source_get_stream_id_for_node (Node *node)
{
	if (!node->subscription)
		return NULL;

	return g_strdup_printf ("feed/%s", node->subscription->source);
}

static void
reedah_source_remove_node (Node *node, Node *child)
{
	g_autofree gchar *url = NULL, *streamId = NULL;
	ReedahSourcePtr source = node->data;

	if (child == node) {
		feedlist_node_removed (child);
		return;
	}

	url = g_strdup (child->subscription->source);
	streamId = reedah_source_get_stream_id_for_node (child);

	feedlist_node_removed (child);

	/* propagate the removal only if there aren't other copies */
	if (!feedlist_find_node (source->root, NODE_BY_URL, url))
		google_reader_api_edit_remove_subscription (node->source, streamId, reedah_source_get_stream_id_for_node);
}

/* GUI callbacks */

static void
on_reedah_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data)
{
	if (response_id == GTK_RESPONSE_OK) {
		Node *node = node_new ("node_source");
		node_source_new (node, SOURCE_ID, "http://www.reedah.com/reader");
		node_set_title (node, node->source->type->name);
		
		subscription_set_auth_info (node->subscription,
					    liferea_dialog_entry_get (GTK_WIDGET (dialog), "userEntry"),
					    liferea_dialog_entry_get (GTK_WIDGET (dialog), "passwordEntry"));

		node->data = reedah_source_new (node);
		feedlist_node_added (node);
		node_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ui_reedah_source_get_account_info (void)
{
	GtkWidget	*dialog;

	dialog = liferea_dialog_new ("reedah_source");

	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_reedah_source_selected),
			  NULL);
}

static void
reedah_source_cleanup (Node *node)
{
	ReedahSourcePtr reader = (ReedahSourcePtr) node->data;
	reedah_source_free(reader);
	node->data = NULL ;
}

static void
reedah_source_item_set_flag (Node *node, itemPtr item, gboolean newStatus)
{
	google_reader_api_edit_mark_starred (node->source, item->sourceId, node->subscription->source, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
reedah_source_item_mark_read (Node *node, itemPtr item, gboolean newStatus)
{
	google_reader_api_edit_mark_read (node->source, item->sourceId, node->subscription->source, newStatus);
	item_read_state_changed (item, newStatus);
}

/**
 * Convert all subscriptions of a Reedah source to local feeds
 *
 * @param node The node to migrate (not the nodeSource!)
 */
static void
reedah_source_convert_to_local (Node *node)
{
	node_source_set_state (node, NODE_SOURCE_STATE_MIGRATE);
}

/* node source provider definition */

extern struct subscriptionType reedahSourceFeedSubscriptionType;
extern struct subscriptionType reedahSourceOpmlSubscriptionType;

typedef struct {
	GObject parent_instance;
} ReedahSourceProvider;

typedef struct {
	GObjectClass parent_class;
} ReedahSourceProviderClass;

static void reedah_source_provider_init(ReedahSourceProvider *self) { }
static void reedah_source_provider_class_init(ReedahSourceProviderClass *klass) { }
static void reedah_source_provider_interface_init(NodeSourceProviderInterface *iface) {
	iface->id                  = SOURCE_ID;
	iface->name                = N_("Reedah");
	iface->capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
				     NODE_SOURCE_CAPABILITY_CAN_LOGIN |
				     NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
				     NODE_SOURCE_CAPABILITY_ADD_FEED |
				     NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
				     NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL |
				     NODE_SOURCE_CAPABILITY_GOOGLE_READER_API;
	iface->feedSubscriptionType = &reedahSourceFeedSubscriptionType;
	iface->sourceSubscriptionType = &reedahSourceOpmlSubscriptionType;
	iface->source_new          = ui_reedah_source_get_account_info;
	iface->source_delete       = opml_source_remove;
	iface->source_import       = reedah_source_import;
	iface->source_export       = opml_source_export;
	iface->source_get_feedlist = opml_source_get_feedlist;
	iface->source_auto_update  = reedah_source_auto_update;
	iface->free                = reedah_source_cleanup;
	iface->item_set_flag       = reedah_source_item_set_flag;
	iface->item_mark_read      = reedah_source_item_mark_read;
	iface->add_subscription    = reedah_source_add_subscription;
	iface->remove_node         = reedah_source_remove_node;
	iface->convert_to_local    = reedah_source_convert_to_local;
}

#define REEDAH_TYPE_SOURCE_PROVIDER (reedah_source_provider_get_type())

G_DEFINE_TYPE_WITH_CODE(ReedahSourceProvider, reedah_source_provider, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(NODE_TYPE_SOURCE_PROVIDER, reedah_source_provider_interface_init))

void
reedah_source_register (void)
{
	metadata_type_register ("reedah-feed-id", METADATA_TYPE_TEXT);

	NodeSourceProviderInterface *iface = NODE_SOURCE_PROVIDER_GET_IFACE (g_object_new (REEDAH_TYPE_SOURCE_PROVIDER, NULL));
	node_source_type_register (iface);
}