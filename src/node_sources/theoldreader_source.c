/**
 * @file theoldreader_source.c  TheOldReader feed list source support
 *
 * Copyright (C) 2007-2026 Lars Windolf <lars.windolf@gmx.de>
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
#include "node_source.h"
#include "node_sources/google_reader_api_edit.h"
#include "node_sources/theoldreader_source_feed_list.h"

/** default TheOldReader subscription list update interval = once a day */
#define THEOLDREADER_SOURCE_UPDATE_INTERVAL 60*60*24

#define BASE_URL "https://theoldreader.com/reader/api/0/"

static void
theoldreader_source_new (Node *node)
{
	g_object_set_data_full (
		G_OBJECT (node),
		"lastTimestampMap",
		g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free),
		(GDestroyNotify)g_hash_table_destroy
	);
	g_object_set_data_full (
		G_OBJECT (node),
		"folderToCategory",
		g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free),
		(GDestroyNotify)g_hash_table_destroy
	);	

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

	// FIXME: document why
	node->subscription->updateInterval = -1;
}

static gboolean
theoldreader_source_login_cb  (UpdateJob *job)
{
	UpdateResult		*result = job->result;
	Node			*node = (Node *) job->user_data;
	gchar			*tmp = NULL;
	subscriptionPtr 	subscription = node->subscription;

	debug (DEBUG_UPDATE, "theoldreader_source: %s |%s| login processing... >>>%s<<<", node->id, node->title, result->data);

	if (result->data && result->httpstatus == 200)
		tmp = strstr (result->data, "Auth=");

	if (tmp) {
		gchar *ttmp = tmp;
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		node_source_set_auth_token (node, g_strdup_printf ("GoogleLogin auth=%s", ttmp + 5));

		/* now that we are authenticated trigger updating to start data retrieval */
		subscription_update (subscription, job->flags);

		/* process any edits waiting in queue */
		google_reader_api_edit_process (node->source);

	} else {
		debug (DEBUG_UPDATE, "theoldreader_source: %s |%s| login failed! no Auth token found in result!", node->id, node->title);
		node_source_set_auth_failed (node, job->flags & UPDATE_REQUEST_PRIORITY_HIGH);
	}

	return TRUE;
}

void
theoldreader_source_login (Node *root, guint32 flags)
{
	UpdateRequest		*request;

	request = update_request_new (
		"POST",
		THEOLDREADER_READER_LOGIN_URL,
		NULL,
		root->subscription->updateOptions
	);

	g_autofree gchar *username = g_uri_escape_string (root->subscription->updateOptions->username, NULL, TRUE);
	g_autofree gchar *password = g_uri_escape_string (root->subscription->updateOptions->password, NULL, TRUE);

	request->postdata = g_strdup_printf (THEOLDREADER_READER_LOGIN_POST, username, password);

	update_job_new (root, request, theoldreader_source_login_cb, root, flags | UPDATE_REQUEST_NO_FEED);
}

/* node source type implementation */

static
void theoldreader_source_init (void)
{
	metadata_type_register ("theoldreader-feed-id", METADATA_TYPE_TEXT);
}

static Node *
theoldreader_source_add_subscription (Node *root, subscriptionPtr subscription)
{
	Node		*parent;
	gchar		*categoryId = NULL;
	GHashTable	*h = g_object_get_data (G_OBJECT (root), "folderToCategory");	

	/* Determine correct category from selected folder name */
	parent = feedlist_get_selected ();
	if (parent) {
		if (parent->subscription)
			parent = parent->parent;
		categoryId = g_hash_table_lookup (h, parent->id);
	}

	google_reader_api_edit_add_subscription (root->source, subscription->origSource, categoryId);

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
		g_warning ("Cannot remove node on remote side as theoldreader-feed-id is unknown!\n");
		return;
	}

	feedlist_node_removed (child);

	google_reader_api_edit_remove_subscription (node->source, id, theoldreader_source_get_stream_id_for_node);
}

static void
theoldreader_source_item_set_flag (Node *node, itemPtr item, gboolean newStatus)
{
	google_reader_api_edit_mark_starred (node->source, item->sourceId, node->subscription->origSource, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
theoldreader_source_item_mark_read (Node *node, itemPtr item, gboolean newStatus)
{
	google_reader_api_edit_mark_read (node->source, item->sourceId, node->subscription->origSource, newStatus);
	item_read_state_changed (item, newStatus);
}

static void
theoldreader_source_reparent_node (Node *root, Node *oldParent, Node *newParent)
{
	gchar			*categoryId = NULL;
	g_autofree gchar	*id = NULL;
	GHashTable		*h = g_object_get_data (G_OBJECT (root), "folderToCategory");

	if (oldParent == newParent)
		return;

	id = theoldreader_source_get_stream_id_for_node (root);
	if (!id) {
		g_warning ("Cannot sync parent on remote side as theoldreader-feed-id is unknown!\n");
		return;
	}

	// Note: this will only work without nested folders
	if (newParent == root) {
		categoryId = g_hash_table_lookup (h, oldParent->id);
		google_reader_api_edit_remove_label (root->source, id, categoryId);
	} else {
		categoryId = g_hash_table_lookup (h, newParent->id);
		google_reader_api_edit_add_label (root->source, id, categoryId);
	}
}

extern struct subscriptionType theOldReaderSourceFeedSubscriptionType;
extern struct subscriptionType theOldReaderSourceOpmlSubscriptionType;

static struct nodeSourceType nst = {
	.id                  = "fl_theoldreader",
	.name                = N_("TheOldReader"),
	.addInfo             = N_("Please provide your <a href='https://theoldreader.com'>TheOldReader</a> account credentials."),
	.url                 = "https://theoldreader.com/reader",
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
	.source_type_deinit  = NULL,
	.source_new	     = theoldreader_source_new,
	.source_login        = theoldreader_source_login,
	.item_set_flag       = theoldreader_source_item_set_flag,
	.item_mark_read      = theoldreader_source_item_mark_read,
	.add_folder          = NULL,
	.add_subscription    = theoldreader_source_add_subscription,
	.remove_node         = theoldreader_source_remove_node,
	.reparent_node       = theoldreader_source_reparent_node
};

nodeSourceTypePtr
theoldreader_source_get_type (void)
{
	return &nst;
}
