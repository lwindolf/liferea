/**
 * @file vfolder_loader.h   Loader for search folder items
 *
 * Copyright (C) 2011 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _VFOLDER_LOADER_H
#define _VFOLDER_LOADER_H

#include "item_loader.h"
#include "node.h"

/**
 * Create a new item loader for the given search folder node.
 *
 * @param vfolder       the search folder to load
 *
 * @returns an ItemLoader instance
 */
ItemLoader * vfolder_loader_new (nodePtr vfolder);

#endif
