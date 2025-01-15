/**
 * @file comments.h comment feed handling
 * 
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _COMMENTS_H
#define _COMMENTS_H

#include "itemset.h"

/**
 * comments_deinit: (skip)
 * 
 * Frees everything related with comments 
 */
void comments_deinit (void);

/**
 * comments_refresh: (skip)
 * @item: the item
 * 
 * Triggers immediate comments retrieval (or update) for the given item.
 */
void comments_refresh (itemPtr item);

/**
 * comments_get_itemset: (skip)
 * @id: the comment feed item
 *
 * Returns the item set of comments for the given item.
 */
itemSetPtr comments_get_itemset (const gchar *id);

#endif
