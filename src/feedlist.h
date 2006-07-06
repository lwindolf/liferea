/**
 * @file feedlist.h feedlist handling
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

#ifndef _FEEDLIST_H
#define _FEEDLIST_H

#include <gtk/gtk.h>
#include "node.h"

/** initializes the feed list handling */
void feedlist_init(void);

/**
 * Get feed list root node.
 *
 * @returns root node
 */
nodePtr feedlist_get_root(void);

/**
 * Get currently selected feed list node 
 *
 * @returns selected node (or NULL)
 */
nodePtr feedlist_get_selected(void);

/**
 * Get the node insertion point for new subscriptions. 
 *
 * @returns a parent node for new subscriptions
 */
nodePtr feedlist_get_insertion_point(void);

/** 
 * Get the parent node of the currently 
 * selected node. Can be used to determine
 * insertion point when creating new subscriptions.
 *
 * @returns parent folder node (or root node)
 */
nodePtr feedlist_get_selected_parent(void);

/** statistic counter handling methods */
int feedlist_get_unread_item_count(void);
int feedlist_get_new_item_count(void);

/* feed list manipulation */
void feedlist_reset_new_item_count(void);
void feedlist_update_counters(gint unreadDiff, gint newDiff);

nodePtr feedlist_get_root(void);

void feedlist_update_node(nodePtr np);
void feedlist_remove_node(nodePtr np);

/**
 * Schedules a save requests for the feed list.
 * Triggers state saving for all feed list plugins.
 */
void feedlist_schedule_save(void);

/**
 * Force immediate save requests for the feed list.
 * Similar to feedlist_schedule_save() but runs
 * synchronously.
 */
void feedlist_save(void);

/** 
 * Handles completed feed update requests.
 *
 * @param request	the completed request
 */
void ui_feed_process_update_result(struct request *request);

/* feed list iterating interface */

/**
 * Helper function to recursivly call node methods for all
 * nodes in the feed list. This method is just a wrapper for
 * node_foreach_child().
 *
 * @param func	the function to process all found elements
 */
#define feedlist_foreach(func) node_foreach_child(feedlist_get_root(), func)

/**
 * Helper function to recursivly call node methods for all
 * nodes in the feed list. This method is just a wrapper for
 * node_foreach_child_data().
 *
 * @param func	the function to process all found elements
 * @param user_data specifies the second argument that func should be passed
 */
#define feedlist_foreach_data(func, user_data) node_foreach_child_data(feedlist_get_root(), func, user_data)

/* UI callbacks */

/**
 * Callback for feed list selection change 
 */
void feedlist_selection_changed(nodePtr np);

/** 
 * Tries to find the first node with an unread item in the given folder.
 * 
 * @return folder pointer or NULL
 */
nodePtr	feedlist_find_unread_feed(nodePtr folder);

/* menu callbacks */
void on_menu_delete(GtkWidget *widget, gpointer user_data);

void on_menu_update(GtkWidget *widget, gpointer user_data);
void on_menu_update_all(GtkWidget *widget, gpointer user_data);

void on_menu_allread(GtkWidget *widget, gpointer user_data);
void on_menu_allfeedsread(GtkWidget *widget, gpointer user_data);

#endif
