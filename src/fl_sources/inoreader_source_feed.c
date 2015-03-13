/**
 * @file inoreader_source_feed.c  InoReader feed subscription routines
 * 
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2014 Lars Windolf <lars.windolf@gmx.de>
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
#include <string.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "common.h"
#include "debug.h"
#include "xml.h"

#include "feedlist.h"
#include "google_reader_api_edit.h"
#include "inoreader_source.h"
#include "subscription.h"
#include "node.h"
#include "metadata.h"
#include "db.h"
#include "item_state.h"

/**
 * This is identical to xpath_foreach_match, except that it takes the context
 * as parameter.
 */
static void
inoreader_source_xpath_foreach_match (const gchar* expr, xmlXPathContextPtr xpathCtxt, xpathMatchFunc func, gpointer user_data) 
{
	xmlXPathObjectPtr xpathObj = NULL;
	xpathObj = xmlXPathEval ((xmlChar*)expr, xpathCtxt);
	
	if (xpathObj && xpathObj->nodesetval && xpathObj->nodesetval->nodeMax) {
		int	i;
		for (i = 0; i < xpathObj->nodesetval->nodeNr; i++) {
			(*func) (xpathObj->nodesetval->nodeTab[i], user_data);
			xpathObj->nodesetval->nodeTab[i] = NULL ;
		}
	}
	
	if (xpathObj)
		xmlXPathFreeObject (xpathObj);
}

void
inoreader_source_migrate_node(nodePtr node) 
{
	/* scan the node for bad ID's, if so, brutally remove the node */
	itemSetPtr itemset = node_get_itemset (node);
	GList *iter = itemset->ids;
	for (; iter; iter = g_list_next (iter)) {
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item && item->sourceId) {
			if (!g_str_has_prefix(item->sourceId, "tag:google.com")) {
				debug1(DEBUG_UPDATE, "Item with sourceId [%s] will be deleted.", item->sourceId);
				db_item_remove(GPOINTER_TO_UINT(iter->data));
			} 
		}
		if (item) item_unload (item);
	}

	/* cleanup */
	itemset_free (itemset);
}

static void
inoreader_source_xml_unlink_node (xmlNodePtr node, gpointer data) 
{
	xmlUnlinkNode (node);
	xmlFreeNode (node);
}

static itemPtr
inoreader_source_load_item_from_sourceid (nodePtr node, gchar *sourceId, GHashTable *cache) 
{
	gpointer    ret = g_hash_table_lookup (cache, sourceId);
	itemSetPtr  itemset;
	int         num = g_hash_table_size (cache);
	GList       *iter; 
	itemPtr     item = NULL;

	if (ret) return item_load (GPOINTER_TO_UINT (ret));

	/* skip the top 'num' entries */
	itemset = node_get_itemset (node);
	iter = itemset->ids;
	while (num--) iter = g_list_next (iter);

	for (; iter; iter = g_list_next (iter)) {
		item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item) {
			if (item->sourceId) {
				/* save to cache */
				g_hash_table_insert (cache, g_strdup(item->sourceId), (gpointer) item->id);
				if (g_str_equal (item->sourceId, sourceId)) {
					itemset_free (itemset);
					return item;
				}
			}
			item_unload (item);
		}
	}

	g_warning ("Could not find item for %s!", sourceId);
	itemset_free (itemset);
	return NULL;
}

static void
inoreader_source_item_retrieve_status (const xmlNodePtr entry, subscriptionPtr subscription, GHashTable *cache)
{
	xmlNodePtr      xml;
	nodePtr         node = subscription->node;
	xmlChar         *id = NULL;
	gboolean        read = FALSE;
	gboolean        starred = FALSE;

	xml = entry->children;
	g_assert (xml);

	for (xml = entry->children; xml; xml = xml->next) {
		if (!id && g_str_equal (xml->name, "id"))
			id = xmlNodeGetContent (xml);

		if (g_str_equal (xml->name, "category")) {
			xmlChar* label = xmlGetProp (xml, "label");
			if (!label)
				continue;

			if (g_str_equal (label, "read"))
				read = TRUE;
			else if (g_str_equal(label, "starred")) 
				starred = TRUE;

			xmlFree (label);
		}
	}
	
	if (!id) {
		g_warning ("Fatal: could not extract item id from InoReader Atom feed!");
		return;
	}

	itemPtr item = inoreader_source_load_item_from_sourceid (node, id, cache);
	if (item && item->sourceId) {
		if (g_str_equal (item->sourceId, id) && !google_reader_api_edit_is_in_queue (node->source, id)) {
			
			if (item->readStatus != read)
				item_read_state_changed (item, read);
			if (item->flagStatus != starred) 
				item_flag_state_changed (item, starred);
		}
	}
	if (item) item_unload (item) ;
	xmlFree (id);
}

static void
inoreader_feed_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult* const result, updateFlags flags)
{
	
	debug_start_measurement (DEBUG_UPDATE);

	if (result->data) { 
		updateResultPtr resultCopy;

		/* FIXME: The following is a very dirty hack to edit the feed's
		   XML before processing it */
		resultCopy = update_result_new () ;
		resultCopy->source = g_strdup (result->source); 
		resultCopy->httpstatus = result->httpstatus;
		resultCopy->contentType = g_strdup (result->contentType);
		g_free (resultCopy->updateState);
		resultCopy->updateState = update_state_copy (result->updateState);
		
		/* update the XML by removing 'read', 'reading-list' etc. as labels. */
		xmlDocPtr doc = xml_parse (result->data, result->size, NULL);
		xmlXPathContextPtr xpathCtxt = xmlXPathNewContext (doc) ;
		xmlXPathRegisterNs (xpathCtxt, "atom", "http://www.w3.org/2005/Atom");
		inoreader_source_xpath_foreach_match ("/atom:feed/atom:entry/atom:category[@scheme='http://www.inoreader.com/reader/']", xpathCtxt, inoreader_source_xml_unlink_node, NULL);
		xmlXPathFreeContext (xpathCtxt);
		
		/* good now we have removed the read and unread labels. */
		
		xmlChar    *newXml; 
		int        newXmlSize ;
		
		xmlDocDumpMemory (doc, &newXml, &newXmlSize);
		
		resultCopy->data = g_strndup ((gchar*) newXml, newXmlSize);
		resultCopy->size = newXmlSize;
		
		xmlFree (newXml);
		xmlFreeDoc (doc);
		
		feed_get_subscription_type ()->process_update_result (subscription, resultCopy, flags);
		update_result_free (resultCopy);
	} else { 
		feed_get_subscription_type ()->process_update_result (subscription, result, flags);
		return ; 
	}

	xmlDocPtr doc = xml_parse (result->data, result->size, NULL);
	if (doc) {		
		xmlNodePtr root = xmlDocGetRootElement (doc);
		xmlNodePtr entry = root->children ; 
		GHashTable *cache = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);

		while (entry) { 
			if (!g_str_equal (entry->name, "entry")) {
				entry = entry->next;
				continue; /* not an entry */
			}
			
			inoreader_source_item_retrieve_status (entry, subscription, cache);
			entry = entry->next;
		}
		
		g_hash_table_unref (cache);
		xmlFreeDoc (doc);
	} else { 
		debug0 (DEBUG_UPDATE, "google_feed_subscription_process_update_result(): Couldn't parse XML!");
		g_warning ("google_feed_subscription_process_update_result(): Couldn't parse XML!");
	}
	
	debug_end_measurement (DEBUG_UPDATE, "time taken to update statuses");
}

static gboolean
inoreader_feed_subscription_prepare_update_request (subscriptionPtr subscription, 
                                                 struct updateRequest *request)
{
	debug0 (DEBUG_UPDATE, "preparing InoReader feed subscription for update\n");
	nodePtr node = subscription->node; 
	
	if (node->source->loginState == NODE_SOURCE_STATE_NONE) { 
		subscription_update (node_source_root_from_node (node)->subscription, 0) ;
		return FALSE;
	}
	debug0 (DEBUG_UPDATE, "Setting cookies for a InoReader subscription");

	gchar* source_escaped = g_uri_escape_string(request->source, NULL, TRUE);
	gchar* newUrl = g_strdup_printf ("http://www.inoreader.com/reader/atom/feed/%s", source_escaped);
	update_request_set_source (request, newUrl);
	g_free (newUrl);
	g_free (source_escaped);

	update_request_set_auth_value (request, node->source->authToken);
	return TRUE;
}

struct subscriptionType inoreaderSourceFeedSubscriptionType = {
	inoreader_feed_subscription_prepare_update_request,
	inoreader_feed_subscription_process_update_result
};

