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
#define GOOGLE_SOURCE_BROADCAST_FRIENDS_URI "http://www.google.com/reader/atom/user/-/state/com.google/broadcast-friends" 
/**
 * when this is set to true, and google_source_item_mark_read is called, 
 * it will do nothing. This is a small hack, so that whenever google source
 * wants to set and item as read/unread within this file, it should not do 
 * any network calls.
 */
static gboolean __mark_read_hack = FALSE ;
static struct subscriptionType googleReaderFeedSubscriptionType;
static struct subscriptionType googleReaderOpmlSubscriptionType;

readerPtr google_source_reader_new(nodePtr node) 
{
	readerPtr reader = g_new0(struct reader, 1) ;
	reader->root = node; 
	reader->editQueue = g_queue_new(); 
	reader->loginState = READER_STATE_NONE; 
	return reader;
}

void google_source_reader_free(readerPtr reader) 
{
	g_queue_free(reader->editQueue) ;
	g_free(reader);
}

/* subscription list merging functions */

static void
google_source_check_for_removal (nodePtr node, gpointer user_data)
{
	gchar		*expr = NULL;

	if ( g_str_equal(node->subscription->source, GOOGLE_SOURCE_BROADCAST_FRIENDS_URI) ) 
		return ; 
	if (IS_FEED (node)) {
		expr = g_strdup_printf ("/object/list[@name='subscriptions']/object/string[@name='id'][. = 'feed/%s']", node->subscription->source);
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
	nodePtr root = google_source_get_root_from_node(node);
	google_source_edit_mark_read((readerPtr)root->data, 
				     item->sourceId, 
				     node->subscription->source,
				     newStatus);
}

 
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
	subscription_update_favicon (node->subscription);
}

static void
google_source_merge_feed (xmlNodePtr match, gpointer user_data)
{
	readerPtr	reader = (readerPtr)user_data;
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
		subscription_update_favicon (node->subscription);
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
 * actual updating. You *MUST* follow this call up with a call to subscription_update to 
 * complete the call. If from a prepare_update_request, then use the following hack:
 * retrieve the subscription's source and set the request's source to that.
 */
void
google_source_login (subscriptionPtr subscription, guint32 flags) 
{ 
	readerPtr reader = (readerPtr) subscription->node->data;
	gchar *source;
	g_assert(reader);
	
	/* We are not logged in yet, we need to perform a login first and retrigger the update later... */
	
	if (reader->loginState != READER_STATE_NONE) {
		debug1(DEBUG_UPDATE, "Logging in while login state is %d\n", 
			     reader->loginState);
	}

	source = g_strdup_printf ("https://www.google.com/accounts/ClientLogin?service=reader&Email=%s&Passwd=%s&source=liferea&continue=http://www.google.com",
	                     	  subscription->updateOptions->username,
	                          subscription->updateOptions->password);

	reader->loginState = READER_STATE_IN_PROGRESS ;
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
	gchar		*tmp = NULL;
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
	
	g_assert(reader);
	if (reader->loginState == READER_STATE_NONE) {
		debug0(DEBUG_UPDATE, "GoogleSource: login");
		google_source_login(subscription, 0) ;

		reader->loginState = READER_STATE_IN_PROGRESS ;
		/* The subscription is updated, but the request has not yet been updated */
		update_request_set_source(request, subscription->source); 
		return TRUE;
	}
	debug1 (DEBUG_UPDATE, "updating Google Reader subscription (node id %s)", subscription->node->id);
	
	update_request_set_source(request, "http://www.google.com/reader/api/0/subscription/list");
	
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
	readerPtr reader = (readerPtr) google_source_get_root_from_node(subscription->node)->data ;
	xmlNodePtr xml;
	nodePtr node = subscription->node;

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
		/* this is extremely inefficient, multiple times loading */
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item && item->sourceId) {
			if (g_str_equal (item->sourceId, id) && item->readStatus != read && !google_source_edit_is_in_queue(reader, id)) {
				
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
google_source_xml_unlink_node(xmlNodePtr node, gpointer data) 
{
	xmlUnlinkNode(node) ;
	xmlFreeNode(node) ;
}

/**
 * This is identical to xpath_foreach_match, except that it takes the context
 * as parameter.
 */
static void
google_source_xpath_foreach_match(gchar* expr, xmlXPathContextPtr xpathCtxt, xpathMatchFunc func, gpointer user_data) 
{
	xmlXPathObjectPtr xpathObj = NULL;
	xpathObj = xmlXPathEval ((xmlChar*)expr, xpathCtxt);
	
	if (xpathObj && xpathObj->nodesetval && xpathObj->nodesetval->nodeMax) {
		int	i;
		for (i = 0; i < xpathObj->nodesetval->nodeNr; i++)
			(*func) (xpathObj->nodesetval->nodeTab[i], user_data);
	}
	
	if (xpathObj)
		xmlXPathFreeObject (xpathObj);
}
static void
google_feed_subscription_process_update_result (subscriptionPtr subscription,
                                                const struct updateResult* const result, 
                                                updateFlags flags)
{

	if ( result->data ) { 
		updateResultPtr resultCopy ;
		
		resultCopy = update_result_new() ;
		resultCopy->source = g_strdup(result->source); 
		resultCopy->returncode = result->returncode;
		resultCopy->httpstatus = result->httpstatus;
		resultCopy->contentType = g_strdup(result->contentType);
		resultCopy->retriesCount = result->retriesCount ;
		resultCopy->updateState = update_state_copy(result->updateState);
		
		/* update the XML by removing 'read', 'reading-list' etc. as labels. */
		xmlDocPtr doc = xml_parse (result->data, result->size, FALSE, NULL);
		xmlXPathContextPtr xpathCtxt = xmlXPathNewContext (doc) ;
		xmlXPathRegisterNs(xpathCtxt, "atom", "http://www.w3.org/2005/Atom");
		google_source_xpath_foreach_match("/atom:feed/atom:entry/atom:category[@scheme='http://www.google.com/reader/']", xpathCtxt, google_source_xml_unlink_node, NULL);
		xmlXPathFreeContext(xpathCtxt);
		
		/* good now we have removed the read and unread labels. */
		
		xmlChar    *newXml; 
		int        newXmlSize ;
		
		xmlDocDumpMemory(doc, &newXml, &newXmlSize) ;
		
		resultCopy->data = g_strndup((gchar*)newXml, newXmlSize);
		resultCopy->size = newXmlSize ;
		
		xmlFree(newXml) ;
		xmlFreeDoc(doc) ;
		
		feed_get_subscription_type ()->process_update_result (subscription, resultCopy, flags);
		update_result_free(resultCopy);
	} else { 
		feed_get_subscription_type()->process_update_result(subscription, result, flags);
		return ; 
	}

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
}

static gboolean
google_feed_subscription_prepare_update_request (subscriptionPtr subscription, 
                                                 struct updateRequest *request)
{
	debug0 (DEBUG_UPDATE, "preparing google reader feed subscription for update\n");
	readerPtr reader = (readerPtr) google_source_get_root_from_node (subscription->node)->data; 
	
	g_assert(reader); 
	if (reader->loginState == READER_STATE_NONE) { 
		subscription_update(google_source_get_root_from_node (subscription->node)->subscription, 0) ;
		return FALSE;
	}
	debug0 (DEBUG_UPDATE, "Setting cookies for a Google Reader subscription");

	if ( !g_str_equal(request->source, GOOGLE_SOURCE_BROADCAST_FRIENDS_URI) ) { 
		gchar* source_escaped = g_uri_escape_string(request->source,
							    NULL, TRUE);
		gchar* newUrl = g_strdup_printf("http://www.google.com/reader/atom/feed/%s", source_escaped) ;
		update_request_set_source(request, newUrl) ;
		g_free (newUrl);
		g_free(source_escaped);
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
	node->data = google_source_reader_new(node);
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
	if (!node->data) node->data = google_source_reader_new (node) ;

	for(iter = node->children; iter; iter = g_slist_next(iter) )
		((nodePtr) iter->data)->subscription->type = &googleReaderFeedSubscriptionType; 
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


nodePtr
google_source_add_subscription(nodePtr node, nodePtr parent, subscriptionPtr subscription) 
{ 
	debug_enter("google_source_add_subscription") ;
	nodePtr child = node_new ();

	debug0(DEBUG_UPDATE, "GoogleSource: Adding a new subscription"); 
	node_set_type (child, feed_get_node_type ());
	node_set_data (child, feed_new ());

	node_set_subscription(child, subscription) ;
	child->subscription->type = &googleReaderFeedSubscriptionType;
	
	node_set_title(child, _("New Subscription")) ;

	google_source_edit_add_subscription(google_source_get_root_from_node(node)->data, subscription->source);
	
	debug_exit("google_source_add_subscription") ;
	return child ; 
}

void
google_source_remove_node(nodePtr node, nodePtr child) 
{ 
	debug_enter("google_source_remove_node");
	
	debug_exit("google_source_remove_node");
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
	google_source_edit_export(reader);
	google_source_reader_free(reader);
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
	google_source_free,
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

