/**
 * @file ui_node.h GUI folder handling
 * 
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
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
 
#ifndef _UI_NODE_H
#define _UI_NODE_H

#include <gtk/gtk.h>
#include "node.h"

/**
 * Determines the tree iter of a given node.
 *
 * @param node	the node
 */
GtkTreeIter * ui_node_to_iter(nodePtr node);

/**
 * Updates the tree iter of a given node.
 *
 * @param node	the node
 * @param iter	the new iter
 */
void ui_node_update_iter(nodePtr node, GtkTreeIter *iter);

/**
 * Add a node to the feedlist
 *
 * @param parent	the parent of the new folder, or NULL to 
 *			insert in the root folder
 * @param node		the node to add
 * @param position	the position in which the folder should be 
 *			added, or -1 to append the folder to the parent.
 */
void ui_node_add(nodePtr parent, nodePtr node, gint position);

/**
 * Remove a node from the feedlist and free its ui_data.
 *
 * @param node	the node to free
 */
void ui_node_remove_node(nodePtr node);

/**
 * Adds an "empty" node to the given tree iter.
 *
 * @param parent	a tree iter
 */
void ui_node_add_empty_node(GtkTreeIter *parent);

/**
 * Removes an "empty" node from the given tree iter.
 *
 * @param parent	a tree iter
 */
void ui_node_remove_empty_node(GtkTreeIter *parent);

/**
 * Updates the tree view iter of the given node.
 *
 * @param node	the node
 */
void ui_node_update(nodePtr node);

gboolean ui_node_is_folder_expanded(nodePtr folder);

/**
 * Checks if the given folder node has children 
 * or not and applies a tree store workaround
 * if necessary.
 *
 * @param folder	the folder node
 */
void ui_node_check_if_folder_is_empty(nodePtr folder);

/**
 * Change the expansion/collapsing of the given folder node.
 *
 * @param folder	the folder node
 * @param expanded	new expansion state
 */
void ui_node_set_expansion(nodePtr folder, gboolean expanded);

/**
 * Updates the tree view entry of the given node.
 *
 * @param node	the node
 */
void ui_node_update(nodePtr node);

#endif
