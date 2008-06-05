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

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "feedlist.h"
#include "node.h"
#include "update.h"
#include "xml.h"
#include "ui/ui_dialog.h"
#include "fl_sources/google_source.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"
#include <libxml/xpath.h>
#include "google_source_edit.h"
#include "subscription.h"
#include "item_state.h"
#include "libxml/xpath.h"

/** default Google reader subscription list update interval = once a day */
#define GOOGLE_SOURCE_UPDATE_INTERVAL 60*60*24

/**
 * when this is set to true, and google_source_item_mark_read is called, 
 * it will do nothing. This is a small hack, so that whenever google source
 * wants to set and item as read/unread within this file, it should not do 
 * any network calls.
 */
gboolean googleSourceBlockEditHack = FALSE ;
extern struct subscriptionType googleSourceFeedSubscriptionType;
static struct subscriptionType googleSourceOpmlSubscriptionType;


/** create a google source with given node as root */ 
GoogleSourcePtr google_source_new(nodePtr node) 
{
	GoogleSourcePtr source = g_new0(struct GoogleSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new(); 
	source->loginState = GOOGLE_SOURCE_STATE_NONE; 
	return source;
}


void google_source_free(GoogleSourcePtr gsource) 
{
	if (!gsource) return ;

	g_queue_free(gsource->actionQueue) ;
	g_free(gsource);
}

/* subscription list merging functions */

static void
google_source_check_for_removal (nodePtr node, gpointer user_data)
{
	gchar		*expr = NULL;

	if ( g_str_equal(node->subscription->source, GOOGLE_READER_BROADCAST_FRIENDS_URL) ) 
		return ; 
	if (IS_FEED (node)) {
		expr = g_strdup_printf ("/object/list[@name='subscriptions']/object/string[@name='id'][. = 'feed/%s']", node->subscription->source);
	} else {
		g_warning("opml_source_check_for_removal(): This should never happen...");
		return;
	}
	
	if (!xpath_find ((xmlNodePtr)user_data, expr)) {
		debug1 (DEBUG_UPDATE, "removing %s...", node_get_title (node));
		feedlist_node_removed (node);
	} else {
		debug1 (DEBUG_UPDATE, "keeping %s...", node_get_title (node));
	}
	g_free (expr);
}

static void
google_source_migrate_node(nodePtr node, gpointer userdata) 
{
	/* scan the node for bad ID's, if so, brutally remove the node */
	itemSetPtr itemset = node_get_itemset(node);
	GList *iter = itemset->ids;
	for (; iter; iter = g_list_next(iter)) {
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item && item->sourceId) {
			if (!g_str_has_prefix(item->sourceId, "tag:google.com")) {
				debug1(DEBUG_UPDATE, "Item with sourceId [%s] will be deleted.", item->sourceId);
				db_item_remove(GPOINTER_TO_UINT(iter->data));
			} 
		}
		if (item) item_unload (item) ;
	}

	/* cleanup */
	itemset_free(itemset);
}

nodePtr
google_source_get_root_from_node (nodePtr node)
{ 
	while (node->parent->source == node->source) 
		node = node->parent;
	return node;
}

/**
 * Add "broadcast-friends" to the list of subscriptions if required 
 */
static void
google_source_add_broadcast_subscription (GoogleSourcePtr gsource)
{
	gchar* title = "Friend's Shared Items"; 
	GSList * iter = NULL ; 
	nodePtr node ; 

	iter = gsource->root->children; 
	while (iter) { 
		node = (nodePtr)iter->data ; 
		if (!node->subscription || !node->subscription->source) 
			continue;
		if (g_str_equal (node->subscription->source, GOOGLE_READER_BROADCAST_FRIENDS_URL)) {
			update_state_set_cookies (node->subscription->updateState, 
			                          gsource->sid);
			return;
		}
		iter = g_slist_next (iter);
	}

	/* aha! add it! */

	node = node_new ();
	node_set_title (node, title);
	node_set_type (node, feed_get_node_type ());
	node_set_data (node, feed_new ());

	node_set_subscription (node, subscription_new (GOOGLE_READER_BROADCAST_FRIENDS_URL, NULL, NULL));
	node->subscription->type = &googleSourceFeedSubscriptionType;
	node_set_parent (node, gsource->root, -1);
	feedlist_node_imported (node);
	
	update_state_set_cookies (node->subscription->updateState, gsource->sid);
	subscription_update (node->subscription, FEED_REQ_RESET_TITLE);
	subscription_update_favicon (node->subscription);
}

static void
google_source_merge_feed (xmlNodePtr match, gpointer user_data)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr)user_data;
	nodePtr		node;
	GSList		*iter;
	xmlNodePtr	xml;
	xmlChar		*title, *id ;
	gchar           *url ;

	xml = xpath_find (match, "./string[@name='title']");
	if (xml)
		title = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
		
	xml = xpath_find (match, "./string[@name='id']");
	if (xml)
		id = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
	url = g_strdup(id+strlen("feed/"));

	/* check if node to be merged already exists */
	iter = gsource->root->children;
	while (iter) {
		node = (nodePtr)iter->data;
		if (g_str_equal (node->subscription->source, url)) {
			update_state_set_cookies (node->subscription->updateState, gsource->sid);
			node->subscription->type = &googleSourceFeedSubscriptionType;
			goto cleanup ;
		}
		iter = g_slist_next (iter);
	}
	
	/* Note: ids look like "feed/http://rss.slashdot.org" */
	if (id && title) {

		debug2 (DEBUG_UPDATE, "adding %s (%s)", title, url);
		node = node_new ();
		node_set_title (node, title);
		node_set_type (node, feed_get_node_type ());
		node_set_data (node, feed_new ());
		
		node_set_subscription (node, subscription_new (url, NULL, NULL));
		node->subscription->type = &googleSourceFeedSubscriptionType;
		node_set_parent (node, gsource->root, -1);
		feedlist_node_imported (node);
		
		update_state_set_cookies (node->subscription->updateState, gsource->sid);
		/**
		 * @todo mark the ones as read immediately after this is done
		 * the feed as retrieved by this has the read and unread
		 * status inherently.
		 */
		subscription_update (node->subscription,  FEED_REQ_RESET_TITLE);
		subscription_update_favicon (node->subscription);
	} else 
		g_warning("Unable to parse subscription information from Google");

cleanup:
	if (id)
		xmlFree (id);
	if (title)
		xmlFree (title);
	g_free (url) ;
}

/**
 * Initialize the reader and subscription for a login. Note that this does not do the
 * actual updating. You *MUST* follow this call up with a call to subscription_update to 
 * complete the call. If from a prepare_update_request, then use the following hack:
 * retrieve the subscription's source and set the request's source to that.
 */
void
google_source_login (subscriptionPtr subscription, guint32 flags) 
{ 
	GoogleSourcePtr gsource = (GoogleSourcePtr) subscription->node->data;
	gchar *source;
	g_assert(gsource);
	
	if (gsource->loginState != GOOGLE_SOURCE_STATE_NONE) {
		/* this should not happen, as of now, we assume the session
		 * doesn't expire. */
		debug1(DEBUG_UPDATE, "Logging in while login state is %d\n", 
			     gsource->loginState);
	}

	/* @todo: The following is a severe security issue, since the password
	 * is being sent in the URL itself. As of now, since 'subscription'
	 * does not have a postdata support, I don't have much choice. */
	source = g_strdup_printf ( GOOGLE_READER_LOGIN_URL "?" 
				   GOOGLE_READER_LOGIN_POST,
	                     	  subscription->updateOptions->username,
	                          subscription->updateOptions->password);

	gsource->loginState = GOOGLE_SOURCE_STATE_IN_PROGRESS ;
	subscription_set_source (subscription, source);
	g_free (source);
}

/* OPML subscription type implementation */

static void
google_subscription_opml_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr)subscription->node->data;
	
	if (result->data) {
		xmlDocPtr doc = xml_parse (result->data, result->size, FALSE, NULL);
		if(doc) {		
			xmlNodePtr root = xmlDocGetRootElement (doc);
			
			/* Go through all existing nodes and remove those whose
			   URLs are not in new feed list. Also removes those URLs
			   from the list that have corresponding existing nodes. */
			node_foreach_child_data (subscription->node, google_source_check_for_removal, (gpointer)root);
			node_foreach_child_data (subscription->node, google_source_migrate_node, (gpointer) root);
						
			opml_source_export (subscription->node);	/* save new feed list tree to disk 
									   to ensure correct document in 
									   next step */

			xpath_foreach_match (root, "/object/list[@name='subscriptions']/object",
			                     google_source_merge_feed,
			                     (gpointer)gsource);
			google_source_add_broadcast_subscription (gsource) ;

			opml_source_export (subscription->node);	/* save new feeds to feed list */
						   
			subscription->node->available = TRUE;
			xmlFreeDoc (doc);
		} else { 
			/** @todo The session seems to have expired */
			g_warning("Unable to parse OPML list from google, the session might have expired.\n");
		}
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "google_subscription_opml_cb(): ERROR: failed to get subscription list!\n");
	}

	if (!(flags & GOOGLE_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));

}

static void
google_subscription_login_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr)subscription->node->data;
	gchar		*tmp = NULL;
	GTimeVal	now;
	
	debug0 (DEBUG_UPDATE, "google login processing...");
	
	if (subscription->updateError) {
		g_free (subscription->updateError);
		subscription->updateError = NULL;
	}
	
	g_assert (!gsource->sid);
	
	if (result->data)
		tmp = strstr (result->data, "SID=");
		
	if (tmp) {
		gsource->sid = tmp;
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		gsource->sid = g_strdup (gsource->sid);
		debug1 (DEBUG_UPDATE, "google reader SID found: %s", gsource->sid);
		subscription->node->available = TRUE;
		
		/* now that we are authenticated retrigger updating to start data retrieval */
		gsource->loginState = GOOGLE_SOURCE_STATE_ACTIVE;
		if ( ! (flags & GOOGLE_SOURCE_UPDATE_ONLY_LOGIN) ) 
			subscription_update (subscription, flags);

		/* process any edits waiting in queue */
		google_source_edit_process (gsource);

	} else {
		debug0 (DEBUG_UPDATE, "google reader login failed! no SID found in result!");
		subscription->node->available = FALSE;

		g_free(subscription->updateError);
		subscription->updateError = g_strdup (_("Google Reader login failed!"));
		gsource->loginState = GOOGLE_SOURCE_STATE_NONE;
	}
}

static void
google_opml_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr)subscription->node->data;
	
	/* Note: the subscription update design supports only request->response pairs
	   and doesn't anticipate multi-step login procedures. Therefore we have only
	   one result callback and must use the "reader" structure to determine what
	   currently needs to be done. */
	   
	if (gsource->sid)
		google_subscription_opml_cb (subscription, result, flags);
	else
		google_subscription_login_cb (subscription, result, flags);
}

static gboolean
google_opml_subscription_prepare_update_request (subscriptionPtr subscription, struct updateRequest *request)
{
	GoogleSourcePtr	gsource = (GoogleSourcePtr)subscription->node->data;
	
	g_assert(gsource);
	if (gsource->loginState == GOOGLE_SOURCE_STATE_NONE) {
		debug0(DEBUG_UPDATE, "GoogleSource: login");
		google_source_login(subscription, 0) ;

		gsource->loginState = GOOGLE_SOURCE_STATE_IN_PROGRESS ;
		/* The subscription is updated, but the request has not yet been updated */
		update_request_set_source(request, subscription->source); 
		return TRUE;
	}
	debug1 (DEBUG_UPDATE, "updating Google Reader subscription (node id %s)", subscription->node->id);
	
	update_request_set_source(request, GOOGLE_READER_SUBSCRIPTION_LIST_URL);
	
	update_state_set_cookies (request->updateState, gsource->sid);
	
	return TRUE;
}

/* OPML subscription type definition */

static struct subscriptionType googleSourceOpmlSubscriptionType = {
	google_opml_subscription_prepare_update_request,
	google_opml_subscription_process_update_result,
	NULL	/* free */
};


static void
google_source_xml_unlink_node(xmlNodePtr node, gpointer data) 
{
	xmlUnlinkNode(node) ;
	xmlFreeNode(node) ;
}


/** 
 * Shared actions needed during import and when subscribing,
 * Only feedlist_node_added() will be done only when subscribing. (FIXME)
 */
void
google_source_setup (nodePtr parent, nodePtr node)
{
	node->icon = create_pixbuf ("fl_google.png");
	
	node_set_type (node, node_source_get_node_type ());
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
	googleSourceBlockEditHack = TRUE ; 
	opml_source_remove (node);
	googleSourceBlockEditHack = FALSE ;
}


nodePtr
google_source_add_subscription(nodePtr node, nodePtr parent, subscriptionPtr subscription) 
{ 
	g_assert(!googleSourceBlockEditHack);
	debug_enter("google_source_add_subscription") ;
	nodePtr child = node_new ();

	debug0(DEBUG_UPDATE, "GoogleSource: Adding a new subscription"); 
	node_set_type (child, feed_get_node_type ());
	node_set_data (child, feed_new ());

	node_set_subscription(child, subscription) ;
	child->subscription->type = &googleSourceFeedSubscriptionType;
	
	node_set_title(child, _("New Subscription")) ;

	google_source_edit_add_subscription(google_source_get_root_from_node(node)->data, subscription->source);
	
	debug_exit("google_source_add_subscription") ;
	return child ; 
}

void
google_source_remove_node(nodePtr node, nodePtr child) 
{ 
	if (child == node) { 
		feedlist_node_removed(child);
		return; 
	}
	g_assert(!googleSourceBlockEditHack);
	google_source_edit_remove_subscription(google_source_get_root_from_node(node)->data, child->subscription->source); 
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
		node = node_new ();
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

	
	dialog = liferea_dialog_new (PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "google_source.glade", "google_source_dialog");
	
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
	NODE_SOURCE_TYPE_API_VERSION,
	"fl_google",
	N_("Google Reader"),
	N_("Integrate the feed list of your Google Reader account. Liferea will "
	   "present your Google Reader subscription as a read-only subtree in the feed list."),
	NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION | NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST,
	google_source_init,
	google_source_deinit,
	ui_google_source_get_account_info,
	google_source_remove,
	google_source_import,
	google_source_export,
	google_source_get_feedlist,
	google_source_update,
	google_source_auto_update,
	google_source_cleanup,
	google_source_item_mark_read,
	NULL, /* add_folder */
	google_source_add_subscription,
	google_source_remove_node
};

nodeSourceTypePtr
google_source_get_type(void)
{
	return &nst;
}

