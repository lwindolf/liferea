/**
 * @file itemset.c support for different item list implementations
 * 
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include "node.h"
#include "feed.h"
#include "vfolder.h"

gchar * itemset_render_item(itemSetPtr sp, itemPtr ip) {
	gchar		*tmp, *buffer = NULL;
	const gchar 	*baseUri = NULL;

	switch(sp->type) {
		case ITEMSET_TYPE_FEED:
			baseUri = feed_get_html_url((feedPtr)ip->itemSet->node->data);
			break;
		case ITEMSET_TYPE_FOLDER:
			baseUri = NULL;
			break;
		case ITEMSET_TYPE_VFOLDER:
			baseUri = NULL;
			break;
	}

	ui_htmlview_start_output(&buffer, baseUri, TRUE);

	tmp = item_render(ip);
	addToHTMLBufferFast(&buffer, tmp);
	g_free(tmp);

	ui_htmlview_finish_output(&buffer);

	/* prevent feed scraping commands to end up as base URI */
	if(!((baseUri != NULL) &&
	     (baseUri[0] != '|') &&
	     (strstr(baseUri, "://") != NULL)))
	   	baseUri = NULL;

	ui_htmlview_write(ui_mainwindow_get_active_htmlview(), buffer, baseUri);

	g_free(buffer);
}

gchar * itemset_render_all(itemSetPtr sp) {

	// FIXME
	return NULL;
}

itemPtr itemset_lookup_item(itemSetPtr sp, gulong nr) {
	GList		*items;
	itemPtr		ip;
		
	items = sp->items;
	while(NULL != items) {
		ip = (itemPtr)(items->data);
		if(ip->nr == nr)
			return ip;
		items = g_list_next(items);
	}
	
	return NULL;
}

void itemset_add_item(itemSetPtr sp, itemPtr ip) {

	ip->itemSet = sp;
	ip->nr = ++(sp->lastItemNr);
	sp->items = g_list_append(sp->items, ip);
}

void itemset_remove_item(itemSetPtr sp, itemPtr ip) {

	if(NULL == g_list_find(sp->items, ip)) {
		g_warning("itemset_remove_item(): item (%s) to be removed not found...", ip->title);
		return;
	}

	/* remove item from itemset */
	sp->items = g_list_remove(sp->items, ip);

	/* propagate item removal to itemset type specific implementation */
	switch(sp->type) {
		ITEMSET_TYPE_FEED:
		ITEMSET_TYPE_FOLDER:

			/* remove vfolder copies */
			vfolder_remove_item(ip);

			sp->node->needsCacheSave = TRUE;
			
			/* is this really correct? e.g. if there is no 
			   unread/important vfolder? then the remove
			   above would do nothing and decrementing
			   the counters would be wrong, the same when
			   there are multiple vfolders catching an
			   unread item...  FIXME!!! (Lars) */
			feedlist_update_counters(item_get_read_status(ip)?0:-1,	
						 item_get_new_status(ip)?-1:0);

			feed_remove_item((feedPtr)ip->itemSet->node->data, ip);
			break;
		ITEMSET_TYPE_VFOLDER:
			vfolder_remove_item(ip);
			break;
	}
}

void itemset_remove_items(itemSetPtr sp) {
	GList	*list, *iter;

	/* hmmm... bad performance when removing a lot of items */
	iter = list = g_list_copy(sp->items);
	while(NULL != iter) {
		itemset_remove_item(sp, (itemPtr)iter->data);
		iter = g_list_next(iter);
	}
	g_list_free(list);

	sp->node->needsCacheSave = TRUE;
}

void itemset_set_item_flag(itemSetPtr sp, itemPtr ip, gboolean newFlagStatus) {
	nodePtr	sourceNode;
	itemPtr	sourceItem;

	g_assert(newFlagStatus != ip->flagStatus);

	ip->flagStatus = newFlagStatus;

	if(ITEMSET_TYPE_VFOLDER == sp->type) {
		/* if this item belongs to a vfolder update the source feed */
		if(ip->sourceSet != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceNode = ip->sourceSet->node;	/* keep feed pointer because ip might be free'd */
			node_load(sourceNode);
			if(NULL != (sourceItem = itemset_lookup_item(sourceNode->itemSet, ip->sourceNr)))
				itemlist_set_flag(sourceItem, newFlagStatus);
			node_unload(sourceNode);
		}
	} else {
		vfolder_update_item(ip);	/* there might be vfolders using this item */
		vfolder_check_item(ip);		/* and check if now a rule matches */
	}
}

void itemset_set_item_read_status(itemSetPtr sp, itemPtr ip, gboolean newReadStatus) {
	nodePtr	sourceNode;
	itemPtr	sourceItem;

	g_assert(newReadStatus != ip->readStatus);

	ip->readStatus = newReadStatus;

	/* Note: unread count updates must be done through the node
	   interface to allow recursive node unread count updates */
	node_update_unread_count(sp->node, (TRUE == newReadStatus)?-1:1);
	
	if(ITEMSET_TYPE_VFOLDER == sp->type) {
		/* if this item belongs to a vfolder update the source feed */
		if(ip->sourceSet != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceNode = ip->sourceSet->node;	/* keep feed pointer because ip might be free'd */
			node_load(sourceNode);
			if(NULL != (sourceItem = itemset_lookup_item(sourceNode->itemSet, ip->sourceNr)))
				itemlist_set_read_status(sourceItem, newReadStatus);
			node_unload(sourceNode);
		}
	} else {		
		vfolder_update_item(ip);	/* there might be vfolders using this item */
		vfolder_check_item(ip);		/* and check if now a rule matches */
	}
}

void itemset_set_item_update_status(itemSetPtr sp, itemPtr ip, gboolean newUpdateStatus) {
	nodePtr	sourceNode;
	itemPtr	sourceItem;

	g_assert(newUpdateStatus != ip->updateStatus);

	ip->updateStatus = newUpdateStatus;

	if(ITEMSET_TYPE_VFOLDER == sp->type) {	
		/* if this item belongs to a vfolder update the source feed */
		if(ip->sourceSet != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceNode = ip->sourceSet->node;	/* keep feed pointer because ip might be free'd */
			node_load(sourceNode);
			if(NULL != (sourceItem = itemset_lookup_item(sourceNode->itemSet, ip->sourceNr)))
				itemlist_set_update_status(sourceItem, newUpdateStatus);
			node_unload(sourceNode);
		}
	} else {
		vfolder_update_item(ip);	/* there might be vfolders using this item */
		vfolder_check_item(ip);		/* and check if now a rule matches */
	}
}

void itemset_set_item_new_status(itemSetPtr sp, itemPtr ip, gboolean newStatus) {

	g_assert(newStatus != ip->newStatus);

	ip->newStatus = newStatus;

	/* Note: new count updates must be done through the node
	   interface to allow global feed list new counter */
	node_update_new_count(sp->node, (TRUE == newStatus)?-1:1);
	
	/* New status is never propagated to vfolders... */
}

void itemset_mark_all_read(itemSetPtr sp) {
	GList	*item, *items;
	itemPtr	ip;

	/* two loops on list copies because the itemlist_set_* 
	   methods may modify the original item list */

	items = g_list_copy(sp->items);
	item = items;
	while(NULL != item) {
		ip = (itemPtr)item->data;
		if(FALSE == ip->readStatus)
			itemset_set_item_read_status(sp, ip, TRUE);
		item = g_list_next(item);
	}
	g_list_free(items);

	items = g_list_copy(sp->items);
	item = items;
	while(NULL != item) {	
		ip = (itemPtr)item->data;
		if(TRUE == ip->updateStatus)
			itemset_set_item_update_status(sp, ip, FALSE);
		item = g_list_next(item);
	}
	g_list_free(items);
}

void itemset_mark_all_old(itemSetPtr sp) {
	GList	*iter, *items;
	itemPtr	ip;

	/* loop on list copy because the itemlist_set_* 
	   methods may modify the original item list */

	items = g_list_copy(sp->items);
	iter = items;
	while(NULL != iter) {
		ip = (itemPtr)iter->data;
		if(TRUE == ip->newStatus)
			itemset_set_item_new_status(sp, ip, FALSE);
		iter = g_list_next(iter);
	}
	g_list_free(items);
}
