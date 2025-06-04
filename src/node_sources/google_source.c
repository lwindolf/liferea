/**
 * @file google_source.c  Google reader feed list source support
 * 
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_sources/google_source.h"

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
#include "node_source.h"
#include "node_sources/opml_source.h"
#include "node_sources/google_reader_api_edit.h"
#include "node_sources/google_source_feed_list.h"

/** default Google reader subscription list update interval = once a day */
#define GOOGLE_SOURCE_UPDATE_INTERVAL 60*60*24

#define SOURCE_ID "fl_google_reader"

/** create a google source with given node as root */ 
static GoogleSourcePtr
google_source_new (Node *node) 
{
	GoogleSourcePtr source = g_new0 (struct GoogleSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new (); 
	source->folderToCategory = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	
	/**
	 * Google Source API URL's
	 * In each of the following, the _URL indicates the URL to use, and _POST
	 * indicates the corresponging postdata to send.
	 * @see https://gist.github.com/alexwilson/e24332ed76d561ca1518f21146e5b4e2
	 * However as of now, the GoogleReaderAPI documentation seems outdated, some of
	 * mark read/unread API does not work as mentioned in the documentation.
	 */
	 
	node->source->api.json = TRUE;
	 
	/**
	 * Google Reader Login api.
	 * @param Email The google account email id.
	 * @param Passwd The google account password.
	 * @return The return data has a line "Auth=xxxx" which will be used as an
	 *         Authorization header in future requests. 
	 */ 
	node->source->api.login				= g_strdup_printf ("%s/accounts/ClientLogin", node->subscription->source);
	node->source->api.login_post			= g_strdup_printf ("%s%s", "service=reader&Email=%s&Passwd=%s&source=liferea&continue=", node->subscription->source);
	
	node->source->api.subscription_list		= g_strdup_printf ("%s/reader/api/0/subscription/list?output=json", node->subscription->source);
	node->source->api.unread_count			= g_strdup_printf ("%s/reader/api/0/unread-count?output=json&all=true&client=liferea", node->subscription->source);
	node->source->api.token				= g_strdup_printf ("%s/reader/api/0/token", node->subscription->source);

	/**
	 * Add a subscription
	 * @param URL The feed URL, or the page URL for feed autodiscovery.
	 * @param T   a token obtained using <api.token>
	 */
	node->source->api.add_subscription		= g_strdup_printf ("%s/reader/api/0/subscription/edit?client=liferea", node->subscription->source);
	node->source->api.add_subscription_post		= g_strdup ("s=feed%%2F%s&i=null&ac=subscribe&T=%s");
	
	/**
	 * Unsubscribe from a subscription.
	 * @param url The feed URL
	 * @param T   a token obtained using <api.token>
	 */
	node->source->api.remove_subscription		= g_strdup_printf ("%s/reader/api/0/subscription/edit?client=liferea", node->subscription->source);
	node->source->api.remove_subscription_post	= g_strdup ("s=%s&i=null&ac=unsubscribe&T=%s");

	node->source->api.edit_tag			= g_strdup_printf ("%s/reader/api/0/edit-tag?client=liferea", node->subscription->source);

	/**
	 * Postdata for adding a tag when using <api.edit_tag>.
	 * @param i The guid of the item.
	 * @param prefix The prefix to 's'. For normal feeds this will be "feed", for
	 *          links etc, this should be "user".
	 * @param s The URL of the subscription containing the item. (Note that the 
	 *          following string adds the "feed/" prefix to this.)
	 * @param a The tag to add. 
	 * @param T a token obtained using <api.token>
	 */
	node->source->api.edit_tag_add_post		= g_strdup ("i=%s&s=%s%%2F%s&a=%s&ac=edit-tags&T=%s&async=true");

	/**
	 * Postdata for removing  a tag, when using <api.edit_tag>. Do
	 * not use for removing the "read" tag, see <api.edit_tag_ar_tag_post> 
	 * for that.
	 *
	 * @param i The guid of the item.
	 * @param prefix The prefix to 's'. @see <api.edit_tag_add_post>
	 * @param s The URL of the subscription containing the item. (Note that the 
	 *          final value of s is feed + "/" + this string)
	 * @param r The tag to remove
	 * @param T a token obtained using <api.token>
	 */
	node->source->api.edit_tag_remove_post		= g_strdup ("i=%s&s=%s%%2F%s&r=%s&ac=edit-tags&T=%s&async=true");
	
	/**
	 * Postdata for adding a tag, and removing another tag at the same time, 
	 * when using GOOGLE_READER_EDIT_TAG_URL.
	 * @param i The guid of the item.
	 * @param prefix The prefix to 's'. @see <api.edit_tag_add_post>
	 * @param s The URL of the subscription containing the item. (Note that the 
	 *          final value of s is feed + "/" + this string)
	 * @param a The tag to add. 
	 * @param r The tag to remove
	 * @param T a token obtained using <api.token>
	 */
	node->source->api.edit_tag_ar_tag_post		= g_strdup ("i=%s&s=%s%%2F%s&a=%s&r=%s&ac=edit-tags&T=%s&async=true");
	
	node->source->api.edit_label			= g_strdup_printf("%s/reader/api/0/subscription/edit?client=liferea", node->subscription->source);
	node->source->api.edit_add_label_post		= g_strdup ("s=%s&a=%s&ac=edit&T=%s&async=true");
	node->source->api.edit_remove_label_post	= g_strdup ("s=%s&r=%s&ac=edit&T=%s&async=true");

	return source;
}

static void
google_source_free (GoogleSourcePtr gsource) 
{
	if (!gsource)
		return;

	update_job_cancel_by_owner (gsource);
	
	g_queue_free (gsource->actionQueue) ;
	g_hash_table_destroy (gsource->folderToCategory);
	g_free (gsource);
}

static void
google_source_login_cb (const UpdateResult * const result, gpointer userdata, updateFlags flags)
{
	Node		*node = (Node *) userdata;
	gchar		*tmp = NULL;
	subscriptionPtr subscription = node->subscription;
		
	debug (DEBUG_UPDATE, "google login processing... %s", result->data);
	
	if (result->data && result->httpstatus == 200)
		tmp = strstr (result->data, "Auth=");
		
	if (tmp) {
		gchar *ttmp = tmp; 
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		node_source_set_auth_token (node, g_strdup_printf ("GoogleLogin auth=%s", ttmp + 5));

		/* now that we are authenticated trigger updating to start data retrieval */
		if (!(flags & GOOGLE_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (subscription, flags);

		/* process any edits waiting in queue */
		google_reader_api_edit_process (node->source);

	} else {
		debug (DEBUG_UPDATE, "google reader login failed! no Auth token found in result!");
		subscription->node->available = FALSE;

		g_free (subscription->updateError);
		subscription->updateError = g_strdup (_("Login failed!"));
		node_source_set_state (node, NODE_SOURCE_STATE_NO_AUTH);
		
		auth_dialog_new (subscription, flags);
	}
}

/**
 * Perform a login to Google Reader, if the login completes the 
 * GoogleSource will have a valid Auth token and will have loginStatus to 
 * GOOGLE_SOURCE_LOGIN_ACTIVE.
 */
void
google_source_login (GoogleSourcePtr source, guint32 flags) 
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
		source->root->source->api.login,
		subscription->updateState,
		NULL	// auth is done via POST below!
	);

	/* escape user and password as both are passed using an URI */
	username = g_uri_escape_string (subscription->updateOptions->username, NULL, TRUE);
	password = g_uri_escape_string (subscription->updateOptions->password, NULL, TRUE);

	request->postdata = g_strdup_printf (source->root->source->api.login_post, username, password);
	request->options = update_options_copy (subscription->updateOptions);
	
	g_free (username);
	g_free (password);
	
	node_source_set_state (source->root, NODE_SOURCE_STATE_IN_PROGRESS);

	update_job_new (source, request, google_source_login_cb, source->root, flags | UPDATE_REQUEST_NO_FEED);
}

/* node source type implementation */

static void
google_source_auto_update (Node *node)
{
	if (node->source->loginState == NODE_SOURCE_STATE_NONE) {
		node_source_update (node);
		return;
	}

	if (node->source->loginState == NODE_SOURCE_STATE_IN_PROGRESS)
		return; /* the update will start automatically anyway */

	debug (DEBUG_UPDATE, "google_source_auto_update()");
	subscription_auto_update (node->subscription);
}

static void
google_source_import (Node *node)
{
	opml_source_import (node);
	
	node->subscription->type = &googleSourceOpmlSubscriptionType;
	if (!node->data)
		node->data = (gpointer) google_source_new (node);
}

static void
google_source_export (Node *node)
{
	opml_source_export (node);
}

static gchar *
google_source_get_feedlist (Node *node)
{
	return opml_source_get_feedlist (node);
}

static void 
google_source_remove (Node *node)
{ 
	opml_source_remove (node);
}

static Node *
google_source_add_subscription (Node *node, subscriptionPtr subscription) 
{ 
	// FIXME: determine correct category from parent folder name
	google_reader_api_edit_add_subscription (node->source, subscription->source, NULL);

	subscription_free (subscription);
	
	return NULL;
}

static gchar *
google_source_get_stream_id_for_node (Node *node)
{
	if (!node->subscription)
		return NULL;

	return g_strdup (metadata_list_get (node->subscription->metadata, "feed-id"));
}

static void
google_source_remove_node (Node *node, Node *child) 
{ 
	g_autofree gchar	*url = NULL, *streamId = NULL;
	GoogleSourcePtr		source = node->data;
	
	// FIXME: Check for login?

	if (child == node) {
		feedlist_node_removed (child);
		return;
	}

	url = g_strdup (child->subscription->source);
	streamId = google_source_get_stream_id_for_node (child);

	feedlist_node_removed (child);

	/* propagate the removal only if there aren't other copies */
	if (!feedlist_find_node (source->root, NODE_BY_URL, url))
		google_reader_api_edit_remove_subscription (node->source, streamId, google_source_get_stream_id_for_node);
}

/* GUI callbacks */

static void
on_google_source_selected (GtkDialog *dialog,
                           gint response_id,
                           gpointer user_data) 
{
	Node	*node;

	if (response_id == GTK_RESPONSE_OK) {
		node = node_new ("node_source");
		node_source_new (node, SOURCE_ID, gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "serverEntry")))));
		node_set_title (node, gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "nameEntry")))));
		
		subscription_set_auth_info (node->subscription,
		                            gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "userEntry")))),
		                            gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET(dialog), "passwordEntry")))));
		                            
		node->data = google_source_new (node);
		feedlist_node_added (node);
		node_source_update (node);
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

static void
ui_google_source_get_account_info (void)
{
	GtkWidget	*dialog;
	
	dialog = liferea_dialog_new ("google_source");
	
	g_signal_connect (G_OBJECT (dialog), "response",
			  G_CALLBACK (on_google_source_selected), 
			  NULL);
}

static void
google_source_cleanup (Node *node)
{
	GoogleSourcePtr reader = (GoogleSourcePtr) node->data;
	google_source_free (reader);
	node->data = NULL;
}

static void 
google_source_item_set_flag (Node *node, itemPtr item, gboolean newStatus)
{
	google_reader_api_edit_mark_starred (node->source, item->sourceId, node->subscription->source, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
google_source_item_mark_read (Node *node, itemPtr item, gboolean newStatus)
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
google_source_convert_to_local (Node *node)
{
	node_source_set_state (node, NODE_SOURCE_STATE_MIGRATE);
}

/* node source provider definition */

typedef struct {
	GObject parent_instance;
} GoogleSourceProvider;

typedef struct {
	GObjectClass parent_class;
} GoogleSourceProviderClass;

static void google_source_provider_init(GoogleSourceProvider *self) { }
static void google_source_provider_class_init(GoogleSourceProviderClass *klass) { }
static void google_source_provider_interface_init(NodeSourceProviderInterface *iface) {
	iface->id                  = SOURCE_ID;
	iface->name                = N_("Google Reader API (FreshRSS, FeedHQ, Miniflux...)");
	iface->capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION | 
				     NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
				     NODE_SOURCE_CAPABILITY_ADD_FEED |
				     NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
				     NODE_SOURCE_CAPABILITY_CAN_LOGIN |
				     NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL;
	iface->feedSubscriptionType = &googleSourceFeedSubscriptionType;
	iface->sourceSubscriptionType = &googleSourceOpmlSubscriptionType;
	iface->source_new          = ui_google_source_get_account_info;
	iface->source_delete       = google_source_remove;
	iface->source_import       = google_source_import;
	iface->source_export       = google_source_export;
	iface->source_get_feedlist = google_source_get_feedlist;
	iface->source_auto_update  = google_source_auto_update;
	iface->free                = google_source_cleanup;
	iface->item_set_flag       = google_source_item_set_flag;
	iface->item_mark_read      = google_source_item_mark_read;
	iface->add_subscription    = google_source_add_subscription;
	iface->remove_node         = google_source_remove_node;
	iface->convert_to_local    = google_source_convert_to_local;
}

#define GOOGLE_TYPE_SOURCE_PROVIDER (google_source_provider_get_type())

G_DEFINE_TYPE_WITH_CODE(GoogleSourceProvider, google_source_provider, G_TYPE_OBJECT,
	G_IMPLEMENT_INTERFACE(NODE_TYPE_SOURCE_PROVIDER, google_source_provider_interface_init))

void
google_source_register (void)
{
	NodeSourceProviderInterface *iface = NODE_SOURCE_PROVIDER_GET_IFACE (g_object_new (GOOGLE_TYPE_SOURCE_PROVIDER, NULL));
	node_source_type_register (iface);

	/* register all source types that are google like */
	/*type = g_new0 (struct nodeSourceType, 1);
	memcpy (type, google_source_get_type (), sizeof(struct nodeSourceType));
	type->name = N_("Miniflux");
	type->id = "fl_miniflux";
	node_source_type_register (type);*/
}
