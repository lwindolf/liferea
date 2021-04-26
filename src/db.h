/**
 * @file db.h sqlite backend
 *
 * Copyright (C) 2007-2020  Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _DB_H
#define _DB_H

#include <glib.h>

#include "item.h"
#include "itemset.h"
#include "subscription.h"
#include "update.h"

/**
 * Open and initialize the DB.
 */
void    db_init (void);

/**
 * Clean up and close the DB.
 */
void    db_deinit (void);

/* item set access (note: item sets are identified by the node id string) */

/**
 * Loads all items of the given node id.
 *
 * @param id	the node id
 *
 * @returns a newly allocated item set, must be freed using itemset_free()
 */
itemSetPtr	db_itemset_load (const gchar *id);

/**
 * Removes all items of the given item set from the DB.
 *
 * @param id	the node id
 */
void	db_itemset_remove_all (const gchar *id);

/**
 * Mass items state changing methods. Mark all items of
 * a given item set as old/popup.
 *
 * @param id	the node id
 */
void	db_itemset_mark_all_popup (const gchar *id);

/**
 * Returns the number of unread items for the given item set.
 *
 * @param id	the node id
 *
 * @returns the number of unread items
 */
guint	db_itemset_get_unread_count (const gchar *id);

/**
 * Returns the number of items for the given item set.
 *
 * @param id	the node id
 *
 * @returns the number of items
 */
guint   db_itemset_get_item_count (const gchar *id);

/**
 * Returns a batch of items starting with the given
 * offset and no more than the given limit.
 *
 * To be used for batched item loading (search folder loaders)
 *
 * @param itemSet       an itemset to add the items to
 * @param offset        the current offset
 * @param limit         maximum number of items to fetch
 *
 * @returns FALSE if no more items to fetch
 */
gboolean        db_itemset_get (itemSetPtr itemSet, gulong offset, guint limit);

/* item access (note: items are identified by the numeric item id) */

/**
 * Loads the item specified by id from the DB.
 *
 * @param id		the id
 *
 * @returns new item structure, must be free'd using item_unload()
 */
itemPtr	db_item_load(gulong id);

/**
 * Updates all attributes of the item in the DB
 *
 * @param item		the item
 */
void	db_item_update(itemPtr item);

/**
 * Removes the given item from the DB
 *
 * @param item		the item
 */
void	db_item_remove(gulong id);

/**
 * Update the attributes related to item state only.
 *
 * @param item          the item
 */
void    db_item_state_update (itemPtr item);

/**
 * Returns a list of item ids with the given GUID.
 *
 * @param guid	the item GUID
 *
 * @returns a list of item ids
 */
GSList * db_item_get_duplicates(const gchar *guid);

/**
 * Returns a list of node ids containing an item with the given GUID.
 *
 * @param guid	the item GUID
 *
 * @returns a list of node ids (to be free'd using g_free)
 */
GSList * db_item_get_duplicate_nodes(const gchar *guid);

/**
 * Returns an item set of all items for the given search folder id.
 *
 * @param id		the search folder id
 *
 * @returns a new item set (to be free'd using itemset_free())
 */
itemSetPtr      db_search_folder_load (const gchar *id);

/**
 * Removes all items from the given search folder
 *
 * @param id		the search folder id
 */
void    db_search_folder_reset (const gchar *id);

/**
 * Add a list of item ids to a search folder.
 *
 * @param id            the search folder id
 * @param items         the list of items
 */
void    db_search_folder_add_items (const gchar *id, GSList *items);

/**
 * Returns the number of items for the given search folder.
 *
 * @param id	the node id
 *
 * @returns the number of items
 */
guint   db_search_folder_get_item_count (const gchar *id);

/**
 * Returns the number of items for the given search folder.
 *
 * @param id	the node id
 *
 * @returns the number of items
 */
guint   db_search_folder_get_unread_count (const gchar *id);

/**
 * Load the metadata and update state of the given subscription.
 *
 * @param subscription	the subscription whose info to load
 */
void db_subscription_load (subscriptionPtr subscription);

/**
 * Updates (or inserts) the properties of the given subscription in the DB.
 *
 * @param subscription	the subscription
 */
void db_subscription_update (subscriptionPtr subscription);

/**
 * Removes the subscription with the given id from the DB
 *
 * @param id		the node id
 */
void db_subscription_remove (const gchar *id);

/**
 * Updates the given nodes properties in the DB.
 *
 * @param node		the node
 */
void db_node_update (nodePtr node);


/**
 * Clean old nodes from the DB by comparing all DB nodes
 * against the OPML feed list.
 *
 * @param root		the root node
 */
void db_node_cleanup (nodePtr root);

#endif
