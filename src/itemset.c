/**
 * @file itemset.c support for different item list implementations
 * 
 * Copyright (C) 2005-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <string.h>
#include "db.h"
#include "debug.h"
#include "conf.h"
#include "common.h"
#include "feed.h"
#include "itemlist.h"
#include "itemset.h"
#include "node.h"
#include "render.h"
#include "support.h"
#include "vfolder.h"

xmlDocPtr itemset_to_xml(nodePtr node) {
	xmlDocPtr 	doc;
	xmlNodePtr 	itemSetNode;
	
	doc = xmlNewDoc("1.0");
	itemSetNode = xmlNewDocNode(doc, NULL, "itemset", NULL);
	
	xmlDocSetRootElement(doc, itemSetNode);
	
	xmlNewTextChild(itemSetNode, NULL, "favicon", node_get_favicon_file(node));
	xmlNewTextChild(itemSetNode, NULL, "title", node_get_title(node));

	if(NODE_TYPE_FEED == node->type) {
	       xmlNewTextChild(itemSetNode, NULL, "source", feed_get_source(node->data));
	       xmlNewTextChild(itemSetNode, NULL, "link", feed_get_html_url(node->data));
	}

	return doc;
}

void itemset_prepend_item(itemSetPtr itemSet, itemPtr item) {

	item->itemSet = itemSet;
	itemSet->items = g_list_prepend(itemSet->items, item);
}

void itemset_append_item(itemSetPtr itemSet, itemPtr item) {

	item->itemSet = itemSet;
	itemSet->items = g_list_append(itemSet->items, item);
}

void itemset_remove_item(itemSetPtr itemSet, itemPtr item) {

	if(!g_list_find(itemSet->items, item)) {
		g_warning("itemset_remove_item(): item (%s) to be removed not found...", item->title);
		return;
	}

	/* remove item from itemset */
	itemSet->items = g_list_remove(itemSet->items, item);
	
	if(item->newStatus)
		node_update_new_count(itemSet->node, -1);
	if(item->popupStatus)
		itemSet->node->popupCount--;

	/* perform itemset type specific removal actions */
	switch(itemSet->type) {
		case ITEMSET_TYPE_FEED:
		case ITEMSET_TYPE_FOLDER:
			/* remove vfolder copies */
			vfolder_remove_item(item);

			itemSet->node->needsCacheSave = TRUE;
			break;
		case ITEMSET_TYPE_VFOLDER:
			/* No propagation */
			break;
		default:
			g_error("itemset_remove_item(): unexpected item set type: %d\n", itemSet->type);
			break;
	}
}

// FIXME: still needed?
void itemset_remove_all_items(itemSetPtr itemSet) {

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

	g_assert(newFlagStatus != item->flagStatus);

	item->flagStatus = newFlagStatus;
	db_update_item(item);

	vfolder_check_item(item);	/* check if now a search folder rule matches */
}

void itemset_set_item_read_status(itemSetPtr itemSet, itemPtr item, gboolean newReadStatus) {

	g_assert(newReadStatus != item->readStatus);

	item->readStatus = newReadStatus;
	db_update_item(item);

	vfolder_check_item(item);	/* check if now a search folder rule matches */
}

void itemset_set_item_update_status(itemSetPtr itemSet, itemPtr item, gboolean newUpdateStatus) {

	g_assert(newUpdateStatus != item->updateStatus);

	item->updateStatus = newUpdateStatus;
	db_update_item(item);

	vfolder_check_item(item);	/* check if now a search folder rule matches */
}

void itemset_set_item_new_status(itemSetPtr itemSet, itemPtr item, gboolean newStatus) {

	g_assert(newStatus != item->newStatus);

	item->newStatus = newStatus;
	db_update_item(item);

	/* Note: new count updates must be done through the node
	   interface to allow global feed list new counter */
	node_update_new_count(itemSet->node, newStatus?-1:1);
	
	/* New status is never propagated to search folders... */
}

void itemset_set_item_popup_status(itemSetPtr itemSet, itemPtr item, gboolean newPopupStatus) {

	g_assert(newPopupStatus != item->popupStatus);

	item->popupStatus = newPopupStatus;

	/* Currently no node popup counter needed, therefore
	   no propagation to nodes... */
	
	/* Popup status is never propagated to vfolders... */
}

void itemset_free(itemSetPtr itemSet) {

	g_warning("FIXME: itemset_free()");
}
