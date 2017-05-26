/**
 * @file feed_list_view.h  the feed list in a GtkTreeView
 * 
 * Copyright (C) 2004-2010 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2005 Raphael Slinckx <raphael@slinckx.net>
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

#ifndef _FEED_LIST_VIEW_H
#define _FEED_LIST_VIEW_H

#include <gtk/gtk.h>
#include "feed.h"

/* constants for attributes in feedstore */
enum {
	FS_LABEL,	/**< Displayed name */
	FS_ICON,	/**< Icon to use */
	FS_PTR,		/**< pointer to the folder or feed */
	FS_UNREAD,	/**< Number of unread items */
	FS_COUNT,	/**< Number of unread items as string */
	FS_LEN
};

extern GtkTreeStore	*feedstore;
extern gboolean		feedlist_reduced_unread;

/**
 * Selects the given node in the feed list.
 *
 * @param node	the node to select
 */
void feed_list_view_select (nodePtr node);

/**
 * Initializes the feed list. For example, it creates the various
 * columns and renderers needed to show the list.
 */
void feed_list_view_init (GtkTreeView *treeview);

/**
 * Sort the feeds of the given folder node.
 *
 * @param folder	the folder
 */
void feed_list_view_sort_folder (nodePtr folder);

void on_menu_delete (GtkWidget *widget, gpointer user_data);

void on_menu_update (void);
void on_menu_update_all (void);

void on_menu_allread (GtkWidget *widget, gpointer user_data);
void on_menu_allfeedsread (GtkWidget *widget, gpointer user_data);

void on_menu_properties (GtkMenuItem *menuitem, gpointer user_data);
void on_menu_feed_new (GtkMenuItem *menuitem, gpointer user_data);
void on_menu_folder_new (GtkMenuItem *menuitem, gpointer user_data);

void on_new_plugin_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_new_newsbin_activate (GtkMenuItem *menuitem, gpointer user_data);
void on_new_vfolder_activate (GtkMenuItem *menuitem, gpointer user_data);

void on_feedlist_reduced_activate (GtkToggleAction *menuitem, gpointer user_data);
#endif
