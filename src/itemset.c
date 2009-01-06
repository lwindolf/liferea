/**
 * @file itemset.c handling sets of items
 * 
 * Copyright (C) 2005-2008 Lars Lindner <lars.lindner@gmail.com>
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

#include "conf.h"
#include "common.h"
#include "db.h"
#include "debug.h"
#include "enclosure.h"
#include "feed.h"
#include "itemlist.h"
#include "itemset.h"
#include "metadata.h"
#include "node.h"

void
itemset_foreach (itemSetPtr itemSet, itemActionFunc callback)
{
	GList	*iter = itemSet->ids;
	
	while(iter) {
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item) {
			(*callback) (item);
			item_unload (item);
		}
		iter = g_list_next (iter);
	}
}

// FIXME: this ought to be a subscription property!
static guint
itemset_get_max_item_count (itemSetPtr itemSet)
{
	nodePtr node = node_from_id (itemSet->nodeId);
	
	if (node && IS_FEED (node))
		return feed_get_max_item_count (node);

	return G_MAXUINT;
}

/**
 * Generic merge logic suitable for feeds
 *
 * @param items		existing items
 * @param newItem	new item to merge
 * @param maxChecks     maximum number of item checks
 * @param allowUpdates	TRUE if item content update is to be
 *      		allowed for existing items
 *
 * @returns TRUE if merging instead of updating is necessary) 
 */
static gboolean
itemset_generic_merge_check (GList *items, itemPtr newItem, gint maxChecks, gboolean allowUpdates)
{
	GList		*oldItemIdIter = items;
	itemPtr		oldItem = NULL;
	gboolean	found, equal = FALSE;
	gint		i = 0;

	/* determine if we should add it... */
	debug1 (DEBUG_CACHE, "check new item for merging: \"%s\"", item_get_title (newItem));
		
	/* compare to every existing item in this feed */
	found = FALSE;
	while (oldItemIdIter) {
		oldItem = (itemPtr)(oldItemIdIter->data);
		
		/* try to compare the two items */

		/* trivial case: one item has id the other doesn't -> they can't be equal */
		if (((item_get_id (oldItem) == NULL) && (item_get_id (newItem) != NULL)) ||
		    ((item_get_id (oldItem) != NULL) && (item_get_id (newItem) == NULL))) {	
			/* cannot be equal (different ids) so compare to 
			   next old item */
			oldItemIdIter = g_list_next (oldItemIdIter);
		   	continue;
		} 

		/* just for the case there are no ids: compare titles and HTML descriptions */
		equal = TRUE;

		if (((item_get_title (oldItem) != NULL) && (item_get_title (newItem) != NULL)) && 
		     (0 != strcmp (item_get_title (oldItem), item_get_title (newItem))))		
	    		equal = FALSE;

		if (((item_get_description (oldItem) != NULL) && (item_get_description (newItem) != NULL)) && 
		     (0 != strcmp (item_get_description(oldItem), item_get_description (newItem))))
	    		equal = FALSE;

		/* best case: they both have ids (position important: id check is useless without knowing if the items are different!) */
		if (item_get_id (oldItem)) {			
			if (0 == strcmp (item_get_id (oldItem), item_get_id (newItem))) {
				found = TRUE;
				break;
			} else {
				/* different ids, but the content might be still equal (e.g. empty)
				   so we need to explicitly unset the equal flag !!!  */
				equal = FALSE;
			}
		}
			
		if (equal) {
			found = TRUE;
			break;
		}
		
/*		if (i++ > maxChecks) {
			found = FALSE;
			break;
		}*/

		oldItemIdIter = g_list_next (oldItemIdIter);
	}
		
	if (!found) {
		debug0 (DEBUG_CACHE, "-> item is to be added");
	} else {
		/* if the item was found but has other contents -> update contents */
		if (!equal) {
			if (allowUpdates) {
				/* no item_set_new_status() - we don't treat changed items as new items! */
				item_set_title (oldItem, item_get_title (newItem));
				
				/* don't use item_set_description as it does some unwanted length handling 
				   and we want to enforce the new description */
				g_free (oldItem->description);
				oldItem->description = newItem->description;
				newItem->description = NULL;
				
				oldItem->time = newItem->time;
				oldItem->updateStatus = TRUE;
				// FIXME: this does not remove metadata from DB
				metadata_list_free (oldItem->metadata);
				oldItem->metadata = newItem->metadata;
				newItem->metadata = NULL;
				db_item_update (oldItem);
				debug0 (DEBUG_CACHE, "-> item already existing and was updated");
			} else {
				debug0 (DEBUG_CACHE, "-> item updates not merged because of parser errors");
			}
		} else {
			debug0 (DEBUG_CACHE, "-> item already exists");
		}
	}

	return !found;
}

static gboolean
itemset_merge_item (itemSetPtr itemSet, GList *items, itemPtr item, gint maxChecks, gboolean allowUpdates)
{
	gboolean	merge;
	nodePtr		node;

	debug2 (DEBUG_UPDATE, "trying to merge \"%s\" to node id \"%s\"", item_get_title (item), itemSet->nodeId);
	
	/* first try to merge with existing item */
	merge = itemset_generic_merge_check (items, item, maxChecks, allowUpdates);

	/* if it is a new item add it to the item set */	
	if (merge) {
		g_assert (itemSet->nodeId);
		g_assert (!item->nodeId);
		g_assert (!item->id);
		item->nodeId = g_strdup (itemSet->nodeId);
		if (!item->parentNodeId)
			item->parentNodeId = g_strdup (itemSet->nodeId);
		
		/* step 1: write item to DB */
		db_item_update (item);
		
		/* step 2: add to itemset */
		itemSet->ids = g_list_prepend (itemSet->ids, GUINT_TO_POINTER (item->id));
				
		debug3 (DEBUG_UPDATE, "-> added \"%s\" (id=%d) to item set %p...", item_get_title (item), item->id, itemSet);
		
		/* step 3: duplicate detection, mark read if it is a duplicate */
		if (item->validGuid) {
			GSList	*iter, *duplicates;

			duplicates = iter = db_item_get_duplicates (item->sourceId);
			while (iter) {
				debug1 (DEBUG_UPDATE, "-> duplicate guid exists: #%lu", GPOINTER_TO_UINT (iter->data));
				iter = g_slist_next (iter);
			}
			
			if (g_slist_length (duplicates) > 1) {
				item->readStatus = TRUE;	/* no unread counting... */
				item->popupStatus = FALSE;	/* no notification... */
			}
			
			g_slist_free (duplicates);
		}

		/* step 4: Check item for new enclosures to download */
		node = node_from_id (itemSet->nodeId);
		if (node && (((feedPtr)node->data)->encAutoDownload)) {
			GSList *iter = metadata_list_get_values (item->metadata, "enclosure");
			while (iter) {
				debug1 (DEBUG_UPDATE, "download enclosure (%s)", (gchar *)iter->data);
				enclosure_save_as_file (NULL, g_strdup (iter->data), NULL);
				iter = g_slist_next (iter);
			}
		}
	} else {
		debug2 (DEBUG_UPDATE, "-> not adding \"%s\" to node id \"%s\"...", item_get_title (item), itemSet->nodeId);
		item_unload (item);
	}
	
	return merge;
}

static gint
itemset_sort_by_date (gconstpointer a, gconstpointer b)
{
	itemPtr item1 = (itemPtr)a;
	itemPtr item2 = (itemPtr)b;
	
	g_assert(item1 && item2);
	
	/* We have a problem here if all items of the feed
	   do have no date, then this comparison is useless.
	   To avoid such a case we alternatively compare by
	   item id (which should be an ever-increasing number)
	   and thereby indicate merge order as a secondary
	   order criterion */
	if (item1->time == item2->time) {
		if (item1->id < item2->id)
			return 1;
		if (item1->id > item2->id) 
			return -1;
		return 0;
	}
		
	if (item1->time < item2->time)
		return 1;
	if (item1->time > item2->time)
		return -1;
	
	return 0;
}

guint
itemset_merge_items (itemSetPtr itemSet, GList *list, gboolean allowUpdates, gboolean markAsRead)
{
	GList	*iter, *droppedItems = NULL, *items = NULL;
	guint	max, length, toBeDropped, newCount = 0, flagCount = 0;

	debug_start_measurement (DEBUG_UPDATE);
	
	debug2 (DEBUG_UPDATE, "old item set %p of (node id=%s):", itemSet, itemSet->nodeId);
	
	/* 1. Preparation: determine effective maximum cache size 
	
	   The problem here is that the configured maximum cache
	   size might not always be sufficient. We need to check
	   border use cases in the following. */
	   
	length = g_list_length (list);
	max = itemset_get_max_item_count (itemSet);

	/* Preload all items for flag counting and later merging comparison */
	iter = itemSet->ids;
	while (iter) {
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item) {
			items = g_list_append (items, item);
			if (item->flagStatus)
				flagCount++;
		}
		iter = g_list_next (iter);
	}
	debug1(DEBUG_UPDATE, "current cache size: %d", g_list_length(itemSet->ids));
	debug1(DEBUG_UPDATE, "current cache limit: %d", max);
	debug1(DEBUG_UPDATE, "downloaded feed size: %d", g_list_length(list));
	debug1(DEBUG_UPDATE, "flag count: %d", flagCount);
	
	/* Case #1: Avoid having too many flagged items. We count the 
	   flagged items and check if they are fewer than 
	   
	      <cache limit> - <downloaded items> 
	      
	   to ensure that all new items fit in a feed cache full of
	   flagged items.
	   
	   This handling MUST NOT be invoked when the number of items 
	   is larger then the cache size, otherwise we would never 
	   remove any items for large feeds. */
	if ((length < max) && (max < length + flagCount)) {
		max = flagCount + length;
		debug2 (DEBUG_UPDATE, "too many flagged items -> increasing cache limit to %u (node id=%s)", max, itemSet->nodeId);
	}
	
	/* 2. Avoid cache wrapping (if feed size > cache size)
	
	   Truncate the new itemset if it is longer than
	   the maximum cache size which could cause items
	   to be dropped and added again on subsequent 
	   merges with the same feed content */
	if (length > max) {
		debug2 (DEBUG_UPDATE, "item list too long (%u, max=%u) for merging!", length, max);
		guint i = 0;
		GList *iter, *copy;
		iter = copy = g_list_copy (list);
		while (iter) {
			i++;
			if (i > max) {
				itemPtr item = (itemPtr) iter->data;
				debug2 (DEBUG_UPDATE, "ignoring item nr %u (%s)...", i, item_get_title(item));
				item_unload (item);
				list = g_list_remove (list, item);
			}
			iter = g_list_next (iter);
		}
		g_list_free (copy);
	}	 
 
	/* 3. Merge received items to existing item set
	 
	   Items are given in top to bottom display order. 
	   Adding them in this order would mean to reverse 
	   their order in the merged list, so merging needs
	   to be done bottom to top. During this step the
	   item list (items) may exceed the cache limit. */
	iter = g_list_last (list);
	while (iter) {
		itemPtr item = (itemPtr)iter->data;
		
		if (markAsRead)
			item->readStatus = TRUE;
			
		if (itemset_merge_item (itemSet, items, item, length, allowUpdates)) {
			newCount++;
			items = g_list_prepend (items, iter->data);
		}
		iter = g_list_previous (iter);
	}
	g_list_free (list);
	
	debug1(DEBUG_UPDATE, "added %d new items", newCount);
	
	/* 4. Apply cache limit for effective item set size
	      and unload older items as necessary. In this step
	      it is important never to drop flagged items and 
	      to drop the oldest items only. */
	
	if (g_list_length (items) > max)
		toBeDropped = g_list_length (items) - max;
	else
		toBeDropped = 0;

	/* Let's never drop more than 50 items at one time to avoid
	   long duration migrations. It is ok to drop only some of the
	   items here, because we can drop the rest during future
	   feed updates. */
	//if(toBeDropped > 50)
	//	toBeDropped = 50;
	
	debug3 (DEBUG_UPDATE, "%u new items, cache limit is %u -> dropping %u items", newCount, max, toBeDropped);
	items = g_list_sort (items, itemset_sort_by_date);
	iter = g_list_last (items);
	while (iter) {
		itemPtr item = (itemPtr) iter->data;
		if (toBeDropped > 0 && !item->flagStatus) {
			debug2 (DEBUG_UPDATE, "dropping item nr %u (%s)....", item->id, item_get_title (item));
			droppedItems = g_list_append (droppedItems, item);
			/* no unloading here, it's done in itemlist_remove_items() */
			toBeDropped--;
		} else {
			item_unload (item);
		}
		iter = g_list_previous (iter);
	}
	
	if (droppedItems) {
		itemlist_remove_items (itemSet, droppedItems);
		g_list_free (droppedItems);
	}
	
	/* 5. Sanity check to detect merging bugs */
	if (g_list_length (items) > itemset_get_max_item_count (itemSet) + flagCount)
		g_warning ("Fatal: Item merging bug! Resulting item list is too long! Cache limit does not work. This is a severe program bug!");
	
	g_list_free (items);
	
	debug_end_measurement (DEBUG_UPDATE, "merge itemset");
	
	return newCount;
}

void
itemset_free (itemSetPtr itemSet)
{
	g_list_free (itemSet->ids);
	g_free (itemSet);
}
