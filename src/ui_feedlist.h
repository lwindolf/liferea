/**
 * @file ui_feedlist.h GUI feed list handling
 * 
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

#ifndef _UI_FEEDLIST_H
#define _UI_FEEDLIST_H

#include <gtk/gtk.h>
#include "feed.h"

extern gint 	selected_type;
extern feedPtr	selected_fp;
extern gchar 	*selected_keyprefix;

typedef void 	(*feedActionFunc)	(feedPtr fp);

/**
 * Returns the feed store, creating it if needed.
 *
 * @return feed store pointer.
 */
GtkTreeStore * getFeedStore(void);

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

/**
 * Adds a feed to the feed list.
 *
 * @param fp		the feed to add
 * @param startup	should be TRUE on initial subscriptions loading
 */
void ui_feedlist_load_subscription(feedPtr fp, gboolean startup);

/**
 * Create a new subscription.
 *
 * @param type		feed type
 * @param source	feed source URL or local file name or piped command
 * @param keyprefix	folder key
 * @param showPropDialog TRUE if the property dialog should popup
 */
void ui_feedlist_new_subscription(gint type, gchar *source, gchar * keyprefix, gboolean showPropDialog);

/**
 * helper function to recursivly call feed_save() for all
 * feeds in the feed list
 *
 * @param iter	iterator whose children should be processed (NULL for root)
 */
void ui_feedlist_do_for_all(GtkTreeIter *iter, feedActionFunc *func);

/**
 * helper function to find next unread item 
 *
 * @param iter	an iterator of the feed list to be processed
 */
feedPtr ui_feedlist_find_unread(GtkTreeIter *iter);

/**
 * marks all items of the feed of the given tree iter as read 
 *
 * @param iter	an iterator of the feed list to be processed
 */
void ui_feedlist_mark_items_as_unread(GtkTreeIter *iter);

/** 
 * @name menu and dialog callbacks 
 * @{
 */
void on_popup_refresh_selected(void);
void on_popup_delete_selected(void);
void on_popup_prop_selected(void);
void on_propchangebtn_clicked(GtkButton *button, gpointer user_data);
void on_newbtn_clicked(GtkButton *button, gpointer user_data);
void on_newfeedbtn_clicked(GtkButton *button, gpointer user_data);

void on_fileselect_clicked(GtkButton *button, gpointer user_data);
void on_localfilebtn_pressed(GtkButton *button, gpointer user_data);
/*@}*/

#endif
