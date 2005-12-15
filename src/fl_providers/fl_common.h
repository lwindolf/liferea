/**
 * @file fl_common.h common feedlist provider methods
 * 
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _FL_COMMON_H
#define _FL_COMMON_H

/**
 * Default implementation for feed list plugin
 * node_load method.
 */
void fl_common_node_load(nodePtr np);

/**
 * Default implementation for feed list plugin
 * node_unload method.
 */
void fl_common_node_unload(nodePtr np);

/**
 * Default implementation for feed list plugin
 * node_auto_update method.
 */
void fl_common_node_auto_update(nodePtr np);

/**
 * Default implementation for feed list plugin
 * node_update method.
 */
void fl_common_node_update(nodePtr np, guint flags);

/**
 * Default implementation for feed list plugin
 * node_save method.
 */
void fl_common_node_save(nodePtr np);

/**
 * Default implementation for feed list plugin
 * node_render method.
 */
gchar *fl_common_node_render(nodePtr np); 
#endif
