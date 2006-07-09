/**
 * @file itemset.c support for different item list implementations
 * 
 * Copyright (C) 2005-2006 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2005-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "itemset.h"
#include <string.h>
#include "conf.h"
#include "common.h"
#include "debug.h"
#include "feed.h"
#include "node.h"
#include "render.h"
#include "support.h"
#include "vfolder.h"

const gchar * itemset_get_base_url(itemSetPtr itemSet) {
	const gchar 	*baseUrl = NULL;

	switch(itemSet->type) {
		case ITEMSET_TYPE_FEED:
			baseUrl = feed_get_html_url((feedPtr)itemSet->node->data);
			break;
		case ITEMSET_TYPE_FOLDER:
			break;
		case ITEMSET_TYPE_VFOLDER:
			break;
	}

	/* prevent feed scraping commands to end up as base URI */
	if(!((baseUrl != NULL) &&
	     (baseUrl[0] != '|') &&
	     (strstr(baseUrl, "://") != NULL)))
	   	baseUrl = NULL;

	return baseUrl;
}

static xmlDocPtr itemset_to_xml(itemSetPtr itemSet) {
	xmlDocPtr 	doc;
	xmlNodePtr 	itemSetNode;
	
	doc = xmlNewDoc("1.0");
	itemSetNode = xmlNewDocNode(doc, NULL, "itemset", NULL);
	
	xmlDocSetRootElement(doc, itemSetNode);

	xmlNewTextChild(itemSetNode, NULL, "favicon", node_get_favicon_file(itemSet->node));
	xmlNewTextChild(itemSetNode, NULL, "title", node_get_title(itemSet->node));

	if(ITEMSET_TYPE_FEED == itemSet->type)
	       xmlNewTextChild(itemSetNode, NULL, "source", feed_get_source(itemSet->node->data));
			
	return doc;
}

gchar * itemset_render(itemSetPtr itemSet) {
	gchar		**params = NULL, *output = NULL;
	gboolean	summaryMode = FALSE;
	gboolean	loadReadItems;
	GList		*iter;
	xmlDocPtr	doc;

	debug_enter("itemset_render");
	
	/* determine wether we want to filter read items */
	switch(itemSet->type) {
		case ITEMSET_TYPE_FOLDER:
			loadReadItems = !getBooleanConfValue(FOLDER_DISPLAY_HIDE_READ);
			break;
		default:
			loadReadItems = TRUE;
			break;
	}
	
	/* for future: here we could apply an item list filter... */
	
	if(ITEMSET_TYPE_FOLDER != itemSet->type) {
		guint missingContent = 0;	
		
		/* Output optimization for feeds without item content. This
		   is not done for folders, because we only support all items
		   in summary mode or all in detailed mode. With folder item 
		   sets displaying everything in summary because of only a
		   single feed without item descriptions would make no sense. */
		   
		iter = itemSet->items;
		while(iter) {
			gchar *desc = ((itemPtr)iter->data)->description;
			if(!desc || (0 == strlen(desc)))
				missingContent++;
				
			if(summaryMode = (missingContent > 3))
				break;
				
			iter = g_list_next(iter);
		}		
	}
	
	/* do the XML serialization */
	doc = itemset_to_xml(itemSet);
	
	iter = itemSet->items;
	while(iter) {
		item_to_xml((itemPtr)iter->data, xmlDocGetRootElement(doc), TRUE);
		iter = g_list_next(iter);
	}
		
	/* and finally the XSLT rendering transformation */
	params = render_add_parameter(params, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");
	params = render_add_parameter(params, "baseUrl='%s'", itemset_get_base_url(itemSet));
	output = render_xml(doc, summaryMode?"summary":"itemset", params);
	g_strfreev(params);
	xmlFree(doc);
	
	debug_exit("itemset_render");

	return output;
}

itemPtr itemset_lookup_item(itemSetPtr itemSet, nodePtr node, gulong nr) {

	GList *iter = itemSet->items;
	while(iter) {
		itemPtr item = (itemPtr)(iter->data);
		if((item->nr == nr) && (item->itemSet->node == node))
			return item;
		iter = g_list_next(iter);
	}

	return NULL;
}

static void itemset_prepare_item(itemSetPtr itemSet, itemPtr item) {

	g_assert(item->itemSet != itemSet);
	item->itemSet = itemSet;

	/* when adding items after loading cache they have
	   no source node and nr, so we set it... */
	if(!item->sourceNode) {
		item->sourceNode = itemSet->node;
		item->nr = item->sourceNr;
	}

	/* to be sure we always assign a new unique item number... */
	item->nr = ++(item->itemSet->lastItemNr);
		
	/* sanity check, should never be triggered... */
	if(itemset_lookup_item(itemSet, item->sourceNode, item->nr)) {
		g_warning("fatal: non-unique item nr! (nr=%d, node=%p, %s)", item->nr, item->sourceNode, item->title); \
		item->nr = ++(itemSet->lastItemNr);
		item->sourceNode->needsCacheSave = TRUE;
	}
	
	// FIXME: really needed?
	if(item->sourceNode == itemSet->node)
		item->sourceNr = item->nr;
}
				

void itemset_prepend_item(itemSetPtr itemSet, itemPtr item) {

	itemset_prepare_item(itemSet, item);
	itemSet->items = g_list_prepend(itemSet->items, item);
}

void itemset_append_item(itemSetPtr itemSet, itemPtr item) {

	itemset_prepare_item(itemSet, item);
	itemSet->items = g_list_append(itemSet->items, item);
}

void itemset_remove_item(itemSetPtr itemSet, itemPtr item) {

	if(!g_list_find(itemSet->items, item)) {
		g_warning("itemset_remove_item(): item (%s) to be removed not found...", item->title);
		return;
	}

	/* remove item from itemset */
	itemSet->items = g_list_remove(itemSet->items, item);

	/* propagate item removal to itemset type specific implementation */
	switch(itemSet->type) {
		case ITEMSET_TYPE_FEED:
		case ITEMSET_TYPE_FOLDER:
			/* remove vfolder copies */
			vfolder_remove_item(item);

			itemSet->node->needsCacheSave = TRUE;
			
			/* is this really correct? e.g. if there is no 
			   unread/important vfolder? then the remove
			   above would do nothing and decrementing
			   the counters would be wrong, the same when
			   there are multiple vfolders catching an
			   unread item...  FIXME!!! (Lars) */
			feedlist_update_counters(item->readStatus?0:-1,	
						 item->newStatus?-1:0);
			break;
		case ITEMSET_TYPE_VFOLDER:
			/* No propagation */
			break;
		default:
			g_error("itemset_remove_item(): unexpected item set type: %d\n", itemSet->type);
			break;
	}
}

void itemset_remove_items(itemSetPtr itemSet) {

	/* hmmm... bad performance when removing a lot of items */
	GList *list = g_list_copy(itemSet->items);
	GList *iter = list;
	while(iter) {
		itemset_remove_item(itemSet, (itemPtr)iter->data);
		iter = g_list_next(iter);
	}
	g_list_free(list);

	itemSet->node->needsCacheSave = TRUE;
}

void itemset_set_item_flag(itemSetPtr itemSet, itemPtr item, gboolean newFlagStatus) {
	nodePtr	sourceNode;
	itemPtr	sourceItem;

	g_assert(newFlagStatus != item->flagStatus);

	item->flagStatus = newFlagStatus;

	/* if this item belongs to a vfolder update the source feed */
	if(ITEMSET_TYPE_VFOLDER == itemSet->type) {
		g_assert(item->sourceNode);
		/* propagate change to source feed, this indirectly updates us... */
		sourceNode = item->sourceNode;	/* keep feed pointer because ip might be free'd */
		node_load(sourceNode);
		if(sourceItem = itemset_lookup_item(sourceNode->itemSet, sourceNode, item->sourceNr))
			itemlist_set_flag(sourceItem, newFlagStatus);
		node_unload(sourceNode);
	} else {
		vfolder_update_item(item);	/* there might be vfolders using this item */
		vfolder_check_item(item);	/* and check if now a rule matches */
	}
}

void itemset_set_item_read_status(itemSetPtr itemSet, itemPtr item, gboolean newReadStatus) {
	nodePtr	sourceNode;
	itemPtr	sourceItem;

	g_assert(newReadStatus != item->readStatus);

	item->readStatus = newReadStatus;

	/* Note: unread count updates must be done through the node
	   interface to allow recursive node unread count updates */
	node_update_unread_count(itemSet->node, newReadStatus?-1:1);

	/* if this item belongs to a vfolder update the source feed */	
	if(ITEMSET_TYPE_VFOLDER == itemSet->type) {
		g_assert(item->sourceNode);
		/* propagate change to source feed, this indirectly updates us... */
		sourceNode = item->sourceNode;	/* keep feed pointer because item might be free'd */
		node_load(sourceNode);
		if(sourceItem = itemset_lookup_item(sourceNode->itemSet, sourceNode, item->sourceNr))
			itemlist_set_read_status(sourceItem, newReadStatus);
		node_unload(sourceNode);
	} else {		
		vfolder_update_item(item);	/* there might be vfolders using this item */
		vfolder_check_item(item);	/* and check if now a rule matches */
	}
}

void itemset_set_item_update_status(itemSetPtr itemSet, itemPtr item, gboolean newUpdateStatus) {
	nodePtr	sourceNode;
	itemPtr	sourceItem;

	g_assert(newUpdateStatus != item->updateStatus);

	item->updateStatus = newUpdateStatus;

	/* if this item belongs to a vfolder update the source feed */
	if(ITEMSET_TYPE_VFOLDER == itemSet->type) {	
		g_assert(item->sourceNode);
		/* propagate change to source feed, this indirectly updates us... */
		sourceNode = item->sourceNode;	/* keep feed pointer because ip might be free'd */
		node_load(sourceNode);
		if(sourceItem = itemset_lookup_item(sourceNode->itemSet, sourceNode, item->sourceNr))
			itemlist_set_update_status(sourceItem, newUpdateStatus);
		node_unload(sourceNode);
	} else {
		vfolder_update_item(item);	/* there might be vfolders using this item */
		vfolder_check_item(item);	/* and check if now a rule matches */
	}
}

void itemset_set_item_new_status(itemSetPtr itemSet, itemPtr item, gboolean newStatus) {

	g_assert(newStatus != item->newStatus);

	item->newStatus = newStatus;

	/* Note: new count updates must be done through the node
	   interface to allow global feed list new counter */
	node_update_new_count(itemSet->node, newStatus?-1:1);
	
	/* New status is never propagated to vfolders... */
}
