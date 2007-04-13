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
#include "support.h"

void
itemset_foreach (itemSetPtr itemSet, itemActionFunc callback)
{
	GList	*iter = itemSet->ids;
	
	while(iter) 
	{
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item) 
		{
			(*callback) (item);
			item_unload (item);
		}
		iter = g_list_next (iter);
	}
}

static guint itemset_get_max_item_count(itemSetPtr itemSet) {

	if(ITEMSET_TYPE_FEED == itemSet->type) {
		nodePtr node = node_from_id(itemSet->nodeId);
		return feed_get_max_item_count(node);
	}

	return G_MAXUINT;
}

/* Generic merge logic suitable for feeds */
static gboolean itemset_generic_merge_check(GList *items, itemPtr newItem) {
	GList		*oldItemIdIter = items;
	itemPtr		oldItem = NULL;
	gboolean	found, equal = FALSE;

	/* determine if we should add it... */
	debug1(DEBUG_CACHE, "check new item for merging: \"%s\"", item_get_title(newItem));
		
	/* compare to every existing item in this feed */
	found = FALSE;
	while(oldItemIdIter) {
		oldItem = (itemPtr)(oldItemIdIter->data);
		
		/* try to compare the two items */

		/* trivial case: one item has id the other doesn't -> they can't be equal */
		if(((item_get_id(oldItem) == NULL) && (item_get_id(newItem) != NULL)) ||
		   ((item_get_id(oldItem) != NULL) && (item_get_id(newItem) == NULL))) {	
			/* cannot be equal (different ids) so compare to 
			   next old item */
			oldItemIdIter = g_list_next(oldItemIdIter);
		   	continue;
		} 

		/* just for the case there are no ids: compare titles and HTML descriptions */
		equal = TRUE;

		if(((item_get_title(oldItem) != NULL) && (item_get_title(newItem) != NULL)) && 
		    (0 != strcmp(item_get_title(oldItem), item_get_title(newItem))))		
	    		equal = FALSE;

		if(((item_get_description(oldItem) != NULL) && (item_get_description(newItem) != NULL)) && 
		    (0 != strcmp(item_get_description(oldItem), item_get_description(newItem))))
	    		equal = FALSE;

		/* best case: they both have ids (position important: id check is useless without knowing if the items are different!) */
		if(item_get_id(oldItem)) {			
			if(0 == strcmp(item_get_id(oldItem), item_get_id(newItem))){
				found = TRUE;
				break;
			} else {
				/* different ids, but the content might be still equal (e.g. empty)
				   so we need to explicitly unset the equal flag !!!  */
				equal = FALSE;
			}
		}
			
		if(equal) {
			found = TRUE;
			break;
		}

		oldItemIdIter = g_list_next(oldItemIdIter);
	}
		
	if(!found) {
		debug0(DEBUG_CACHE, "-> item is to be added");
	} else {
		/* if the item was found but has other contents -> update contents */
		if(!equal) {
//			if(((feedPtr)(node_from_id(itemSet->nodeId)->data))->valid) {
//				/* no item_set_new_status() - we don't treat changed items as new items! */
//				item_set_title(oldItem, item_get_title(newItem));
//				item_set_description(oldItem, item_get_description(newItem));
//				oldItem->time = newItem->time;
//				oldItem->updateStatus = TRUE;
//				// FIXME: this does not remove metadata from DB
//				metadata_list_free(oldItem->metadata);
//				oldItem->metadata = newItem->metadata;
//				newItem->metadata = NULL;
//				debug0(DEBUG_CACHE, "-> item already existing and was updated");
//			} else {
//				debug0(DEBUG_CACHE, "-> item updates not merged because of parser errors");
//			}
		} else {
			debug0(DEBUG_CACHE, "-> item already exists");
		}
	}

	return !found;
}

/**
 * Determine wether a given item is to be merged
 * into the itemset or if it was already added.
 */
static gboolean itemset_merge_check(GList *items, itemPtr item) {

	return itemset_generic_merge_check(items, item);	
}

static void itemset_merge_item(itemSetPtr itemSet, itemPtr item) {
	GList		*iter, *items = NULL;
	gboolean	merge;

	debug2(DEBUG_UPDATE, "trying to merge \"%s\" to node id \"%s\"", item_get_title(item), itemSet->nodeId);
	
	/* load all items for merging comparison */
	iter = itemSet->ids;
	while(iter) {
		items = g_list_append(items, item_load(GPOINTER_TO_UINT(iter->data)));
		iter = g_list_next(iter);
	}

	/* first try to merge with existing item */
	merge = itemset_merge_check(items, item);

	/* if it is a new item add it to the item set */	
	if(merge) {
		g_assert(itemSet->nodeId);
		g_assert(!item->nodeId);
		g_assert(!item->id);
		item->nodeId = itemSet->nodeId;
		
		/* step 1: write item to DB */
		db_item_update(item);
		
		/* step 2: add to itemset */
		itemSet->ids = g_list_prepend(itemSet->ids, GUINT_TO_POINTER(item->id));
				
		debug3(DEBUG_UPDATE, "-> added \"%s\" (id=%d) to item set %p...", item_get_title(item), item->id, itemSet);
		
		/* step 3: duplicate detection, mark read if it is a duplicate */
		// FIXME: still needed?
//		if(item->validGuid) {
//			if(item_guid_list_get_duplicates_for_id(item)) {
				// FIXME do something better: item->readStatus = TRUE;
//				debug2(DEBUG_UPDATE, "-> duplicate guid detected: %s -> %s\n", item->id, item->title);
//			}
//		}

		/* step 4: If a new item has enclosures and auto downloading
		   is enabled we start the download. Enclosures added
		   by updated items are not supported. */

//		if((TRUE == ((feedPtr)(node_from_id(itemSet->nodeId)->data))->encAutoDownload) &&
//		   (TRUE == newItem->newStatus)) {
//			GSList *iter = metadata_list_get_values(newItem->metadata, "enclosure");
//			while(iter) {
//				debug1(DEBUG_UPDATE, "download enclosure (%s)", (gchar *)iter->data);
//				ui_enclosure_save(NULL, g_strdup(iter->data), NULL);
//				iter = g_slist_next(iter);
//			}
//		}
	} else {
		debug2(DEBUG_UPDATE, "-> not adding \"%s\" to node id \"%s\"...", item_get_title(item), itemSet->nodeId);
		item_unload(item);
	}
	
	/* unload items again */
	iter = items;
	while(iter) {
		item_unload((itemPtr)(iter->data));
		iter = g_list_next(iter);
	}
	g_list_free(items);
}

void itemset_merge_items(itemSetPtr itemSet, GList *list) {
	GList	*iter;
	guint	max;

	debug_start_measurement (DEBUG_UPDATE);
	
	debug2(DEBUG_UPDATE, "old item set %p of (node id=%s):", itemSet, itemSet->nodeId);
	
	/* Truncate the new itemset if it is longer than
	   the maximum cache size which could cause items
	   to be dropped and added again on subsequent 
	   merges with the same feed content */
	max = itemset_get_max_item_count(itemSet);
	if(g_list_length(list) > max) {
		debug2(DEBUG_UPDATE, "item list too long (%u, max=%u) for merging!", g_list_length(list), max);
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
	
	debug_end_measurement (DEBUG_UPDATE, "merge itemset");
}

void itemset_free(itemSetPtr itemSet) {

	g_list_free(itemSet->ids);
	g_free(itemSet);
}
