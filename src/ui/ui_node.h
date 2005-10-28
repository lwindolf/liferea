/**
 * @file ui_node.h GUI folder handling
 * 
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
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
 * Remove a node from the feedlist and free its ui_data.
 *
 * @param np	the node to free
 */
void ui_node_remove_node(nodePtr np);

/**
 * Updates the tree view iter of the given node.
 *
 * @param np	the node
 */
void ui_node_update(nodePtr np);

gboolean ui_node_is_folder_expanded(nodePtr folder);

/**
 * Checks if the given folder node has children 
 * or not and applies a tree store workaround
 * if necessary.
 *
 * @param folder	the folder node
 */
void ui_node_check_if_folder_is_empty(nodePtr folder);

/* expansion/collapsing */
void ui_node_set_expansion(nodePtr folder, gboolean expanded);

/* Callbacks */
void on_popup_newfolder_selected(void);
void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data);
void on_popup_foldername_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_foldername_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_foldernamechangebtn_clicked(GtkButton *button, gpointer user_data);
void on_popup_removefolder_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);

#endif
