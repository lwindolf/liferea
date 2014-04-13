/**
 * @file inoreader_source_feed_list.c  Inoreader handling routines.
 * 
 * Copyright (C) 2013-2014  Lars Windolf <lars.lindner@gmail.com>
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


#include "inoreader_source_feed_list.h"

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
#include "xml.h" // FIXME

#include "fl_sources/opml_source.h"
#include "fl_sources/inoreader_source.h"
#include "fl_sources/inoreader_source_edit.h"

/**
 * Find a node by the source id.
 */
nodePtr
inoreader_source_opml_get_node_by_source (InoreaderSourcePtr gsource, const gchar *source) 
{
	return inoreader_source_opml_get_subnode_by_node (gsource->root, source);
}

/**
 * Recursively find a node by the source id.
 */
nodePtr
inoreader_source_opml_get_subnode_by_node (nodePtr node, const gchar *source) 
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
			subsubnode = inoreader_source_opml_get_subnode_by_node(subnode, source);
			if (subnode != NULL)
				return subsubnode;
		}
	}
	return NULL;
}

/* subscription list merging functions */

static void
inoreader_source_merge_feed (InoreaderSourcePtr source, const gchar *url, const gchar *title, const gchar *id)
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
	node->subscription->type = &inoreaderSourceFeedSubscriptionType;

	/* Save Inoreader feed id which we need to fetch items... */
	node->subscription->metadata = metadata_list_append (node->subscription->metadata, "inoreader-feed-id", id);
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


/* OPML subscription type implementation */

static void
google_subscription_opml_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	InoreaderSourcePtr	source = (InoreaderSourcePtr) subscription->node->data;

	subscription->updateJob = NULL;
	
	// FIXME: the following code is very similar to ttrss!
	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), "subscriptions"));
			GList		*iter, *elements;
			GSList		*siter;
	
			/* We expect something like this:

			   [{"id":"feed\/http:\/\/rss.slashdot.org\/Slashdot\/slashdot",
                             "title":"Slashdot",
                             "categories":[],
                             "firstitemmsec":"1368112925514",
                             "htmlUrl":"null"},
                           ... 

			   Note that the data doesn't contain an URL. 
			   We recover it from the id field.
			*/
			elements = iter = json_array_get_elements (array);
			/* Add all new nodes we find */
			while (iter) {
				JsonNode *node = (JsonNode *)iter->data;
				
				/* ignore everything without a feed url */
				if (json_get_string (node, "id")) {
					inoreader_source_merge_feed (source, 
					                          json_get_string (node, "id") + 5,	// FIXME: Unescape string!
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
					// FIXME: Compare with unescaped string
					if (g_str_equal (node->subscription->source, json_get_string (json_node, "id") + 5)) {
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
			g_warning ("Invalid JSON returned on Inoreader feed list request! >>>%s<<<", result->data);
		}

		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "inoreader_subscription_cb(): ERROR: failed to get subscription list!");
	}

	if (!(flags & INOREADER_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));
}

/** functions for an efficient updating mechanism */

static void
inoreader_source_opml_quick_update_helper (xmlNodePtr match, gpointer userdata) 
{
	InoreaderSourcePtr gsource = (InoreaderSourcePtr) userdata;
	xmlNodePtr      xmlNode;
	xmlChar         *id, *newestItemTimestamp;
	nodePtr         node = NULL; 
	const gchar     *oldNewestItemTimestamp;

	xmlNode = xpath_find (match, "./string[@name='id']");
	id = xmlNodeGetContent (xmlNode); 

	if (g_str_has_prefix (id, "feed/"))
		node = inoreader_source_opml_get_node_by_source (gsource, id + strlen ("feed/"));
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
		debug3(DEBUG_UPDATE, "InoreaderSource: auto-updating %s "
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
inoreader_source_opml_quick_update_cb (const struct updateResult* const result, gpointer userdata, updateFlags flags) 
{
	InoreaderSourcePtr gsource = (InoreaderSourcePtr) userdata;
	xmlDocPtr       doc;

	if (!result->data) { 
		/* what do I do? */
		debug0 (DEBUG_UPDATE, "InoreaderSource: Unable to get unread counts, this update is aborted.");
		return;
	}
	doc = xml_parse (result->data, result->size, NULL);
	if (!doc) {
		debug0 (DEBUG_UPDATE, "InoreaderSource: The XML failed to parse, maybe the session has expired. (FIXME)");
		return;
	}

	xpath_foreach_match (xmlDocGetRootElement (doc),
			    "/object/list[@name='unreadcounts']/object", 
			    inoreader_source_opml_quick_update_helper, gsource);
	
	xmlFreeDoc (doc);
}

gboolean
inoreader_source_opml_quick_update(InoreaderSourcePtr gsource) 
{
	updateRequestPtr request = update_request_new ();
	request->updateState = update_state_copy (gsource->root->subscription->updateState);
	request->options = update_options_copy (gsource->root->subscription->updateOptions);
	update_request_set_source (request, INOREADER_UNREAD_COUNTS_URL);
	update_request_set_auth_value(request, gsource->authHeaderValue);

	update_execute_request (gsource, request, inoreader_source_opml_quick_update_cb,
				gsource, 0);

	return TRUE;
}


static void
inoreader_source_opml_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	google_subscription_opml_cb (subscription, result, flags);
}

static gboolean
inoreader_source_opml_subscription_prepare_update_request (subscriptionPtr subscription, struct updateRequest *request)
{
	InoreaderSourcePtr	gsource = (InoreaderSourcePtr)subscription->node->data;
	
	g_assert(gsource);
	if (gsource->loginState == INOREADER_SOURCE_STATE_NONE) {
		debug0(DEBUG_UPDATE, "InoreaderSource: login");
		inoreader_source_login ((InoreaderSourcePtr) subscription->node->data, 0) ;
		return FALSE;
	}
	debug1 (DEBUG_UPDATE, "updating Inoreader subscription (node id %s)", subscription->node->id);
	
	update_request_set_source (request, INOREADER_SUBSCRIPTION_LIST_URL);
	
	update_request_set_auth_value (request, gsource->authHeaderValue);
	
	return TRUE;
}

/* OPML subscription type definition */

struct subscriptionType inoreaderSourceOpmlSubscriptionType = {
	inoreader_source_opml_subscription_prepare_update_request,
	inoreader_source_opml_subscription_process_update_result
};
