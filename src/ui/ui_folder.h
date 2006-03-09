/**
 * @file ui_folder.h GUI folder handling
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
 
#ifndef _UI_FOLDER_H
#define _UI_FOLDER_H

#include <gtk/gtk.h>
#include "node.h"

/**
 * Start interaction to create a new sub folder
 * attached to the given parent node.
 *
 * @param parent	the node
 */
void ui_folder_add(nodePtr parent);

/**
 * Start interaction to change the properties of 
 * the given folder node.
 *
 * @param folder	the node
 */
void ui_folder_properties(nodePtr folder);

/* menu callbacks */
void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data);
void on_foldernamechangebtn_clicked(GtkButton *button, gpointer user_data);

#endif
