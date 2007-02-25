/**
 * @file db.h sqlite backend for item storage
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

#include <glib.h>

#include "item.h"
#include "itemset.h"

/**
 * Open and initialize the DB.
 */
void db_init(void);

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
 * a given item set as read/popup/old.
 *
 * @param id	the node id
 */
void		db_itemset_mark_all_read(const gchar *id);
void		db_itemset_mark_all_popup(const gchar *id);
void		db_itemset_mark_all_old(const gchar *id);

/**
 * Returns the number of unread items for the given item set.
 *
 * @param id	the node id
 *
 * @returns the number of unread items
 */
guint		db_itemset_get_unread_count(const gchar *id);

/* item access (note: items are identified by the numeric item id) */

/**
 * Loads the item specified by id from the DB.
 *
 * @param id		the id
 *
 * @returns new item structure, must be freed using item_free()
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


