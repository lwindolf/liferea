/**
 * @file item_state.c   item state controller interface
 * 
 * Copyright (C) 2007-2008 Lars Windolf <lars.windolf@gmx.de>
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
 * Request to change the flag state of the given item.
 *
 * @param item		the item
 * @param newState	new flag state
 */
void item_set_flag_state (itemPtr item, gboolean newState);

/**
 * Notifies the item list controller that the flag 
 * state of the given item has changed.
 *
 * @param item		the item
 * @param newState	new flag state
 */
void item_flag_state_changed (itemPtr item, gboolean newState);

/**
 * Request to change the read state of the given item.
 *
 * @param item		the item
 * @param newState	new read state
 */
void item_set_read_state (itemPtr item, gboolean newState);

/**
 * Notifies the item list controller that the read 
 * state of the given item has changed.
 *
 * @param item		the item
 * @param newState	new read status
 */
void item_read_state_changed (itemPtr item, gboolean newState);

/**
 * Requests to mark read all items in the given nodes item list.
 *
 * @param nodeId	the node whose item list is to be modified
 */
void itemset_mark_read (nodePtr node);

/**
 * Resets the popup flag for all items of the given item set.
 *
 * @param nodeId	the node whose item list is to be modified
 */
void item_state_set_all_popup (const gchar *nodeId);

#endif
