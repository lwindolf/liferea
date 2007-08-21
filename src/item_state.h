/**
 * @file item_state.c   item state handling
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _ITEM_STATE_H
#define _ITEM_STATE_H

#include "item.h"
#include "node.h"

 /**
 * Sets the flag status of the given item.
 *
 * @param item		the item
 * @param newStatus	new flag status
 */
void item_state_set_flagged (itemPtr item, gboolean newStatus);

/**
 * Sets the read status of the given item.
 *
 * @param item		the item
 * @param newStatus	new read status
 */
void item_state_set_read (itemPtr item, gboolean newStatus);

/**
 * Sets the update status of the given item.
 *
 * @param item		the item
 * @param newStatus	new update status
 */
void item_state_set_updated (itemPtr item, const gboolean newStatus);

/**
 * Marks all items in the given nodes item list read.
 * Does not update the GUI to avoid excessive GUI updates.
 * You need to call feedlist_update() to do so.
 *
 * @param nodeId	the node whose item list is to be modified
 */
void item_state_set_all_read (nodePtr node);

/**
 * Resets the new flag for all items of the given item set.
 *
 * @param nodeId	the node whose item list is to be modified
 */
void item_state_set_all_old (const gchar *nodeId);

/**
 * Resets the popup flag for all items of the given item set.
 *
 * @param nodeId	the node whose item list is to be modified
 */
void item_state_set_all_popup (const gchar *nodeId);

#endif
