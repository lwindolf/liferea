/**
 * @file feedlist.h feedlist handling
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

#ifndef _FEEDLIST_H
#define _FEEDLIST_H

#include <gtk/gtk.h>
#include "feed.h"

/** initializes the feed list handling */
void feedlist_init(void);

/** statistic counter handling methods */
int feedlist_get_unread_item_count(void);
int feedlist_get_new_item_count(void);
int feedlist_get_item_count(void);

void feedlist_increase_new_item_count(void);
void feedlist_decrease_new_item_count(void);
void feedlist_increase_unread_item_count(void);
void feedlist_decrease_unread_item_count(void);
void feedlist_increase_item_count(void);
void feedlist_decrease_item_count(void);

/* feed list manipulation */
void feedlist_reset_new_item_count(void);
void feedlist_update_counters(gint unreadDiff, gint newDiff);

void feedlist_add_feed(folderPtr parent, feedPtr feed, gint position);
void feedlist_add_folder(folderPtr parent, folderPtr folder, gint position);
void feedlist_update_feed(nodePtr fp);
void feedlist_remove_node(nodePtr np);

/**
 * Feed loading and unloading to/from memory.
 */
gboolean feedlist_load_feed(feedPtr fp);
void feedlist_unload_feed(feedPtr fp);

/**
 * Schedules a save requests for the feed list.
 * Triggers state saving for all feed list plugins.
 */
void feedlist_schedule_save(void);

/** 
 * Handles completed feed update requests.
 *
 * @param request	the completed request
 */
void ui_feed_process_update_result(struct request *request);

/* UI callbacks */
void feedlist_selection_changed(nodePtr np);

/* direct user callbacks */
void on_menu_delete(GtkMenuItem *menuitem, gpointer user_data);
void on_menu_update(GtkMenuItem *menuitem, gpointer user_data);
void on_menu_folder_delete(GtkMenuItem *menuitem, gpointer user_data);

void on_popup_refresh_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_allunread_selected(void);
void on_popup_allfeedsunread_selected(void);
void on_popup_delete(gpointer callback_data, guint callback_action, GtkWidget *widget);

void on_refreshbtn_clicked(GtkButton *button, gpointer user_data);

#endif
