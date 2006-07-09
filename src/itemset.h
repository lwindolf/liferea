/**
 * @file itemset.h interface for different item list implementations
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

#ifndef _ITEMSET_H
#define _ITEMSET_H

#include "item.h"

/**
 * The itemset interface processes item list actions
 * based on the itemset type specified by the node
 * the itemset belongs to.
 *
 * Currently there are three types of itemsets:
 *   - Feed
 *   - Folder
 *   - VFolder
 *
 * The type of the itemset can be determined from
 * the node type (valid values: FST_FEED, FST_FOLDER
 * and FST_VFOLDER).
 */

enum itemSetTypes {
	ITEMSET_TYPE_INVALID = 0,
	ITEMSET_TYPE_FEED,
	ITEMSET_TYPE_FOLDER,
	ITEMSET_TYPE_VFOLDER
};

typedef struct itemSet {
	guint		type;		/**< the type of the item set */
	GList		*items;		/**< the list of items */
	struct node	*node;		/**< the node this item set belongs to */

	gboolean	valid;		/**< FALSE if libxml2 recovery mode was used to create this item set*/
	gulong		lastItemNr;	/**< internal counter used to uniqely assign item id's. */
} *itemSetPtr;

/**
 * Returns the base URL for the given item set.
 * If it is a mixed item set NULL will be returned.
 *
 * @param itemSet	the itemset
 *
 * @returns base URL
 */
const gchar * itemset_get_base_url(itemSetPtr itemSet);

/**
 * Renders a single item out of a given itemset.
 *
 * @param itemSet	the itemset
 * @param item		the item to render
 *
 * @returns rendered HTML 
 */
gchar * itemset_render_item(itemSetPtr itemSet, itemPtr item);

/**
 * Renders all items of a given itemset (condensed mode) 
 *
 * @param itemSet	the itemset
 *
 * @returns rendered HTML 
 */
gchar * itemset_render(itemSetPtr itemSet);

/**
 * Scans all item of a given item set for the given item id.
 * The node must be also given to correctly extract items from
 * merged itemlists (like folders)
 *
 * @param itemSet	the itemset
 * @param node		the parent node
 * @param nr		the item id
 *
 * @returns NULL or the first found item
 */
itemPtr itemset_lookup_item(itemSetPtr itemSet, struct node *node, gulong nr);

/**
 * Prepends a single item to the given itemset.
 *
 * @param itemSet	the itemset
 * @param item		the item to add
 */
void itemset_prepend_item(itemSetPtr itemSet, itemPtr item);

/**
 * Appends a single item to the given itemset.
 *
 * @param itemSet	the itemset
 * @param item		the item to add
 */
void itemset_append_item(itemSetPtr itemSet, itemPtr item);

/**
 * Removes a single item of a given itemset.
 *
 * @param itemSet	the itemset
 * @param item		the item to remove
 */
void itemset_remove_item(itemSetPtr itemSet, itemPtr item);

/**
 * Removes all items of a given itemset.
 *
 * @param itemSet	the itemset
 */
void itemset_remove_all_items(itemSetPtr itemSet);

/**
 * Changes the flag status of a single item of the given itemset.
 *
 * @param itemSet	the itemset
 * @param item		the item to change
 * @param newStatus	the new flag status
 */
void itemset_set_item_flag(itemSetPtr itemSet, itemPtr item, gboolean newStatus);

/**
 * Changes the read status of a single item of the given itemset.
 *
 * @param itemSet	the itemset
 * @param item		the item to change
 * @param newStatus	the new read status
 */
void itemset_set_item_read_status(itemSetPtr itemSet, itemPtr item, gboolean newStatus);

/**
 * Changes the update status of a single item of the given itemset.
 *
 * @param itemSet	the itemset
 * @param item		the item to change
 * @param newStatus	the new update status
 */
void itemset_set_item_update_status(itemSetPtr itemSet, itemPtr item, gboolean newStatus);

#endif
