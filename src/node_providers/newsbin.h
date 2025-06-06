/**
 * @file newsbin.h  news bin node type implementation
 * 
 * Copyright (C) 2024 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_provider.h"

/**
 * Returns a list of the names of all news bins
 */
GSList * newsbin_get_list (void);

/**
 * newsbin_add_item:
 * @newsbin: The number of the newsbin to add to
 * @item: The item to add
 *
 * Copies existing item to newsbin
 */
void newsbin_add_item (guint32 index, itemPtr item);

/**
 * newsbin_get_provider: (skip)
 */
nodeProviderPtr newsbin_get_provider (void);

#endif
