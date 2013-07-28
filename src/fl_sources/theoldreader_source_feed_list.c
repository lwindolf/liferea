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
#include <string.h>

#include "common.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "folder.h"
#include "json.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "xml.h"

#include "fl_sources/opml_source.h"
#include "fl_sources/theoldreader_source.h"
#include "fl_sources/theoldreader_source_edit.h"

static void
theoldreader_source_merge_feed (TheOldReaderSourcePtr source, const gchar *url, const gchar *title, const gchar *id)
{
	nodePtr	node;
	GSList	*iter;

	/* check if node to be merged already exists */
	iter = source->root->children;
	while (iter) {
		node = (nodePtr)iter->data;
		if (g_str_equal (node->subscription->source, url))
			return;
		iter = g_slist_next (iter);
	}
	
	debug2 (DEBUG_UPDATE, "adding %s (%s)", title, url);
	node = node_new (feed_get_node_type ());
	node_set_title (node, title);
	node_set_data (node, feed_new ());
		
	node_set_subscription (node, subscription_new (url, NULL, NULL));
	node->subscription->type = &theOldReaderSourceFeedSubscriptionType;
	
	/* Save TheOldReader feed id which we need to fetch items... */
	node->subscription->metadata = metadata_list_append (node->subscription->metadata, "theoldreader-feed-id", id);
	db_subscription_update (node->subscription);
	
	node_set_parent (node, source->root, -1);
	feedlist_node_imported (node);
		
	/**
	 * @todo mark the ones as read immediately after this is done
	 * the feed as retrieved by this has the read and unread
	 * status inherently.
	 */
	subscription_update (node->subscription, FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
	subscription_update_favicon (node->subscription);
}

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
		g_warning ("theoldreader_source_check_for_removal(): This should never happen...");
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

/* JSON subscription list processing implementation */

static void
theoldreader_subscription_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	TheOldReaderSourcePtr	source = (TheOldReaderSourcePtr) subscription->node->data;

	debug1 (DEBUG_UPDATE,"theoldreader_subscription_cb(): %s", result->data);
	
	// FIXME: the following code is very similar to ttrss!
	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), "subscriptions"));
			GList		*iter, *elements;
			GSList		*siter;
		
			/* We expect something like this:

			   [{"id":"feed/51d49b79d1716c7b18000025",
                             "title":"LZone",
                             "categories":[],
                             "sortid":"51d49b79d1716c7b18000025",
                             "firstitemmsec":"1371403150181",
                             "url":"http://lzone.de/rss.xml",
                             "htmlUrl":"http://lzone.de",
                             "iconUrl":"http://s.yeoldereader.com/system/uploads/feed/picture/5152/884a/4dce/57aa/7e00/icon_0a6a.ico"},
                           ... 
			*/
			elements = iter = json_array_get_elements (array);
			/* Add all new nodes we find */
			while (iter) {
				JsonNode *node = (JsonNode *)iter->data;
				
				/* ignore everything without a feed url */
				if (json_get_string (node, "url")) {
					theoldreader_source_merge_feed (source, 
					                                json_get_string (node, "url"),
					                                json_get_string (node, "title"),
					                                json_get_string (node, "id"));
				}
				iter = g_list_next (iter);
			}
			g_list_free (elements);

			/* Remove old nodes we cannot find anymore */
			siter = source->root->children;
			while (siter) {
				nodePtr node = (nodePtr)siter->data;
				gboolean found = FALSE;
				
				elements = iter = json_array_get_elements (array);
				while (iter) {
					JsonNode *json_node = (JsonNode *)iter->data;
					if (g_str_equal (node->subscription->source, json_get_string (json_node, "url"))) {
						debug1 (DEBUG_UPDATE, "node: %s", node->subscription->source);
						found = TRUE;
						break;
					}
					iter = g_list_next (iter);
				}
				g_list_free (elements);

				if (!found)			
					feedlist_node_removed (node);
				
				siter = g_slist_next (siter);
			}
			
			opml_source_export (subscription->node);	/* save new feeds to feed list */				   
			subscription->node->available = TRUE;			
			//return;
		} else {
			g_warning ("Invalid JSON returned on TheOldReader request! >>>%s<<<", result->data);
		}

		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "theoldreader_subscription_cb(): ERROR: failed to get subscription list!");
	}

	if (!(flags & THEOLDREADER_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));
}

/** functions for an efficient updating mechanism */
/*
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
theoldreader_source_opml_quick_update (TheOldReaderSourcePtr source) 
{
	updateRequestPtr request = update_request_new ();
	request->updateState = update_state_copy (source->root->subscription->updateState);
	request->options = update_options_copy (source->root->subscription->updateOptions);
	update_request_set_source (request, THEOLDREADER_READER_UNREAD_COUNTS_URL);
	update_request_set_auth_value (request, source->authHeaderValue);

	update_execute_request (source, request, theoldreader_source_opml_quick_update_cb,
				source, 0);

	return TRUE;
}
*/

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
