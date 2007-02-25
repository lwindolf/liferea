/**
 * @file itemset.h interface for different item list implementations
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

enum itemSetTypes {
	ITEMSET_TYPE_INVALID = 0,
	ITEMSET_TYPE_FEED,
	ITEMSET_TYPE_FOLDER,
	ITEMSET_TYPE_VFOLDER
};

typedef struct itemSet {
	guint		type;		/**< the type of the item set */
	GList		*items;		/**< the list of items */
	struct node	*node;		/**< the feed list node this item set belongs to */

	gboolean	valid;		/**< FALSE if libxml2 recovery mode was used to create this item set*/
} *itemSetPtr;

/**
 * Scans all item of a given item set for the given item id.
 * The node must be also given to correctly extract items from
 * merged item lists (like folders)
 *
 * @param itemSet	the item set
 * @param node		the parent node
 * @param nr		the item nr
 *
 * @returns NULL or the first found item
 */
itemPtr itemset_lookup_item(itemSetPtr itemSet, struct node *node, gulong nr);

/**
 * Merges the given item set into the item set of
 * the given node. Used for node updating.
 *
 * @param itemSet	the item set to merge into
 * @param items		a list of items to merge
 */
void itemset_merge_items(itemSetPtr itemSet, GList *items);

/**
 * Prepends a single item to the given item set.
 *
 * @param itemSet	the item set
 * @param item		the item to add
 */
void itemset_prepend_item(itemSetPtr itemSet, itemPtr item);

/**
 * Appends a single item to the given item set.
 *
 * @param itemSet	the item set
 * @param item		the item to add
 */
void itemset_append_item(itemSetPtr itemSet, itemPtr item);

/**
 * Removes a single item of a given item set.
 *
 * @param itemSet	the item set
 * @param item		the item to remove
 */
void itemset_remove_item(itemSetPtr itemSet, itemPtr item);

/**
 * Removes all items of a given item set.
 *
 * @param itemSet	the item set
 */
void itemset_remove_all_items(itemSetPtr itemSet);

/**
 * Changes the "flag" status of a single item of the given itemset.
 *
 * @param itemSet	the item set
 * @param item		the item to change
 * @param newStatus	the new flag status
 */
void itemset_set_item_flag(itemSetPtr itemSet, itemPtr item, gboolean newStatus);

/**
 * Changes the "read" status of a single item of the given itemset.
 *
 * @param itemSet	the item set
 * @param item		the item to change
 * @param newStatus	the new read status
 */
void itemset_set_item_read_status(itemSetPtr itemSet, itemPtr item, gboolean newStatus);

/**
 * Changes the "update" status of a single item of the given itemset.
 *
 * @param itemSet	the item set
 * @param item		the item to change
 * @param newStatus	the new update status
 */
void itemset_set_item_update_status(itemSetPtr itemSet, itemPtr item, gboolean newStatus);

/**
 * Changes the "new" status of a single item of the given itemset.
 *
 * @param itemSet	the item set
 * @param item		the item to change
 * @param newStatus	the new update status
 */
void itemset_set_item_new_status(itemSetPtr itemSet, itemPtr item, gboolean newStatus);

/**
 * Changes the "popup" status of a single item of the given itemset.
 *
 * @param itemSet	the item set
 * @param item		the item to change
 * @param newStatus	the new update status
 */
void itemset_set_item_popup_status(itemSetPtr itemSet, itemPtr item, gboolean newStatus);

/**
 * Serialize the given item set to XML. Does not serialize items!
 * It only creates an XML document frame for an item set.
 *
 * @param node	the node whose item set is to be serialized
 */
xmlDocPtr itemset_to_xml(struct node *node);

/**
 * Frees the given item set and all items it contains.
 *
 * @param itemSet	the item set to free
 */
void itemset_free(itemSetPtr itemSet);

#endif
