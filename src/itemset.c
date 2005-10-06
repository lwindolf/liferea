/**
 * @file itemset.c different item list implementations
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

gchar * itemset_render_item(itemSetPtr sp, itemPtr ip) {

	switch(sp->type) {
		ITEMSET_TYPE_FEED:
		ITEMSET_TYPE_FOLDER:
		ITEMSET_TYPE_VFOLDER:
			return item_render(ip);
			break;
	}

	return NULL;
}

gchar * itemset_render_all(itemSetPtr sp) {

	// FIXME
	return NULL;
}

/* This method is called after parsing newly downloaded
   feeds or any other item aquiring was done. This method
   adds the item to the given node and does additional
   updating according to the node type. */
void itemset_add_item(node np, itemPtr ip) {
	gboolean added;

	/* step 1: merge into node type internal data structures */
	switch(np->sp->type) {
		case FST_PLUGIN:
			added = TRUE; /* no merging for now */
			break;
		case FST_FEED:
			added = feed_add_item((feedPtr)np->data, ip);
			break;
		case FST_FOLDER:
			added = TRUE; /* nothing to do */
			break;
		case FST_VFOLDER:
			added = TRUE;
			break;
	}

	/* step 2: add to the itemset */
	if(added)
		np->itemSet = g_slist_append(np->itemSet, ip);

	/* step 3: update statistics */
	if(added) {
		/* Always update the node counter statistics */
		if(FALSE == item_get_read_status(ip))
			node_increase_unread_counter(np);
			
		// FIXME: prevent the next two for folders+vfolders?
		if(TRUE == item_get_popup_status(ip))
			node_increase_popup_counter(np);
			
		if(TRUE == item_get_new_status(ip))
			node_increase_new_counter(np);
			
		/* Never update the overall feed list statistic 
		   for folders and vfolders (because this are item
		   list types with item copies or references)! */
		if((FST_FOLDER != np->type) && (FST_VFOLDER != np->type))
			feedlist_update_counters(item_get_read_status(ip)?0:1,
						 item_get_new_status(ip)?1:0);
	}
}

void itemset_remove_item(itemSetPtr sp, itemPtr ip) {

	if(NULL == g_slist_find(sp->items, ip)) {
		g_warning("itemset_remove_item(): item (%s) to be removed not found...", ip->title);
		return;
	}

	switch(sp->type) {
		ITEMSET_TYPE_FEED:
		ITEMSET_TYPE_FOLDER:
			sp->items = g_slist_remove(sp->items, ip);

			/* remove vfolder copies */
			vfolder_remove_item(ip);

			// FIXME: np->needsCacheSave = TRUE
			
			/* is this really correct? e.g. if there is no 
			   unread/important vfolder? then the remove
			   above would do nothing and decrementing
			   the counters would be wrong, the same when
			   there are multiple vfolders catching an
			   unread item...  FIXME!!! (Lars) */
			feedlist_update_counters(item_get_read_status(ip)?0:-1, 	
						 item_get_new_status(ip)?-1:0);

			/* remove the original */
			feed_remove_item(ip);
			break;
		ITEMSET_TYPE_VFOLDER:
			/* just remove the item from the vfolder */
			feed_remove_item(ip);
			break;
	}
}

void itemset_remove_items(itemSetPtr sp) {
	GSList	*list, *iter;

	/* hmmm... bad performance when removing a lot of items */
	iter = list = g_slist_copy(sp->items);
	while(NULL != iter) {
		itemset_remove_item(sp, (itemPtr)iter->data);
		iter = g_slist_next(iter);
	}
	g_slist_free(list);

	// FIXME: np->needsCacheSave = TRUE
}

void itemset_set_item_flag(itemSetPtr sp, itemPtr ip, gboolean newStatus) {

	item_set_flag_status(ip, newStatus);
	if(ITEMSET_TYPE_VFOLDER == sp->type) {
		/* if this item belongs to a vfolder update the source feed */
		if(ip->sourceFeed != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceFeed = ip->sourceFeed;	/* keep feed pointer because ip might be free'd */
			feedlist_load_feed(sourceFeed);
			if(NULL != (sourceItem = feed_lookup_item(sourceFeed, ip->sourceNr)))
				itemlist_set_flag(sourceItem, newStatus);
			feedlist_unload_feed(sourceFeed);
		}
	} else {
		vfolder_update_item(ip);	/* there might be vfolders using this item */
		vfolder_check_item(ip);		/* and check if now a rule matches */
	}
}

void itemset_set_item_read_status(itemSetPtr sp, itemPtr ip, gboolean newStatus) {

	item_set_read_status(ip, newStatus);
	if(ITEMSET_TYPE_VFOLDER == sp->type) {
		/* if this item belongs to a vfolder update the source feed */
		if(ip->sourceFeed != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceFeed = ip->sourceFeed;	/* keep feed pointer because ip might be free'd */
			feedlist_load_feed(sourceFeed);
			if(NULL != (sourceItem = feed_lookup_item(sourceFeed, ip->sourceNr)))
				itemlist_set_read_status(sourceItem, newStatus);
			feedlist_unload_feed(sourceFeed);
		}
	} else {		
		vfolder_update_item(ip);	/* there might be vfolders using this item */
		vfolder_check_item(ip);		/* and check if now a rule matches */
		feedlist_update_counters(newStatus?-1:1, 0);
	}
}

void itemset_set_item_update_status(itemSetPtr sp, itemPtr ip, gboolean newStatus) {

	item_set_update_status(ip, newStatus);
	if(ITEMSET_TYPE_VFOLDER == sp->type) {	
		/* if this item belongs to a vfolder update the source feed */
		if(ip->sourceFeed != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceFeed = ip->sourceFeed;	/* keep feed pointer because ip might be free'd */
			feedlist_load_feed(sourceFeed);
			if(NULL != (sourceItem = feed_lookup_item(sourceFeed, ip->sourceNr)))
				itemlist_set_update_status(sourceItem, newStatus);
			feedlist_unload_feed(sourceFeed);
		}
	} else {
		vfolder_update_item(ip);	/* there might be vfolders using this item */
		vfolder_check_item(ip);		/* and check if now a rule matches */
	}
}
