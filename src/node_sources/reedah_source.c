/**
 * @file reedah_source.c  Reedah source support
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
#include "node_source.h"
#include "subscription.h"
#include "update.h"
#include "xml.h"
#include "ui/auth_dialog.h"
#include "node_sources/google_reader_api_edit.h"
#include "node_sources/reedah_source_feed_list.h"

/** default Reedah subscription list update interval = once a day */
#define NODE_SOURCE_UPDATE_INTERVAL (guint64)(60*60*24) * (guint64)G_USEC_PER_SEC

#define BASE_URL "https://www.reedah.com/reader/api/0/"

/* About this node source:

   - feedlist sync strategy: remote is always right, no 2-way
   - protocol: Google reader like, but a little bit different
 */

/** create a Reedah source with given node as root */
static void
reedah_source_new (Node *node)
{
	g_object_set_data_full (
		G_OBJECT (node),
		"lastTimestampMap",
		g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free),
		(GDestroyNotify)g_hash_table_destroy
	);

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

	// FIXME: document why
	node->subscription->updateInterval = -1;
}

static gboolean
reedah_source_login_cb (UpdateJob *job)
{
	UpdateResult	*result = job->result;
	Node		*root = (Node *) job->user_data;
	gchar		*tmp = NULL;
	subscriptionPtr subscription = root->subscription;

	debug (DEBUG_UPDATE, "reedah_source: %s |%s| login processing... msg=>>>%s<<<", root->id, root->title, result->data);

	if (result->data && result->httpstatus == 200)
		tmp = strstr (result->data, "Auth=");

	if (tmp) {
		gchar *ttmp = tmp;
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		node_source_set_auth_token (root, g_strdup_printf ("GoogleLogin auth=%s", ttmp + 5));

		/* now that we are authenticated trigger updating to start data retrieval */
		subscription_update (subscription, job->flags);

		/* process any edits waiting in queue */
		google_reader_api_edit_process (root->source);
	} else {
		debug (DEBUG_UPDATE, "reedah_source: %s |%s| login failed! no Auth token found in result!", root->id, root->title);
		node_source_set_auth_failed (root, job->flags & UPDATE_REQUEST_PRIORITY_HIGH);
	}

	return TRUE;
}

void
reedah_source_login (Node *root, guint32 flags)
{
	UpdateRequest		*request;
	subscriptionPtr		subscription = root->subscription;

	request = update_request_new (
		"POST",
		REEDAH_READER_LOGIN_URL,
		subscription->updateState,
		NULL	// auth is done via POST below!
	);

	/* escape user and password as both are passed using an URI */
	g_autofree gchar *username = g_uri_escape_string (subscription->updateOptions->username, NULL, TRUE);
	g_autofree gchar *password = g_uri_escape_string (subscription->updateOptions->password, NULL, TRUE);

	request->postdata = g_strdup_printf (REEDAH_READER_LOGIN_POST, username, password);


	update_job_new (root, request, reedah_source_login_cb, root, flags | UPDATE_REQUEST_NO_FEED);
}

/* node source type implementation */

static void
reedah_source_init (void)
{
	metadata_type_register ("reedah-feed-id", METADATA_TYPE_TEXT);
}

static Node *
reedah_source_add_subscription (Node *node, subscriptionPtr subscription)
{
	// FIXME: determine correct category from parent folder name
	google_reader_api_edit_add_subscription (node->source, subscription->origSource, NULL);

	subscription_free (subscription);

	return NULL;
}

static gchar *
reedah_source_get_stream_id_for_node (Node *node)
{
	if (!node->subscription)
		return NULL;

	return g_strdup_printf ("feed/%s", node->subscription->origSource);
}

static void
reedah_source_remove_node (Node *root, Node *child)
{
	g_autofree gchar *url = NULL, *streamId = NULL;

	if (child == root) {
		feedlist_node_removed (child);
		return;
	}

	url = g_strdup (child->subscription->origSource);
	streamId = reedah_source_get_stream_id_for_node (child);

	feedlist_node_removed (child);

	/* propagate the removal only if there aren't other copies */
	if (!feedlist_find_node (root, NODE_BY_URL, url))
		google_reader_api_edit_remove_subscription (root->source, streamId, reedah_source_get_stream_id_for_node);
}

static void
reedah_source_item_set_flag (Node *node, itemPtr item, gboolean newStatus)
{
	google_reader_api_edit_mark_starred (node->source, item->sourceId, node->subscription->origSource, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
reedah_source_item_mark_read (Node *node, itemPtr item, gboolean newStatus)
{
	google_reader_api_edit_mark_read (node->source, item->sourceId, node->subscription->origSource, newStatus);
	item_read_state_changed (item, newStatus);
}

extern struct subscriptionType reedahSourceFeedSubscriptionType;
extern struct subscriptionType reedahSourceOpmlSubscriptionType;

static struct nodeSourceType nst = {
	.id                  = "fl_reedah",
	.name                = N_("Reedah"),
	.addInfo             = N_("Please provide your <a href='https://reedah.com'>reedah.com</a> account credentials."),
	.url                 = "https://www.reedah.com/reader",
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION |
	                       NODE_SOURCE_CAPABILITY_CAN_LOGIN |
	                       NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
	                       NODE_SOURCE_CAPABILITY_ADD_FEED |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
	                       NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL |
	                       NODE_SOURCE_CAPABILITY_GOOGLE_READER_API,
	.feedSubscriptionType = &reedahSourceFeedSubscriptionType,
	.sourceSubscriptionType = &reedahSourceOpmlSubscriptionType,
	.source_type_init    = reedah_source_init,
	.source_type_deinit  = NULL,
	.source_new	     = reedah_source_new,
	.source_login        = reedah_source_login,
	.item_set_flag       = reedah_source_item_set_flag,
	.item_mark_read      = reedah_source_item_mark_read,
	.add_folder          = NULL,
	.add_subscription    = reedah_source_add_subscription,
	.remove_node         = reedah_source_remove_node
};

nodeSourceTypePtr
reedah_source_get_type (void)
{
	return &nst;
}
