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
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"

static const gchar * itemset_get_base_url(itemSetPtr sp) {
	const gchar 	*baseUrl = NULL;

	switch(sp->type) {
		case ITEMSET_TYPE_FEED:
			baseUrl = feed_get_html_url((feedPtr)sp->node->data);
			break;
		case ITEMSET_TYPE_FOLDER:
			break;
		case ITEMSET_TYPE_VFOLDER:
			break;
	}

	return baseUrl;
}

gchar * itemset_render_item(itemSetPtr sp, itemPtr ip) {
	gchar		*tmp, *buffer = NULL;
	const gchar	*baseUrl;

	debug_enter("itemset_render_item");

	baseUrl = itemset_get_base_url(sp);
	ui_htmlview_start_output(&buffer, baseUrl, TRUE);

	tmp = item_render(ip);
	addToHTMLBufferFast(&buffer, tmp);
	g_free(tmp);

	ui_htmlview_finish_output(&buffer);

	/* prevent feed scraping commands to end up as base URI */
	if(!((baseUrl != NULL) &&
	     (baseUrl[0] != '|') &&
	     (strstr(baseUrl, "://") != NULL)))
	   	baseUrl = NULL;

	ui_htmlview_write(ui_mainwindow_get_active_htmlview(), buffer, baseUrl);

	g_free(buffer);

	debug_exit("itemset_render_item");
}

gchar * itemset_render_all(itemSetPtr sp) {
	gchar		*tmp, *buffer = NULL;
	const gchar	*baseUrl;
	GList		*iter;
	itemPtr		ip;

	debug_enter("itemset_render_all");

	baseUrl = itemset_get_base_url(sp);
	ui_htmlview_start_output(&buffer, baseUrl, FALSE);

	iter = sp->items;
	while(iter) {	
		ip = (itemPtr)iter->data;

		if(ip->readStatus) 
			addToHTMLBuffer(&buffer, UNSHADED_START);
		else
			addToHTMLBuffer(&buffer, SHADED_START);
				
		tmp = item_render(ip);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
				
		if(ip->readStatus)
			addToHTMLBuffer(&buffer, UNSHADED_END);
		else
			addToHTMLBuffer(&buffer, SHADED_END);
		

		iter = g_list_next(iter);
	}

	debug_exit("itemset_render_all");

	return buffer;
}

itemPtr itemset_lookup_item(itemSetPtr sp, nodePtr np, gulong nr) {
	GList		*items;
	itemPtr		ip;
		
	items = sp->items;
	while(NULL != items) {
		ip = (itemPtr)(items->data);
		if((ip->nr == nr) && (ip->itemSet->node == np))
			return ip;
		items = g_list_next(items);
	}
	
	return NULL;
}

gboolean itemset_merge_check(itemSetPtr sp, itemPtr ip) {
	gboolean toBeMerged = FALSE;

	switch(sp->type) {
		case ITEMSET_TYPE_FEED:
			toBeMerged = feed_merge_check(sp, ip);
			break;
		case ITEMSET_TYPE_FOLDER:
		case ITEMSET_TYPE_VFOLDER:
		default:
			g_warning("itemset_merge_check(): If this happens something is wrong!");
			break;
	}

	return toBeMerged;
}

void itemset_prepend_item(itemSetPtr sp, itemPtr ip) {

	ip->itemSet = sp;
	ip->nr = ++(sp->lastItemNr);
	sp->items = g_list_prepend(sp->items, ip);
}

void itemset_append_item(itemSetPtr sp, itemPtr ip) {

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
		case ITEMSET_TYPE_FEED:
		case ITEMSET_TYPE_FOLDER:
			/* remove vfolder copies */
			vfolder_remove_item(ip);

			sp->node->needsCacheSave = TRUE;
			
			/* is this really correct? e.g. if there is no 
			   unread/important vfolder? then the remove
			   above would do nothing and decrementing
			   the counters would be wrong, the same when
			   there are multiple vfolders catching an
			   unread item...  FIXME!!! (Lars) */
			feedlist_update_counters(ip->readStatus?0:-1,	
						 ip->newStatus?-1:0);
			break;
		case ITEMSET_TYPE_VFOLDER:
			/* No propagation */
			break;
		default:
			g_error("itemset_remove_item(): unexpected item set type: %d\n", sp->type);
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
		if(ip->sourceNode != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceNode = ip->sourceNode;	/* keep feed pointer because ip might be free'd */
			node_load(sourceNode);
			if(NULL != (sourceItem = itemset_lookup_item(sourceNode->itemSet, sourceNode, ip->sourceNr)))
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
		if(ip->sourceNode != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceNode = ip->sourceNode;	/* keep feed pointer because ip might be free'd */
			node_load(sourceNode);
			if(NULL != (sourceItem = itemset_lookup_item(sourceNode->itemSet, sourceNode, ip->sourceNr)))
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
		if(ip->sourceNode != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceNode = ip->sourceNode;	/* keep feed pointer because ip might be free'd */
			node_load(sourceNode);
			if(NULL != (sourceItem = itemset_lookup_item(sourceNode->itemSet, sourceNode, ip->sourceNr)))
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

	sp->node->needsCacheSave = TRUE;
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
