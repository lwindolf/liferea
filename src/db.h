/**
 * @file db.h sqlite backend 
 * 
 * Copyright (C) 2007  Lars Lindner <lars.lindner@gmail.com>
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
#include "itemlist.h"
#include "itemset.h"
#include "subscription.h"
#include "update.h"

/**
 * Open and initialize the DB.
 *
 * @param initial	TRUE for initial opening
 */
void db_init(gboolean initial);

/**
 * Clean up and close the DB.
 */
void db_deinit(void);


/* item set access (note: item sets are identified by the node id string) */

/**
 * Loads all items of the given node id.
 *
 * @param id	the node id
 *
 * @returns a newly allocated item set, must be freed using itemset_free()
 */
itemSetPtr	db_itemset_load(const gchar *id);

/**
 * Removes all items of the given item set from the DB.
 *
 * @param id	the node id
 */
void		db_itemset_remove_all(const gchar *id);

/**
 * Mass items state changing methods. Mark all items of
 * a given item set as read/updated/old/popup.
 *
 * @param id	the node id
 */
void		db_itemset_mark_all_updated(const gchar *id);
void		db_itemset_mark_all_old(const gchar *id);
void		db_itemset_mark_all_popup(const gchar *id);

/**
 * Returns the number of unread items for the given item set.
 *
 * @param id	the node id
 *
 * @returns the number of unread items
 */
guint		db_itemset_get_unread_count(const gchar *id);

/**
 * Returns the number of items for the given item set.
 *
 * @param id	the node id
 *
 * @returns the number of items
 */
guint		db_itemset_get_item_count(const gchar *id);

/**
 * Calls the given callback for each item of the given node id.
 *
 * @param id		the node id
 * @param callback	the callback
 */
void		db_itemset_foreach (const gchar *id, itemActionFunc callback);

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
 * Marks a single item as read. If the item has a valid GUID
 * it will mark all duplicates read.
 *
 * @param item		the item
 */
void	db_item_mark_read (itemPtr item);

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
 * Explicitely start a transaction.
 */
void	db_begin_transaction (void);

/**
 * End previously started transaction.
 */
void	db_end_transaction (void);

/**
 * Commit current transaction.
 */
void	db_commit_transaction (void);

/**
 * Rollback current transaction.
 */
void	db_rollback_transaction (void);

/** Query table flags (to construct join expression for a query) */
typedef enum {
	QUERY_TABLE_ITEMS	= (1 << 0),
	QUERY_TABLE_METADATA	= (1 << 1),
	QUERY_TABLE_NODE	= (1 << 2)
} queryTables;

/** Query column flags (to construct column list) */
typedef enum {
	QUERY_COLUMN_ITEM_ID		= (1 << 0),
	QUERY_COLUMN_ITEM_READ_STATUS	= (1 << 1)
} queryColumns;

/** Query info structure to be used with views and for dynamic item checks. */
typedef struct query {
	guint		tables;		/**< used tables, set of queryTable flags */
	guint		columns;	/**< used columns, set of queryColumn flags */
	
	gchar 		*conditions;	/**< condition in SQL WHERE syntax */
} *queryPtr;

/**
 * Executes the passed matching query and checks if the
 * given item id matches the query.
 *
 * @param id		the item id
 * @param query		query info for item check
 *
 * @returns TRUE if the item matches the query
 */
gboolean db_item_check (guint id, const queryPtr query);

/**
 * Creates a new temporary view (used for search folders)
 *
 * @param id		the view id
 * @param query		query info to construct view
 */
void db_view_create (const gchar *id, const queryPtr query);

/**
 * Removes a temparory view with the given id from the DB session
 *
 * @param id		the view id
 */
void db_view_remove (const gchar *id);

/**
 * Returns an item set of all items for the given view id.
 *
 * @param id		the view id
 *
 * @returns a new item set (to be free'd using itemset_free())
 */
itemSetPtr db_view_load (const gchar *id);

/**
 * Checks wether the given item id belongs to the item list
 * of the given view id.
 *
 * @param id		the view id
 * @param itemId	the item id
 *
 * @returns TRUE if the item belongs to the view
 */
gboolean db_view_contains_item (const gchar *id, gulong itemId);

/** 
 * Returns the item count for the given view id.
 *
 * @param id		the view id
 *
 * @returns the number of items in the view
 */
guint db_view_get_item_count (const gchar *id);

/** 
 * Returns the unread item count for the given view id.
 *
 * @param id		the view id
 *
 * @returns the number of unread items in the view
 */
guint db_view_get_unread_count (const gchar *id);

/**
 * Loads the feed state for the given feed from the DB
 *
 * @param id		the node id
 * @param updateState	update state structure to fill
 *
 * @returns TRUE if update state could be loaded
 */
gboolean db_update_state_load (const gchar *id, updateStatePtr updateState);

/**
 * Updates all attributes and state of the feed in the DB
 *
 * @param id		the node id
 */
void db_update_state_save (const gchar *id, updateStatePtr updateState);

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
 * Returns a list of all subscription node ids.
 *
 * @returns a list of strings (values must be free'd by caller)
 */
GSList * db_subscription_list_load (void);

/**
 * Loads the node with the given node id from the DB.
 *
 * @param id		the node id
 */
void db_node_load (const gchar *id);

/**
 * Updates the given nodes properties in the DB.
 *
 * @param node		the node
 */
void db_node_update (nodePtr node);

/**
 * Remove the given node.
 *
 * @param id		the node id
 */
void db_node_remove (nodePtr node);

#endif
