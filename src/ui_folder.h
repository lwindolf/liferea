/**
 * @file ui_folder.h GUI folder handling
 * 
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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
 
#ifndef _UI_FOLDER_H
#define _UI_FOLDER_H

#include <gtk/gtk.h>

#define	ROOT_FOLDER_PREFIX	""

/* Adding/removing folders & feeds*/

/**
 * Add a folder to the feedlist
 *
 * @param parent the parent of the new folder, or NULL to insert in
 * the root folder
 * @param folder the folder to add
 * @param the position in which the folder should be added, or -1 to
 * append the folder to the parent.
 */
void ui_add_folder(folderPtr parent, folderPtr folder, gint position);

void ui_folder_add_feed(folderPtr parent, feedPtr fp, gint position);

/**
 * Remove a node from the feedlist and free its ui_data.
 *
 * @param ptr the node to free
 */
void ui_folder_remove_node(nodePtr ptr);

void ui_folder_update(folderPtr folder);
gboolean ui_is_folder_expanded(folderPtr folder);
gboolean ui_folder_is_empty(folderPtr folder);
void checkForEmptyFolders(void);
/* expansion/collapsing */
void ui_folder_set_expansion(folderPtr folder, gboolean expanded);

/* Callbacks */
void on_popup_newfolder_selected(void);
void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data);
void on_popup_foldername_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_foldername_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_foldernamechangebtn_clicked(GtkButton *button, gpointer user_data);
void on_popup_removefolder_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);

#endif
