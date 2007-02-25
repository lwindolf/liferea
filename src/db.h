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

void db_init(void);

void db_deinit(void);

itemSetPtr db_load_itemset_with_node_id(const gchar *id);

itemPtr db_load_item_with_id(gulong id);

/**
 * To be used on new items. Determine a new unique
 * items id and sets the id for the given item.
 *
 * @param item		the item 
 */
void db_set_item_id(itemPtr item);

void db_update_item(itemPtr item);

void db_update_itemset(itemSetPtr itemSet);

void db_remove_item_with_id(const gchar *id);

void db_remove_all_items_with_node_id(const gchar *id);

guint db_get_unread_count_with_node_id(const gchar *id);
