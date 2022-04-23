/*
 * @file itemset.c handling sets of items
 *
 * Copyright (C) 2005-2017 Lars Windolf <lars.windolf@gmx.de>
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

#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "enclosure.h"
#include "feed.h"
#include "itemlist.h"
#include "itemset.h"
#include "metadata.h"
#include "node.h"
#include "rule.h"
#include "vfolder.h"
#include "fl_sources/node_source.h"

void
itemset_foreach (itemSetPtr itemSet, itemActionFunc callback, gpointer userdata)
{
	GList	*iter = itemSet->ids;

	while(iter) {
		itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
		if (item) {
			(*callback) (item, userdata);
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
 * itemset_generic_merge_check: (skip)
 * @items:		existing items
 * @newItem:		new item to merge
 * @maxChecks: 		maximum number of item checks
 * @allowUpdates:	TRUE if item content update is to be
 *      		allowed for existing items
 * @allowStateChanges:	TRUE if item state shall be
 *				overwritten by source
 *
 * Generic merge logic suitable for feeds
 *
 * Returns: TRUE if merging instead of updating is necessary)
 */
static gboolean
itemset_generic_merge_check (GList *items, itemPtr newItem, gint maxChecks, gboolean allowUpdates, gboolean allowStateChanges)
{
	GList		*oldItemIdIter = items;
	itemPtr		oldItem = NULL;
	gboolean	found, equal = FALSE;
	guint		reason = 0;

	/* determine if we should add it... */
	debug3 (DEBUG_CACHE, "check new item for merging: \"%s\", %i, %i", item_get_title (newItem), allowUpdates, allowStateChanges);

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

		equal = TRUE;

		if (!item_get_id (oldItem)) {
			/* just for the case there are no ids: compare titles and HTML descriptions */
			if (((item_get_title (oldItem) != NULL) && (item_get_title (newItem) != NULL)) &&
			     (0 != strcmp (item_get_title (oldItem), item_get_title (newItem)))) {
		    		equal = FALSE;
				reason |= 1;
			}

			if (((item_get_description (oldItem) != NULL) && (item_get_description (newItem) != NULL)) &&
			     (0 != strcmp (item_get_description(oldItem), item_get_description (newItem)))) {
		    		equal = FALSE;
				reason |= 2;
			}
		} else {
			/* best case: they both have ids (position important: id check is useless without knowing if the items are different!) */
			if (0 == strcmp (item_get_id (oldItem), item_get_id (newItem))) {
				if (allowStateChanges) {
					/* found corresponding item, check if they are REALLY equal (e.g. read status may have changed) */
					if(oldItem->readStatus != newItem->readStatus) {
						equal = FALSE;
						reason |= 4;
					}
					if(oldItem->flagStatus != newItem->flagStatus) {
						equal = FALSE;
						reason |= 8;
					}
				}

				found = TRUE;
				break;
			} else {
				/* different ids, but the content might be still equal (e.g. empty)
				   so we need to explicitly unset the equal flag !!!  */
				equal = FALSE;
				reason |= 16;
			}
		}

		if (equal) {
			found = TRUE;
			break;
		}

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

				/* Only update item state for feed sources where it is necessary
				   which means online accounts we sync against, but not normal
				   online feeds where items have no read status. */
				if (allowStateChanges) {
					/* To avoid notification spam from external
					   sources: never set read items to unread again! */
					if ((!oldItem->readStatus) && (newItem->readStatus))
						oldItem->readStatus = newItem->readStatus;

					oldItem->flagStatus = newItem->flagStatus;
				}

				db_item_update (oldItem);
				debug1 (DEBUG_CACHE, "-> item already existing and was updated, reason %x", reason);
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
	gboolean	allowStateChanges = FALSE;
	gboolean	merge;
	nodePtr		node;

	debug2 (DEBUG_UPDATE, "trying to merge \"%s\" to node id \"%s\"", item_get_title (item), itemSet->nodeId);

	g_assert (itemSet->nodeId);
	node = node_from_id (itemSet->nodeId);
	if (node)
		allowStateChanges = NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC;

	/* first try to merge with existing item */
	merge = itemset_generic_merge_check (items, item, maxChecks, allowUpdates, allowStateChanges);

	/* if it is a new item add it to the item set */
	if (merge) {
		g_assert (!item->nodeId);
		g_assert (!item->id);
		item->nodeId = g_strdup (itemSet->nodeId);
		if (!item->parentNodeId)
			item->parentNodeId = g_strdup (itemSet->nodeId);

		/* step 1: write item to DB */
		db_item_update (item);

		/* step 2: add to itemset */
		itemSet->ids = g_list_prepend (itemSet->ids, GUINT_TO_POINTER (item->id));

		/* step 3: trigger async enrichment of item description */
		if (node && IS_FEED (node) && ((feedPtr)node->data)->html5Extract)
			feed_enrich_item (node->subscription, item);

		debug3 (DEBUG_UPDATE, "-> added \"%s\" (id=%d) to item set %p...", item_get_title (item), item->id, itemSet);

		/* step 4: duplicate detection, mark read if it is a duplicate */
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

		/* step 5: Check item for new enclosures to download */
		if (node && (((feedPtr)node->data)->encAutoDownload)) {
			GSList *iter = metadata_list_get_values (item->metadata, "enclosure");
			while (iter) {
				enclosurePtr enc = enclosure_from_string (iter->data);
				debug1 (DEBUG_UPDATE, "download enclosure (%s)", (gchar *)iter->data);
				enclosure_download (NULL, enc->url, FALSE /* non interactive */);
				iter = g_slist_next (iter);
				enclosure_free (enc);
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
	guint	i, max, length, toBeDropped, newCount = 0, flagCount = 0;
	nodePtr	node;

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

		/* reach max element */
		for(i = 0, iter = list; (i < max) && iter; ++i)
			iter = g_list_next (iter);

		/* and remove all following elements */
		while (iter) {
			itemPtr item = (itemPtr) iter->data;
			debug2 (DEBUG_UPDATE, "ignoring item nr %u (%s)...", ++i, item_get_title (item));
			item_unload (item);
			iter = g_list_next (iter);
			list = g_list_remove (list, item);
		}
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

	vfolder_foreach (node_update_counters);

	node = node_from_id (itemSet->nodeId);
	if (node && (NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC))
		node_update_counters (node);

	debug1(DEBUG_UPDATE, "added %d new items", newCount);

	/* 4. Apply cache limit for effective item set size
	      and unload older items as necessary. In this step
	      it is important never to drop flagged items and
	      to drop the oldest items only. */

	if (g_list_length (items) > max)
		toBeDropped = g_list_length (items) - max;
	else
		toBeDropped = 0;

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
		debug0 (DEBUG_CACHE, "Fatal: Item merging bug! Resulting item list is too long! Cache limit does not work. This is a severe program bug!");

	g_list_free (items);

	debug_end_measurement (DEBUG_UPDATE, "merge itemset");

	return newCount;
}

gboolean
itemset_check_item (itemSetPtr itemSet, itemPtr item)
{
	gboolean	result = TRUE;
	GSList		*iter = itemSet->rules;

	while (iter) {
		rulePtr		rule = (rulePtr) iter->data;
		ruleCheckFunc	func = rule->ruleInfo->checkFunc;
		gboolean	ruleResult = FALSE;

		ruleResult = (*func) (rule, item);
		result &= (rule->additive)?ruleResult:!ruleResult;
		if (itemSet->anyMatch && ruleResult)
			return TRUE;

		iter = g_slist_next (iter);
	}

	return result;
}

void
itemset_add_rule (itemSetPtr itemSet,
                  const gchar *ruleId,
                  const gchar *value,
                  gboolean additive)
{
	rulePtr		rule;

	rule = rule_new (ruleId, value, additive);
	if (rule)
		itemSet->rules = g_slist_append (itemSet->rules, rule);
	else
		g_warning ("unknown search folder rule id: \"%s\"", ruleId);
}

void
itemset_free (itemSetPtr itemSet)
{
	GSList		*rule;

	if (!itemSet)
		return;

	rule = itemSet->rules;
	while (rule) {
		rule_free (rule->data);
		rule = g_slist_next (rule);
	}
	g_slist_free (itemSet->rules);
	g_list_free (itemSet->ids);
	g_free (itemSet);
}
