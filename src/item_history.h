/**
 * @file item_history.h tracking recently viewed items
 *
 * Copyright (C) 2012 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _ITEM_HISTORY
#define _ITEM_HISTORY

#include "item.h"

/**
 * Add a new item to the item history stack.
 *
 * @param id	the id of the item to add
 */
void item_history_add (guint id);

/**
 * Returns the previous item in the item history.
 *
 * @returns an item (or NULL) to be free'd with item_unload()
 */
itemPtr item_history_get_previous (void);

/**
 * Returns the next item in history.
 *
 * @returns an item (or NULL) to be free'd with item_unload()
 */
itemPtr item_history_get_next (void);

/**
 * Check whether a previous item exists in the item history.
 *
 * @returns TRUE if there is an item
 */
gboolean item_history_has_previous (void);

/**
 * Check whether a following item exists in the item history.
 *
 * @returns TRUE if there is an item
 */
gboolean item_history_has_next (void);

#endif
