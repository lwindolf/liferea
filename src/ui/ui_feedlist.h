/**
 * @file ui_feedlist.h GUI feed list handling
 * 
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2005 Raphaël Slinckx <raphael@slinckx.net>
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

#ifndef _UI_FEEDLIST_H
#define _UI_FEEDLIST_H

#include <gtk/gtk.h>
#include "feed.h"

/* constants for attributes in feedstore */
enum {
	FS_LABEL, /* Displayed name */
	FS_ICON,  /* Icon to use */
	FS_PTR,   /* pointer to the folder or feed */
	FS_UNREAD, /* Number of unread items */
	FS_LEN
};

extern GtkTreeStore	*feedstore;

/**
 * Selects the given node in the feed list.
 *
 * @param node	the node to select
 */
void ui_feedlist_select(nodePtr node);

/**
 * Initializes the feed list. For example, it creates the various
 * columns and renderers needed to show the list.
 */
void ui_feedlist_init(GtkWidget *mainview);

/* Selects the proper destination for a new feed based on which feed
 * is currently selected.
 *
 * @param pos a pointer to an integer that will be set to where the
 * item should be created. If it is set to -1, then it should be
 * appended. 0 means prepend.
 *
 * @returns folder into which the feed should be inserted
 */
nodePtr ui_feedlist_get_target_folder(int *pos);

/**
 * Prompt the user for confirmation of a folder or feed, and
 * recursively remove the feed or folder if the user accepts. This
 * function does not block, so the folder/feeds will not have
 * been deleted when this function returns.
 *
 * @param ptr the node to delete
 */
void ui_feedlist_delete_prompt(nodePtr ptr);

/** 
 * @name menu and dialog callbacks 
 * @{
 */

void on_newbtn_clicked(GtkButton *button, gpointer user_data);

/** Feed properties menu creating callback */
void on_menu_properties(GtkMenuItem *menuitem, gpointer user_data);

/** New feed menu creating callback */
void on_menu_feed_new(GtkMenuItem *menuitem, gpointer user_data);

/** New folder menu creating callback */
void on_menu_folder_new(GtkMenuItem *menuitem, gpointer user_data);

/** New plugin menu creating callback */
void on_new_plugin_activate(GtkMenuItem *menuitem, gpointer user_data);

/** New newsbin menu creating callback */
void on_new_newsbin_activate(GtkMenuItem *menuitem, gpointer user_data);

/*@}*/

#endif
