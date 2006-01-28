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

void feedlist_add_node(nodePtr parent, nodePtr np, gint position);
void feedlist_update_node(nodePtr np);
void feedlist_remove_node(nodePtr np);

void feedlist_load_node(nodePtr np);
void feedlist_unload_node(nodePtr np);

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

#define	FEEDLIST_FILTER_FEED	1	/** Only matches nodes where IS_FEED(node->type) */	
#define	FEEDLIST_FILTER_FOLDER	2	/** Only matches nodes where IS_FOLDER(node->type) */
#define	FEEDLIST_FILTER_PLUGIN	4	/** Only matches nodes where IS_PLUGIN(node->type) */
#define	FEEDLIST_FILTER_ANY	7	/** Matches any node */
#define	FEEDLIST_FILTER_CHILDREN	8	/** Matches immediate children of the given node */

/**
 * Helper function to recursivly call node methods for all
 * elements of the given type in the feed list.
 *
 * @param ptr	node pointer whose children should be processed (NULL defaults to root)
 * @param filter specifies the types of nodes for which func should be called
 * @param func	the function to process all found elements
 * @param params Set to 1 if there will be user_data. Set to 0 for no user data
 * @param user_data specifies the second argument that func should be passed
 */
void feedlist_foreach_full(nodePtr ptr, gint filter, gpointer func, gint params, gpointer user_data);

/**
 * Helper function to recursivly call node methods for all
 * elements of the given type in the feed list.
 *
 * @param ptr	node pointer whose children should be processed (NULL defaults to root)
 * @param filter specifies the types of nodes for which func should be called
 * @param func	the function to process all found elements
 */
#define feedlist_foreach(ptr, filter, func) feedlist_foreach_full(ptr,filter,func,0,NULL)

/**
 * Helper function to recursivly call node methods for all
 * elements of the given type in the feed list.
 *
 * @param ptr	node pointer whose children should be processed (NULL defaults to root)
 * @param filter specifies the types of nodes for which func should be called
 * @param func	the function to process all found elements
 * @param user_data specifies the second argument that func should be passed
 */
#define feedlist_foreach_data(ptr, filter, func, user_data) feedlist_foreach_full(ptr,filter,func,1,user_data)

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
