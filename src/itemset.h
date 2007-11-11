/**
 * @file itemset.h interface to handle sets of items
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

#ifndef _ITEMSET_H
#define _ITEMSET_H

#include <libxml/tree.h>
#include "item.h"

/**
 * The itemset interface processes item list actions
 * based on the item set type specified by the node
 * the item set belongs to.
 */

typedef struct itemSet {
	GList		*ids;		/**< the list of item ids */
	gchar		*nodeId;	/**< the feed list node id this item set belongs to */
} *itemSetPtr;

/* item set iterating interface */

typedef void 	(*itemActionFunc)	(itemPtr item);

/**
 * Calls the given callback for each of the items in the item set.
 *
 * @param itemSet	the item set
 * @param callback	the callback
 */
void itemset_foreach (itemSetPtr itemSet, itemActionFunc callback);

/**
 * Merges the given item set into the item set of
 * the given node. Used for node updating.
 *
 * @param itemSet	the item set to merge into
 * @param items		a list of items to merge
 * @param allowUpdates	TRUE if older items may be replaced
 *
 * @returns the number of new merged items
 */
guint itemset_merge_items(itemSetPtr itemSet, GList *items, gboolean allowUpdates);

/**
 * Frees the given item set and all items it contains.
 *
 * @param itemSet	the item set to free
 */
void itemset_free(itemSetPtr itemSet);

#endif
