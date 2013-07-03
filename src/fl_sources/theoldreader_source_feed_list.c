/**
 * @file theoldreader_source_feed_list.c  TheOldReader feed list handling
 * 
 * Copyright (C) 2013  Lars Windolf <lars.lindner@gmail.com>
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


#include "theoldreader_source_feed_list.h"

#include <glib.h>
#include <libxml/xpath.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "folder.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "xml.h"

#include "fl_sources/opml_source.h"
#include "fl_sources/theoldreader_source.h"
#include "fl_sources/theoldreader_source_edit.h"

/**
 * Find a node by the source id.
 */
nodePtr
theoldreader_source_opml_get_node_by_source (TheOldReaderSourcePtr gsource, const gchar *source) 
{
	return theoldreader_source_opml_get_subnode_by_node (gsource->root, source);
}

/**
 * Recursively find a node by the source id.
 */
nodePtr
theoldreader_source_opml_get_subnode_by_node (nodePtr node, const gchar *source) 
{
	nodePtr subnode;
	nodePtr subsubnode;
	GSList  *iter = node->children;
	for (; iter; iter = g_slist_next (iter)) {
		subnode = (nodePtr)iter->data;
		if (subnode->subscription
		    && g_str_equal (subnode->subscription->source, source))
			return subnode;
		else if (subnode->type->capabilities
			 & NODE_CAPABILITY_SUBFOLDERS) {
			subsubnode = theoldreader_source_opml_get_subnode_by_node(subnode, source);
			if (subnode != NULL)
				return subsubnode;
		}
	}
	return NULL;
}

/* subscription list merging functions */

static void
theoldreader_source_check_for_removal (nodePtr node, gpointer user_data)
{
	gchar	*expr = NULL;

	if (IS_FEED (node)) {
		expr = g_strdup_printf ("/object/list[@name='subscriptions']/object/string[@name='id'][. = 'feed/%s']", node->subscription->source);
	} else if (IS_FOLDER (node)) {
		node_foreach_child_data (node, theoldreader_source_check_for_removal, user_data);
		expr = g_strdup_printf ("/object/list[@name='subscriptions']/object/list[@name='categories']/object[string='%s']", node->title);
	} else {
		g_warning ("google_opml_source_check_for_removal(): This should never happen...");
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

/* 
 * Find a node by the name under root or create it.
 */
static nodePtr
theoldreader_source_find_or_create_folder (const gchar *name, nodePtr root)
{
	nodePtr		folder = NULL;
	GSList		*iter_parent;

	/* find a node by the name under root */
	iter_parent = root->children;
	while (iter_parent) {
		if (g_str_equal (name, node_get_title (iter_parent->data))) {
			folder = (nodePtr)iter_parent->data;
			break;
		}
		iter_parent = g_slist_next (iter_parent);
	}
	
	/* if not found, create new folder */
	if (!folder) {
		folder = node_new (folder_get_node_type ());
		node_set_title (folder, name);
		node_set_parent (folder, root, -1);
		feedlist_node_imported (folder);
		subscription_update (folder->subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
	}
	
	return folder;
}

/* 
 * Check if folder of a node changed in TheOldReader and move
 * node to the folder with the same name.
 */
static void
theoldreader_source_update_folder (xmlNodePtr match, TheOldReaderSourcePtr gsource, nodePtr node)
{
	xmlNodePtr	xml;
	xmlChar		*label;
	const gchar	*ptitle;
	nodePtr		parent;
	
	/* check if label of a feed changed */ 
	parent = node->parent;
	ptitle = node_get_title (parent);
	xml = xpath_find (match, "./list[@name='categories']/object/string[@name='label']");
	if (xml) {
		label = xmlNodeListGetString (xml->doc,	xml->xmlChildrenNode, 1);
		if (parent == gsource->root || ! g_str_equal (label, ptitle)) {
			debug2 (DEBUG_UPDATE, "GSource feed label changed for %s to '%s'", node->id, label);
			parent = theoldreader_source_find_or_create_folder ((gchar*)label, gsource->root);
			node_reparent (node, parent);
		}
		xmlFree (label);
	} else {
		/* if feed has no label and parent is not gsource root, reparent to gsource root */
		if (parent != gsource->root)
			node_reparent (node, gsource->root);
	}
}

static void
theoldreader_source_merge_feed (xmlNodePtr match, gpointer user_data)
{
	TheOldReaderSourcePtr	gsource = (TheOldReaderSourcePtr)user_data;
	nodePtr		node, parent = NULL, subnode = NULL;
	GSList		*iter, *iter_sub;
	xmlNodePtr	xml;
	xmlChar		*title = NULL, *id = NULL, *label = NULL;
	gchar		*url = NULL;

	xml = xpath_find (match, "./string[@name='title']");
	if (xml)
		title = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
		
	xml = xpath_find (match, "./string[@name='id']");
	if (xml) {
		id = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
		url = g_strdup (id + strlen ("feed/"));
	}

	/* Note: ids look like "feed/http://rss.slashdot.org" */
	if (id && title) {

		/* check if node to be merged already exists */
		iter = gsource->root->children;
		while (iter) {
			node = (nodePtr)iter->data;
			if (node->subscription != NULL
			    && g_str_equal (node->subscription->source, url)) {
				node->subscription->type = &theOldReaderSourceFeedSubscriptionType;
				theoldreader_source_update_folder (match, gsource, node);
				goto cleanup;
			} else if (node->type->capabilities
				 & NODE_CAPABILITY_SUBFOLDERS) {
				iter_sub = node->children;
				while (iter_sub) {
					subnode = (nodePtr)iter_sub->data;
					if (subnode->subscription != NULL
					    && g_str_equal (subnode->subscription->source, url)) {
						subnode->subscription->type = &theOldReaderSourceFeedSubscriptionType;
						theoldreader_source_update_folder (match, gsource, subnode);
						goto cleanup;
					}
					iter_sub = g_slist_next (iter_sub);
				}
			}
			iter = g_slist_next (iter);
		}

		/* if a new feed contains label, put its node under a folder with the same name */
		xml = xpath_find (match, "./list[@name='categories']/object/string[@name='label']");
		if (xml) {
			label = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
			parent = theoldreader_source_find_or_create_folder ((gchar*)label, gsource->root);
			xmlFree (label);
		} else {
			parent = gsource->root;
		}
		
		g_assert (NULL != parent);

		debug2 (DEBUG_UPDATE, "adding %s (%s)", title, url);
		node = node_new (feed_get_node_type ());
		node_set_title (node, title);
		node_set_data (node, feed_new ());
		
		node_set_subscription (node, subscription_new (url, NULL, NULL));
		node->subscription->type = &theOldReaderSourceFeedSubscriptionType;
		node_set_parent (node, parent, -1);
		feedlist_node_imported (node);
		
		/**
		 * @todo mark the ones as read immediately after this is done
		 * the feed as retrieved by this has the read and unread
		 * status inherently.
		 */
		subscription_update (node->subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
		subscription_update_favicon (node->subscription);
	} else {
		g_warning("Unable to parse subscription information from Google");
	}

cleanup:
	xmlFree (id);
	xmlFree (title);
	g_free (url) ;
}


/* JSON subscription list processing implementation */

static void
theoldreader_subscription_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	TheOldReaderSourcePtr	gsource = (TheOldReaderSourcePtr) subscription->node->data;
	
	if (result->data) {
		xmlDocPtr doc = xml_parse (result->data, result->size, NULL);
		if(doc) {		
			xmlNodePtr root = xmlDocGetRootElement (doc);
			
			/* Go through all existing nodes and remove those whose
			   URLs are not in new feed list. Also removes those URLs
			   from the list that have corresponding existing nodes. */
			node_foreach_child_data (subscription->node, theoldreader_source_check_for_removal, (gpointer)root);
			node_foreach_child (subscription->node, theoldreader_source_migrate_node);
						
			opml_source_export (subscription->node);	/* save new feed list tree to disk 
									   to ensure correct document in 
									   next step */

			xpath_foreach_match (root, "/object/list[@name='subscriptions']/object",
			                     theoldreader_source_merge_feed,
			                     (gpointer)gsource);

			opml_source_export (subscription->node);	/* save new feeds to feed list */
						   
			subscription->node->available = TRUE;
			xmlFreeDoc (doc);
		} else { 
			/** @todo The session seems to have expired */
			g_warning ("Unable to parse OPML list from google, the session might have expired.\n");
		}
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "google_subscription_opml_cb(): ERROR: failed to get subscription list!\n");
	}

	if (!(flags & THEOLDREADER_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));

}

/** functions for an efficient updating mechanism */

static void
theoldreader_source_opml_quick_update_helper (xmlNodePtr match, gpointer userdata) 
{
	TheOldReaderSourcePtr gsource = (TheOldReaderSourcePtr) userdata;
	xmlNodePtr      xmlNode;
	xmlChar         *id, *newestItemTimestamp;
	nodePtr         node = NULL; 
	const gchar     *oldNewestItemTimestamp;

	xmlNode = xpath_find (match, "./string[@name='id']");
	id = xmlNodeGetContent (xmlNode); 

	if (g_str_has_prefix (id, "feed/"))
		node = theoldreader_source_opml_get_node_by_source (gsource, id + strlen ("feed/"));
	else {
		xmlFree (id);
		return;
	}

	if (node == NULL) {
		xmlFree (id);
		return;
	}

	xmlNode = xpath_find (match, "./number[@name='newestItemTimestampUsec']");
	newestItemTimestamp = xmlNodeGetContent (xmlNode);

	oldNewestItemTimestamp = g_hash_table_lookup (gsource->lastTimestampMap, node->subscription->source);

	if (!oldNewestItemTimestamp ||
	    (newestItemTimestamp && 
	     !g_str_equal (newestItemTimestamp, oldNewestItemTimestamp))) { 
		debug3(DEBUG_UPDATE, "TheOldReaderSource: auto-updating %s "
		       "[oldtimestamp%s, timestamp %s]", 
		       id, oldNewestItemTimestamp, newestItemTimestamp);
		g_hash_table_insert (gsource->lastTimestampMap,
				    g_strdup (node->subscription->source), 
				    g_strdup (newestItemTimestamp));
				    
		subscription_update (node->subscription, 0);
	}

	xmlFree (newestItemTimestamp);
	xmlFree (id);
}

static void
theoldreader_source_opml_quick_update_cb (const struct updateResult* const result, gpointer userdata, updateFlags flags) 
{
	TheOldReaderSourcePtr gsource = (TheOldReaderSourcePtr) userdata;
	xmlDocPtr       doc;

	if (!result->data) { 
		/* what do I do? */
		debug0 (DEBUG_UPDATE, "TheOldReaderSource: Unable to get unread counts, this update is aborted.");
		return;
	}
	doc = xml_parse (result->data, result->size, NULL);
	if (!doc) {
		debug0 (DEBUG_UPDATE, "TheOldReaderSource: The XML failed to parse, maybe the session has expired. (FIXME)");
		return;
	}

	xpath_foreach_match (xmlDocGetRootElement (doc),
			    "/object/list[@name='unreadcounts']/object", 
			    theoldreader_source_opml_quick_update_helper, gsource);
	
	xmlFreeDoc (doc);
}

gboolean
theoldreader_source_opml_quick_update(TheOldReaderSourcePtr gsource) 
{
	updateRequestPtr request = update_request_new ();
	request->updateState = update_state_copy (gsource->root->subscription->updateState);
	request->options = update_options_copy (gsource->root->subscription->updateOptions);
	update_request_set_source (request, THEOLDREADER_READER_UNREAD_COUNTS_URL);
	update_request_set_auth_value(request, gsource->authHeaderValue);

	update_execute_request (gsource, request, theoldreader_source_opml_quick_update_cb,
				gsource, 0);

	return TRUE;
}


static void
theoldreader_source_opml_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	theoldreader_subscription_cb (subscription, result, flags);
}

static gboolean
theoldreader_source_opml_subscription_prepare_update_request (subscriptionPtr subscription, struct updateRequest *request)
{
	TheOldReaderSourcePtr	gsource = (TheOldReaderSourcePtr)subscription->node->data;
	
	g_assert(gsource);
	if (gsource->loginState == THEOLDREADER_SOURCE_STATE_NONE) {
		debug0(DEBUG_UPDATE, "TheOldReaderSource: login");
		theoldreader_source_login ((TheOldReaderSourcePtr) subscription->node->data, 0) ;
		return FALSE;
	}
	debug1 (DEBUG_UPDATE, "updating TheOldReader subscription (node id %s)", subscription->node->id);
	
	update_request_set_source (request, THEOLDREADER_READER_SUBSCRIPTION_LIST_URL);
	
	update_request_set_auth_value (request, gsource->authHeaderValue);
	
	return TRUE;
}

/* OPML subscription type definition */

struct subscriptionType theOldReaderSourceOpmlSubscriptionType = {
	theoldreader_source_opml_subscription_prepare_update_request,
	theoldreader_source_opml_subscription_process_update_result
};
