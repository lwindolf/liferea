/**
 * @file folder.h feed list callbacks for folders
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifndef _FOLDER_H
#define _FOLDER_H

#include "node_type.h"

#define IS_FOLDER(node) (node->type == folder_get_node_type ())

/**
 * Returns the implementation of the folder node type.
 */
nodeTypePtr folder_get_node_type(void);

/**
 * Returns the implementation of the root node type.
 */
nodeTypePtr root_get_node_type(void);

#endif
