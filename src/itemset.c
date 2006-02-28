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
#include "debug.h"
#include "node.h"
#include "feed.h"
#include "vfolder.h"
#include "common.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"

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

gchar * itemset_render_item(itemSetPtr itemSet, itemPtr item) {
	gchar		*tmp, *buffer = NULL;

	debug_enter("itemset_render_item");

	ui_htmlview_start_output(&buffer, itemset_get_base_url(itemSet), TRUE);

	if(item) {
		tmp = item_render(item);
		addToHTMLBufferFast(&buffer, tmp);
		g_free(tmp);
    } else {
		g_warning("error: rendering NULL item requested");
	}

	ui_htmlview_finish_output(&buffer);

	debug_exit("itemset_render_item");

	return buffer;
}

gchar * itemset_render_all(itemSetPtr itemSet) {
	gchar		*tmp, *buffer = NULL;
	const gchar	*baseUrl;

	debug_enter("itemset_render_all");

	ui_htmlview_start_output(&buffer, itemset_get_base_url(itemSet), FALSE);

	GList *iter = itemSet->items;
	while(iter) {	
		itemPtr item = (itemPtr)iter->data;

		if(item->readStatus) 
			addToHTMLBuffer(&buffer, UNSHADED_START);
		else
			addToHTMLBuffer(&buffer, SHADED_START);
				
		// FIXME: for merged itemset each item must 
		// be rendered with the correct base URL
		tmp = item_render(item);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
				
		if(item->readStatus)
			addToHTMLBuffer(&buffer, UNSHADED_END);
		else
			addToHTMLBuffer(&buffer, SHADED_END);
		

		iter = g_list_next(iter);
	}

	ui_htmlview_finish_output(&buffer);

	debug_exit("itemset_render_all");

	return buffer;
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

gboolean itemset_merge_check(itemSetPtr itemSet, itemPtr item) {
	gboolean toBeMerged = FALSE;

	switch(itemSet->type) {
		case ITEMSET_TYPE_FEED:
			toBeMerged = feed_merge_check(itemSet, item);
			break;
		case ITEMSET_TYPE_FOLDER:
		case ITEMSET_TYPE_VFOLDER:
		default:
			g_warning("itemset_merge_check(): If this happens something is wrong!");
			break;
	}

	return toBeMerged;
}

void itemset_prepend_item(itemSetPtr itemSet, itemPtr item) {

	item->itemSet = itemSet;
	item->nr = ++(itemSet->lastItemNr);
	itemSet->items = g_list_prepend(itemSet->items, item);
}

void itemset_append_item(itemSetPtr itemSet, itemPtr item) {

	item->itemSet = itemSet;
	item->nr = ++(itemSet->lastItemNr);
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

	if(ITEMSET_TYPE_VFOLDER == itemSet->type) {
		/* if this item belongs to a vfolder update the source feed */
		if(item->sourceNode) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceNode = item->sourceNode;	/* keep feed pointer because ip might be free'd */
			node_load(sourceNode);
			if(sourceItem = itemset_lookup_item(sourceNode->itemSet, sourceNode, item->sourceNr))
				itemlist_set_flag(sourceItem, newFlagStatus);
			node_unload(sourceNode);
		}
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
	
	if(ITEMSET_TYPE_VFOLDER == itemSet->type) {
		/* if this item belongs to a vfolder update the source feed */
		if(item->sourceNode) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceNode = item->sourceNode;	/* keep feed pointer because ip might be free'd */
			node_load(sourceNode);
			if(sourceItem = itemset_lookup_item(sourceNode->itemSet, sourceNode, item->sourceNr))
				itemlist_set_read_status(sourceItem, newReadStatus);
			node_unload(sourceNode);
		} 
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

	if(ITEMSET_TYPE_VFOLDER == itemSet->type) {	
		/* if this item belongs to a vfolder update the source feed */
		if(item->sourceNode) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceNode = item->sourceNode;	/* keep feed pointer because ip might be free'd */
			node_load(sourceNode);
			if(sourceItem = itemset_lookup_item(sourceNode->itemSet, sourceNode, item->sourceNr))
				itemlist_set_update_status(sourceItem, newUpdateStatus);
			node_unload(sourceNode);
		}
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

void itemset_mark_all_read(itemSetPtr itemSet) {
	GList	*iter, *items;
	itemPtr	item;

	/* two loops on list copies because the itemlist_set_* 
	   methods may modify the original item list */

	items = g_list_copy(itemSet->items);
	iter = items;
	while(iter) {
		item = (itemPtr)iter->data;
		if(!item->readStatus)
			itemset_set_item_read_status(itemSet, item, TRUE);
		iter = g_list_next(iter);
	}
	g_list_free(items);

	items = g_list_copy(itemSet->items);
	iter = items;
	while(iter) {	
		item = (itemPtr)iter->data;
		if(item->updateStatus)
			itemset_set_item_update_status(itemSet, item, FALSE);
		iter = g_list_next(iter);
	}
	g_list_free(items);

	itemSet->node->needsCacheSave = TRUE;
}

void itemset_mark_all_old(itemSetPtr itemSet) {

	/* loop on list copy because the itemlist_set_* 
	   methods may modify the original item list */

	GList *items = g_list_copy(itemSet->items);
	GList *iter = items;
	while(iter) {
		itemPtr item = (itemPtr)iter->data;
		if(item->newStatus)
			itemset_set_item_new_status(itemSet, item, FALSE);
		iter = g_list_next(iter);
	}
	g_list_free(items);
}
