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

itemSetPtr itemset_new(guint type) {
	itemSetPtr	sp = NULL;

	sp = (itemSetPtr)g_new0(itemSet, 1);
	sp->type = type;
	return sp;
}

void itemset_free(itemSetPtr sp) {

	// FIXME: free item list
	g_free(sp);
}

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
