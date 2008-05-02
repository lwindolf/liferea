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

/** default Google reader subscription list update interval = once a day */
#define GOOGLE_SOURCE_UPDATE_INTERVAL 60*60*24
#define GOOGLE_SOURCE_BROADCAST_FRIENDS_URI "http://www.google.com/reader/atom/user/-/state/com.google/broadcast-friends" 
/**
 * when this is set to true, and google_source_item_mark_read is called, 
 * it will do nothing. This is a small hack, so that whenever google source
 * wants to set and item as read/unread within this file, it should not do 
 * any network calls.
 */
gboolean __mark_read_hack = FALSE ;


typedef struct reader {
	nodePtr		root;	/**< the root node in the feed list */
	gchar		*sid;	/**< session id */
	GTimeVal	*lastSubscriptionListUpdate;
	GQueue          *editQueue;
	enum { 
		READER_STATE_NONE = 0,
		READER_STATE_IN_PROGRESS,
		READER_STATE_ACTIVE
	} loginState ; 
} *readerPtr;

/**
 * A structure to indicate an edit to the google reader "database".
 * These edits are put in a queue and processed in sequential order
 * so that google does not end up processing the requests in an 
 * unintended order.
 */
typedef struct edit { 
	gchar* guid;		/**< guid of the item to edit */
	gchar* feedUrl;	/**< url of the feed containing the item, or the "stream" to get the list for */
	enum { 
		EDIT_ACTION_MARK_READ,
		EDIT_ACTION_MARK_UNREAD,
		EDIT_ACTION_TRACKING_MARK_UNREAD /**< every UNREAD request, should be followed by tracking-kept-unread */
	} action;
		
} *editPtr ; 

/**
 * Create an edit structure.
 *
 * @return editPtr newly allocated edit structure
 */
editPtr 
google_source_edit_new (void)
{
	editPtr edit = g_new0 (struct edit, 1);
	return edit;
}

/**
 * Free an allocated edit structure
 * @param edit the a pointer to the edit structure to delete.
 */
void 
google_source_edit_free (editPtr edit)
{ 
	g_free (edit->guid);
	g_free (edit->feedUrl);
	g_free (edit);
}

void google_source_edit_process(readerPtr reader);

static void
google_source_edit_action_complete(const struct updateResult* const result, gpointer userdata, updateFlags flags) 
{ 
	readerPtr reader = (readerPtr) userdata;

	/* process anything else waiting on the edit queue */
	google_source_edit_process (reader);
}

/**
 * Callback from an token request, and sends the actual edit request
 * in processing
 */
static void
google_source_edit_token_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{ 
	if (result->returncode != 0) { 
		printf ("sigh... something went wrong.\n");
		return;
	}

	readerPtr reader = (readerPtr) userdata; 
	const gchar* token = result->data; 

	if (g_queue_is_empty (reader->editQueue))
		return;

	editPtr edit = g_queue_peek_head (reader->editQueue);
	gchar* source = NULL ; 
	gchar* postdata = NULL ; 
	source = g_strdup_printf ("http://www.google.com/reader/api/0/edit-tag?client=liferea");
	updateRequestPtr request = update_request_new ();
	request->updateState = update_state_copy (reader->root->subscription->updateState);
	request->options = update_options_copy (reader->root->subscription->updateOptions) ;
	request->source = source; /* already strdup-ed */
	update_state_set_cookies (request->updateState, reader->sid);
	gchar* tmp = g_strdup_printf ("%s", edit->feedUrl); 
	gchar* s_escaped = common_uri_escape (tmp);
	g_free (tmp);
	
	gchar* a_escaped;
	gchar* i_escaped = common_uri_escape (edit->guid);
	
	if (edit->action == EDIT_ACTION_MARK_UNREAD) {
		a_escaped = common_uri_escape ("user/-/state/com.google/kept-unread");
		gchar *r_escaped = common_uri_escape ("user/-/state/com.google/read");
		postdata = g_strdup_printf ("i=%s&s=%s&a=%s&r=%s&ac=edit-tags&T=%s", i_escaped, s_escaped, a_escaped, r_escaped, token);
		g_free (r_escaped);
	}
	else if (edit->action == EDIT_ACTION_MARK_READ) { 
		a_escaped = common_uri_escape ("user/-/state/com.google/read");
		postdata = g_strdup_printf ("i=%s&s=%s&a=%s&ac=edit-tags&T=%s", i_escaped, s_escaped, a_escaped, token);
	}
	else if (edit->action == EDIT_ACTION_TRACKING_MARK_UNREAD) {
		a_escaped = common_uri_escape ("user/-/state/com.google/tracking-kept-unread");
		postdata = g_strdup_printf ("i=%s&s=%s&a=%s&ac=edit-tags&async=true&T=%s", i_escaped, s_escaped, a_escaped, token);
	} else {
		g_assert (FALSE);
	}
	
	g_free (s_escaped);
	g_free (a_escaped); 
	g_free (i_escaped);
	
	debug1 (DEBUG_UPDATE, "google_source: postdata [%s]", postdata);

	if (postdata) 
		request->postdata = g_strdup (postdata);

	update_execute_request (reader, request, google_source_edit_action_complete, 
	                        reader, 0);

	edit = g_queue_pop_head (reader->editQueue);
	google_source_edit_free (edit) ;

	g_free (postdata); 
}

/**
 * process the topmost element on the queue (if there are any).
 */
void
google_source_edit_process (readerPtr reader)
{ 
	g_assert (reader);
	if (g_queue_is_empty (reader->editQueue))
		return;
	
	/*
 	* Google reader has a system of tokens. So first, I need to request a 
 	* token from google, before I can make the actual edit request. The
 	* code here is the token code, the actual edit commands are in 
 	* google_source_edit_token_cb
	 */
	updateRequestPtr request = update_request_new ();
	request->updateState = update_state_copy (reader->root->subscription->updateState);
	request->options = update_options_copy (reader->root->subscription->updateOptions);
	request->source = g_strdup ("http://www.google.com/reader/api/0/token");
	update_state_set_cookies (request->updateState, reader->sid);

	update_execute_request (reader, request, google_source_edit_token_cb, 
	                        reader, 0);
}

/**
 * Push an edit action onto the processing queue. This is
 * a helper function, use google_source_edit_push_safe instead,
 * as this may not work if the reader is not yet connected.
 */
void
google_source_edit_push_ (readerPtr reader, editPtr edit)
{ 
	g_assert (reader->editQueue);
	g_queue_push_tail (reader->editQueue, edit) ;
}

static void google_source_update_subscription_list (nodePtr node, guint flags);

/* subscription list merging functions */

static void
google_source_check_for_removal (nodePtr node, gpointer user_data)
{
	gchar		*expr = NULL;

	if ( g_str_equal(node->subscription->source, GOOGLE_SOURCE_BROADCAST_FRIENDS_URI) ) 
		return ; 
	if (IS_FEED (node)) {
		expr = g_strdup_printf ("/object/list[@name='subscriptions']/object/string[@name='title'][. = '%s']", node_get_title (node));
	} else {
		g_warning("opml_source_check_for_removal(): This should never happen...");
		return;
	}
	
	if (!xpath_find ((xmlNodePtr)user_data, expr)) {
		debug1 (DEBUG_UPDATE, "removing %s...", node_get_title (node));
		if (feedlist_get_selected() == node)
			ui_feedlist_select (NULL);
		node_request_remove (node);
	} else {
		debug1 (DEBUG_UPDATE, "keeping %s...", node_get_title (node));
	}
	g_free (expr);
}

static nodePtr
google_source_get_root_from_node (nodePtr node)
{ 
	while (node->parent->source == node->source) {
		node = node->parent;
	}

	return node;
}

static void google_source_login (subscriptionPtr subscription, guint32 flags);

/**
 * Add an edit action onto the edit queue. The edit action may not be 
 * immediately processed if we are not yet connected to google, or if
 * there are other edits on the queue.
 */
static void 
google_source_edit_push_safe (nodePtr root, editPtr edit)
{
	readerPtr reader = (readerPtr) root->data ; 

	/** @todo any flags I should specify? */
	if (!reader || reader->loginState == READER_STATE_NONE) {
		google_source_login (root->subscription, 0);
		subscription_update(root->subscription, 0) ;
		google_source_edit_push_ (root->data, edit);
	} else if (reader->loginState == READER_STATE_IN_PROGRESS) {
		google_source_edit_push_ (root->data, edit);
	} else { 
		google_source_edit_push_ (root->data, edit);
		google_source_edit_process (root->data);
	}
	
}

/**
 * Mark an item as read on the google server.
 */
static void
google_source_item_mark_read (nodePtr node, itemPtr item, 
                              gboolean newStatus)
{
	/**
	 * This global is an internal hack so that this source file
	 * may call item_state_set_read without any network calls being
	 * made. @see google_source_edit_action_cb
	 */
	if (__mark_read_hack)
		return;
	nodePtr root = google_source_get_root_from_node (node);
	editPtr edit = google_source_edit_new ();
	edit->guid = g_strdup (item->sourceId);
	edit->feedUrl = g_strdup (node->subscription->source + 
	                           strlen ("http://www.google.com/reader/atom/"));
	edit->action = newStatus ? EDIT_ACTION_MARK_READ :
	                           EDIT_ACTION_MARK_UNREAD;

	google_source_edit_push_safe (root, edit);

	if (newStatus == FALSE) { 
		/*
		 * According to the Google Reader API, to mark an item unread, 
		 * I also need to mark it as tracking-kept-unread in a separate
		 * network call.
		 */
		edit = google_source_edit_new ();
		edit->guid = g_strdup (item->sourceId);
		edit->feedUrl = g_strdup (node->subscription->source + 
		                           strlen ("http://www.google.com/reader/atom/"));
		edit->action = EDIT_ACTION_TRACKING_MARK_UNREAD;
		google_source_edit_push_safe (root, edit);
	}
}

static struct subscriptionType googleReaderFeedSubscriptionType;
 
static void
google_source_add_shared (readerPtr reader)
{
	gchar* title = "Friend's Shared Items"; 
	GSList * iter = NULL ; 
	nodePtr node ; 

	iter = reader->root->children; 
	while (iter) { 
		node = (nodePtr)iter->data ; 
		if (!node->subscription || !node->subscription->source) 
			continue;
		if (g_str_equal (node->subscription->source, GOOGLE_SOURCE_BROADCAST_FRIENDS_URI)) {
			update_state_set_cookies (node->subscription->updateState, 
			                          reader->sid);
			return;
		}
		iter = g_slist_next (iter);
	}

	/* aha! add it! */

	node = node_new ();
	node_set_title (node, title);
	node_set_type (node, feed_get_node_type ());
	node_set_data (node, feed_new ());

	node_set_subscription (node, subscription_new (GOOGLE_SOURCE_BROADCAST_FRIENDS_URI, NULL, NULL));
	node->subscription->type = &googleReaderFeedSubscriptionType;
	node_add_child (reader->root, node, -1);
	update_state_set_cookies (node->subscription->updateState, reader->sid);

	subscription_update (node->subscription, FEED_REQ_RESET_TITLE);
}

static void
google_source_merge_feed (xmlNodePtr match, gpointer user_data)
{
	readerPtr	reader = (readerPtr)user_data;
	nodePtr		node;
	GSList		*iter;
	xmlNodePtr	xml;
	xmlChar		*title, *id ;
	gchar           *url, *origUrl ;

	xml = xpath_find (match, "./string[@name='title']");
	if (xml)
		title = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
		
	xml = xpath_find (match, "./string[@name='id']");
	if (xml)
		id = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
	url = g_strdup(id+5);

	/* check if node to be merged already exists */
	iter = reader->root->children;
	while (iter) {
		node = (nodePtr)iter->data;
		if (g_str_equal (node->subscription->source, url)) {
			update_state_set_cookies (node->subscription->updateState, reader->sid);
			node->subscription->type = &googleReaderFeedSubscriptionType;
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
		node->subscription->type = &googleReaderFeedSubscriptionType ;
		node_add_child (reader->root, node, -1);
		update_state_set_cookies (node->subscription->updateState, reader->sid);
		/**
		 * @todo mark the ones as read immediately after this is done
		 * the feed as retrieved by this has the read and unread
		 * status inherently.
		 */
		subscription_update (node->subscription,  FEED_REQ_RESET_TITLE);
	}

cleanup:
	if (id)
		xmlFree (id);
	if (title)
		xmlFree (title);
	g_free (url) ;
}

/**
 * Initialize the reader and subscription for a login. Note that this does not do the
 * actual updating. You can follow this call up with a call to subscription_update to 
 * complete the call. If from a prepare_update_request, then use the following hack:
 * retrieve the subscription's source and set the request's source to that.
 */
static void
google_source_login (subscriptionPtr subscription, guint32 flags) 
{ 
	readerPtr reader = (readerPtr) subscription->node->data;
	gchar *source;
	
	
	/* We are not logged in yet, we need to perform a login first and retrigger the update later... */
	
	if (!reader) {
		subscription->node->data = reader = g_new0 (struct reader, 1);
		reader->root = subscription->node;
		reader->editQueue = g_queue_new ();
	}
	source = g_strdup_printf ("https://www.google.com/accounts/ClientLogin?service=reader&Email=%s&Passwd=%s&source=liferea&continue=http://www.google.com",
	                     	  subscription->updateOptions->username,
	                          subscription->updateOptions->password);
	
	debug2 (DEBUG_UPDATE, "login to Google Reader source %s (node id %s)", source, subscription->node->id);
	subscription_set_source (subscription, source);
	
	g_free (source);
	
}

/* OPML subscription type implementation */

static void
google_subscription_opml_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	readerPtr	reader = (readerPtr)subscription->node->data;
	
	if (result->data) {
		xmlDocPtr doc = xml_parse (result->data, result->size, FALSE, NULL);
		if(doc) {		
			xmlNodePtr root = xmlDocGetRootElement (doc);
			
			/* Go through all existing nodes and remove those whose
			   URLs are not in new feed list. Also removes those URLs
			   from the list that have corresponding existing nodes. */
			node_foreach_child_data (subscription->node, google_source_check_for_removal, (gpointer)root);
						
			opml_source_export (subscription->node);	/* save new feed list tree to disk 
									   to ensure correct document in 
									   next step */

			xpath_foreach_match (root, "/object/list[@name='subscriptions']/object",
			                     google_source_merge_feed,
			                     (gpointer)reader);
			google_source_add_shared (reader) ;

			opml_source_export (subscription->node);	/* save new feeds to feed list */
						   
			subscription->node->available = TRUE;
			xmlFreeDoc (doc);
		}
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "google_subscription_opml_cb(): ERROR: failed to get subscription list!\n");
	}

	node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));

}

static void
google_subscription_login_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	readerPtr	reader = (readerPtr)subscription->node->data;
	gchar		*tmp;
	GTimeVal	now;
	
	debug0 (DEBUG_UPDATE, "google login processing...");
	
	if (subscription->updateError) {
		g_free (subscription->updateError);
		subscription->updateError = NULL;
	}
	
	g_assert (!reader->sid);
	
	if (result->data)
		tmp = strstr (result->data, "SID=");
		
	if (tmp) {
		reader->sid = tmp;
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		reader->sid = g_strdup (reader->sid);
		debug1 (DEBUG_UPDATE, "google reader SID found: %s", reader->sid);
		subscription->node->available = TRUE;
		
		/* now that we are authenticated retrigger updating to start data retrieval */
		g_get_current_time (&now);

		reader->loginState = READER_STATE_ACTIVE;
		subscription_update (subscription, 0);

		/* process any edits waiting in queue */
		google_source_edit_process (reader);

	} else {
		debug0 (DEBUG_UPDATE, "google reader login failed! no SID found in result!");
		subscription->node->available = FALSE;
		subscription->updateError = g_strdup (_("Google Reader login failed!"));
		reader->loginState = READER_STATE_NONE;
	}
}

static void
google_opml_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	readerPtr	reader = (readerPtr)subscription->node->data;
	
	/* Note: the subscription update design supports only request->response pairs
	   and doesn't anticipate multi-step login procedures. Therefore we have only
	   one result callback and must use the "reader" structure to determine what
	   currently needs to be done. */
	   
	if (reader->sid)
		google_subscription_opml_cb (subscription, result, flags);
	else
		google_subscription_login_cb (subscription, result, flags);
}

static gboolean
google_opml_subscription_prepare_update_request (subscriptionPtr subscription, struct updateRequest *request)
{
	readerPtr	reader = (readerPtr)subscription->node->data;
	
	if (!reader || !reader->sid) {
		google_source_login(subscription, 0) ;

		/* The subscription is updated, but the request has not yet been updated */
		update_request_set_source(request, subscription->source); 
		return TRUE;
	}

	debug1 (DEBUG_UPDATE, "updating Google Reader subscription (node id %s)", subscription->node->id);
	
	/** @todo should be replaced with neater code */
	g_free(request->source) ; 
	request->source = g_strdup("http://www.google.com/reader/api/0/subscription/list");
	
	update_state_set_cookies (request->updateState, reader->sid);
	
	return TRUE;
}

/* OPML subscription type definition */

static struct subscriptionType googleReaderOpmlSubscriptionType = {
	google_opml_subscription_prepare_update_request,
	google_opml_subscription_process_update_result,
	NULL	/* free */
};

static void
google_source_item_retrieve_status (xmlNodePtr entry, gpointer userdata)
{
	subscriptionPtr subscription = (subscriptionPtr) userdata;
	xmlNodePtr xml;
	nodePtr node = subscription->node;

	itemPtr item;
	xmlChar* id;
	
	xml = entry->children;
	g_assert (xml);
	g_assert (g_str_equal (xml->name, "id"));

	id = xmlNodeGetContent (xml);

	gboolean read = FALSE;
	for (xml = entry->children; xml; xml = xml->next ) {
		if (g_str_equal (xml->name, "category")) {
			xmlChar* label = xmlGetProp (xml, "label");
			if (!label)
				continue;

			if (g_str_equal (label, "read")) {
				debug1 (DEBUG_UPDATE, "%s will be marked as read\n", id);
				read = TRUE;
				break;
			}
			xmlFree (label);
		}
	}

	itemSetPtr itemset = node_get_itemset(node);
	GList *iter = itemset->ids;
	for (; iter; iter = g_list_next(iter)) {
		gchar *itemid = (gchar*) iter->data;

		/* this is extremely inefficient, multiple times loading */
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item && item->sourceId) {
			if (g_str_equal (item->sourceId, id) && item->readStatus != read) {

				__mark_read_hack = TRUE;
				item_state_set_read (item, read);
				__mark_read_hack = FALSE;

				item_unload (item);
				goto cleanup;
			}
		}
		if (item) item_unload (item) ;
	}

	debug1 (DEBUG_UPDATE, "google_source_item_retrieve_status(): [%s] didn't get an item :( \n", id);
cleanup:
	itemset_free (itemset);
	xmlFree (id);
}

static void
google_feed_subscription_process_update_result (subscriptionPtr subscription,
                                                const struct updateResult* const result, 
                                                updateFlags flags)
{
	feed_get_subscription_type ()->process_update_result (subscription, result, flags);

	if (result->data) {
		xmlDocPtr doc = xml_parse (result->data, result->size, FALSE, NULL);
		if (doc) {		
			int i ; 
			xmlNodePtr root = xmlDocGetRootElement (doc);
			xmlNodePtr entry = root->children ; 

			while (entry) { 
				if (!g_str_equal (entry->name, "entry")) {
					entry = entry->next;
					continue; /* not an entry */
				}

				google_source_item_retrieve_status (entry, subscription);
				entry = entry->next;
			}

			xmlFreeDoc (doc);
		} else { 
			debug0 (DEBUG_UPDATE, "google_feed_subscription_process_update_result(): Couldn't parse XML!");
			g_warning ("google_feed_subscription_process_update_result(): Couldn't parse XML!");
		}
	} else { 
		debug0 (DEBUG_UPDATE, "google_feed_subscription_process_update_result(): No data!");
		g_warning ("google_feed_subscription_process_update_result(): No data!");
	}
}

static gboolean
google_feed_subscription_prepare_update_request (subscriptionPtr subscription, 
                                                 struct updateRequest *request)
{
	debug0 (DEBUG_UPDATE, "preparing google reader feed subscription for update\n");
	readerPtr reader = (readerPtr) google_source_get_root_from_node (subscription->node)->data; 
	
	if (!reader ||!reader->sid) { 
		google_source_login (google_source_get_root_from_node (subscription->node)->subscription, 0);
		subscription_update(google_source_get_root_from_node (subscription->node)->subscription, 0) ;
		return FALSE;
	}
	debug0 (DEBUG_UPDATE, "Setting cookies for a Google Reader subscription");

	if ( !g_str_equal(request->source, GOOGLE_SOURCE_BROADCAST_FRIENDS_URI) ) { 
		gchar* newUrl = g_strdup_printf("http://www.google.com/reader/atom/feed/%s", request->source) ;
		update_request_set_source(request, newUrl) ;
		g_free (newUrl);
	}
	update_state_set_cookies (request->updateState, reader->sid);
	return TRUE;
}

static struct subscriptionType googleReaderFeedSubscriptionType = {
	google_feed_subscription_prepare_update_request,
	google_feed_subscription_process_update_result,
	NULL  /* free */
};

/** 
 * Shared actions needed during import and when subscribing,
 * Only node_add_child() will be done only when subscribing.
 */
void
google_source_setup (nodePtr parent, nodePtr node)
{
	node->icon = create_pixbuf ("fl_google.png");
	
	node_set_type (node, node_source_get_node_type ());
	if (parent) {
		gint pos;
		ui_feedlist_get_target_folder (&pos);
		node_add_child (parent, node, pos);
	}
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
	
	node->subscription->type = &googleReaderOpmlSubscriptionType;
	/** @todo get specific information like reader->sid from this? */ 
	
	for(iter = node->children; iter; iter = g_slist_next(iter) )
		((nodePtr) iter->data)->subscription->type = &googleReaderFeedSubscriptionType; 
}

void
google_source_export (nodePtr node)
{
	opml_source_export (node);
}

gchar* 
google_source_get_feedlist (nodePtr node)
{
	opml_source_get_feedlist (node);
}

void 
google_source_remove (nodePtr node)
{ 
	opml_source_remove (node);
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
		subscription->type = &googleReaderOpmlSubscriptionType ; 
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
google_source_free (nodePtr node)
{
	readerPtr reader = (readerPtr) node->data;
	
	g_free (reader->sid);	
	g_free (reader);
}

/* node source type definition */

static struct nodeSourceType nst = {
	NODE_SOURCE_TYPE_API_VERSION,
	"fl_google",
	N_("Google Reader"),
	N_("Integrate the feed list of your Google Reader account. Liferea will "
	   "present your Google Reader subscription as a read-only subtree in the feed list."),
	NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION,
	google_source_init,
	google_source_deinit,
	ui_google_source_get_account_info,
	google_source_remove,
	google_source_import,
	google_source_export,
	google_source_get_feedlist,
	google_source_update,
	google_source_auto_update,
	google_source_free,
	google_source_item_mark_read
};

nodeSourceTypePtr
google_source_get_type(void)
{
	return &nst;
}

