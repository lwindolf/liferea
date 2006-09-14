/**
 * @file ui_feedlist.h GUI feed list handling
 * 
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
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

typedef struct ui_data {
	GtkTreeIter row;
} ui_data;

extern GtkTreeStore	*feedstore;

/**
 * Selects the given node in the feed list.
 *
 * @param node	the node to select
 */
void ui_feedlist_select(nodePtr node);

typedef void 	(*nodeActionFunc)	(nodePtr node);
typedef void 	(*nodeActionDataFunc)	(nodePtr node, gpointer user_data);

/**
 * Initializes the feed list. For example, it creates the various
 * columns and renderers needed to show the list.
 */
void ui_feedlist_init(GtkWidget *mainview);

/** 
 * Determines the currently selected feed list iterator.
 *
 * @param iter	pointer to iter structure to return selected iter
 */
gboolean ui_feedlist_get_iter(GtkTreeIter *iter);

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
 * Create a new subscription in the currently selected folder.
 *
 * @param source	feed source URL or local file name or piped command
 * @param filter    filename of the filter to use for the new feed
 * @param flags feed request flags to pass to the update requesting subsystem
 */
void ui_feedlist_new_subscription(const gchar *source, const gchar *filter, gint flags);

/**
 * marks all items of the feed of the given tree iter as read 
 *
 * @param iter	an iterator of the feed list to be processed
 */
void ui_feedlist_mark_items_as_unread(GtkTreeIter *iter);

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
void on_popup_mark_as_read(gpointer callback_data,
                           guint callback_action,
                           GtkWidget *widget);

void on_popup_properties(gpointer callback_data, guint callback_action, GtkWidget *widget);

void on_newbtn_clicked(GtkButton *button, gpointer user_data);

void on_fileselect_clicked(GtkButton *button, gpointer user_data);
void on_localfilebtn_pressed(GtkButton *button, gpointer user_data);

void on_filter_feeds_without_unread_headlines_activate(GtkMenuItem *menuitem, gpointer user_data);

/** Feed properties menu creating callback */
void on_menu_properties(GtkMenuItem *menuitem, gpointer user_data);

/** New feed menu creating callback */
void on_menu_feed_new(GtkMenuItem *menuitem, gpointer user_data);

/** New folder menu creating callback */
void on_menu_folder_new(GtkMenuItem *menuitem, gpointer user_data);

/** New plugin menu creating callback */
void on_new_plugin_activate(GtkMenuItem *menuitem, gpointer user_data);

/*@}*/

/* UI folder stuff */
void verify_iter(nodePtr node);
GtkWidget *ui_feedlist_build_prop_dialog(void);

#endif
