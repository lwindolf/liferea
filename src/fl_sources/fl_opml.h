/**
 * @file fl_opml.h OPML Planet/Blogroll feed list provider
 * 
 * Copyright (C) 2005-2006 Lars Lindner <lars.lindner@gmx.net>
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
 
#ifndef _FL_OPML_H
#define _FL_OPML_H

#include "node.h"

/**
 * Sets up the given node as a OPML source node.
 *
 * @param parent	parent node (or NULL when importing)
 * @param node		the node
 */
void fl_opml_source_setup(nodePtr parent, nodePtr node);

/**
 * Updates the OPML source of the given node.
 *
 * @param node		the node
 */
void fl_opml_source_update(nodePtr node);

#endif
