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

static guint itemset_get_max_item_count(itemSetPtr itemSet) {

	switch(itemSet->node->type) {
		case NODE_TYPE_FEED:
			return feed_get_max_item_count(itemSet->node);
			break;
		default:
			return G_MAXUINT;
	}
}

/**
 * Determine wether a given item is to be merged
 * into the itemset or if it was already added.
 */
static gboolean itemset_merge_check(itemSetPtr itemSet, itemPtr item) {

	switch(itemSet->node->type) {
		case NODE_TYPE_FEED:
			return feed_merge_check(itemSet, item);
			break;
		default:
			g_warning("node_merge_check(): If this happens something is wrong!");
			break;
	}
	
	return FALSE;
}

static void itemset_merge_item(itemSetPtr itemSet, itemPtr item) {

	debug2(DEBUG_UPDATE, "trying to merge \"%s\" to node \"%s\"", item_get_title(item), node_get_title(itemSet->node));

	/* step 1: merge into node type internal data structures */
	if(itemset_merge_check(itemSet, item)) {
		g_assert(itemSet->node);
		g_assert(!item->node);
		g_assert(!item->id);
		item->node = itemSet->node;
		
		/* step 1: add to itemset */
		itemset_prepend_item(itemSet, item);

		/* step 2: write to DB */
		db_item_update(item);
				
		debug3(DEBUG_UPDATE, "-> added \"%s\" (id=%d) to item set %p...", item_get_title(item), item->id, itemSet);
		
		/* step 3: check for matching vfolders */
		// FIXME
		
		/* step 4: duplicate detection, mark read if it is a duplicate */
		// FIXME: still needed?
		if(item->validGuid) {
			if(item_guid_list_get_duplicates_for_id(item)) {
				// FIXME do something better: item->readStatus = TRUE;
				debug2(DEBUG_UPDATE, "-> duplicate guid detected: %s -> %s\n", item->id, item->title);
			}
		}
	} else {
		debug2(DEBUG_UPDATE, "-> not adding \"%s\" to node \"%s\"...", item_get_title(item), node_get_title(itemSet->node));
		item_unload(item);
	}
}

void itemset_merge_items(itemSetPtr itemSet, GList *list) {
	GList	*iter;
	guint	max;
	
	if(debug_level & DEBUG_UPDATE) {
		debug3(DEBUG_UPDATE, "old item set %p of \"%s\" (%p):", itemSet, node_get_title(itemSet->node), itemSet->node);
		iter = itemSet->items;
		while(iter) {
			itemPtr item = (itemPtr)iter->data;
			debug2(DEBUG_UPDATE, " -> item #%d \"%s\"", item->id, item_get_title(item));
			iter = g_list_next(iter);
		}
	}
	
	/* Truncate the new itemset if it is longer than
	   the maximum cache size which could cause items
	   to be dropped and added again on subsequent 
	   merges with the same feed content */
	max = itemset_get_max_item_count(itemSet);
	if(g_list_length(list) > max) {
		debug3(DEBUG_UPDATE, "item list too long (%u, max=%u) when merging into \"%s\"!", g_list_length(list), max, node_get_title(itemSet->node));
		guint i = 0;
		GList *iter, *copy;
		iter = copy = g_list_copy(list);
		while(iter) {
			i++;
			if(i > max) {
				itemPtr item = (itemPtr)iter->data;
				debug2(DEBUG_UPDATE, "ignoring item nr %u (%s)...", i, item_get_title(item));
				item_unload(item);
				list = g_list_remove(list, item);
			}
			iter = g_list_next(iter);
		}
		g_list_free(copy);
	}	   

	/* Items are given in top to bottom display order. 
	   Adding them in this order would mean to reverse 
	   their order in the merged list, so merging needs
	   to be done bottom to top. */
	iter = g_list_last(list);
	while(iter) {
		itemset_merge_item(itemSet, ((itemPtr)iter->data));
		iter = g_list_previous(iter);
	}
	g_list_free(list);
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
			// FIXME

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

void itemset_set_item_flag(itemSetPtr itemSet, itemPtr item, gboolean newFlagStatus) {

	g_assert(newFlagStatus != item->flagStatus);

	item->flagStatus = newFlagStatus;
	db_item_update(item);

	// FIXME: vfolder update?
}

void itemset_set_item_read_status(itemSetPtr itemSet, itemPtr item, gboolean newReadStatus) {

	g_assert(newReadStatus != item->readStatus);

	item->readStatus = newReadStatus;
	db_item_update(item);

	// FIXME: vfolder update?
}

void itemset_set_item_update_status(itemSetPtr itemSet, itemPtr item, gboolean newUpdateStatus) {

	g_assert(newUpdateStatus != item->updateStatus);

	item->updateStatus = newUpdateStatus;
	db_item_update(item);

	// FIXME: vfolder update?
}

void itemset_set_item_new_status(itemSetPtr itemSet, itemPtr item, gboolean newStatus) {

	g_assert(newStatus != item->newStatus);

	item->newStatus = newStatus;
	db_item_update(item);

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
