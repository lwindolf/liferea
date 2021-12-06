/**
 * @file newsbin.h  news bin node type implementation
 * 
 * Copyright (C) 2006 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _NEWSBIN_H
#define _NEWSBIN_H

#include "node_type.h"

/**
 * Returns a list of the names of all news bins
 */
GSList * newsbin_get_list(void);

/**
 * on_action_copy_to_newsbin: (skip)
 * @action:	the action that emitted the signal
 * @parameter:	a GVariant of type "(umt)", first value is the index of the
 * 		newsbin in the list, second is optionnal item id. If no item id is
 * 		given the selected item is used.
 * @user_data:	unused
 *
 * Activate callback for the "copy-item-to-newsbin" action.
 * Copy the selected item to the specified newsbin.
 */
void on_action_copy_to_newsbin(GSimpleAction *action, GVariant *parameter, gpointer user_data);

/* implementation of the node type interface */

#define IS_NEWSBIN(node) (node->type == newsbin_get_node_type ())

/** 
 * Returns the news bin node type implementation.
 */
nodeTypePtr newsbin_get_node_type (void);

#endif
