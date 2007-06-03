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

/* feed list icons */
enum allicons {
	ICON_READ,
	ICON_UNREAD,
	ICON_FLAG,
	ICON_AVAILABLE,
	ICON_AVAILABLE_OFFLINE,
	ICON_UNAVAILABLE,
	ICON_DEFAULT,
	ICON_OCS,
	ICON_FOLDER,
	ICON_VFOLDER,
	ICON_NEWSBIN,
	ICON_EMPTY,
	ICON_EMPTY_OFFLINE,
	ICON_ONLINE,
	ICON_OFFLINE,
	ICON_UPDATED,
	ICON_ENCLOSURE,
	MAX_ICONS
};

extern GdkPixbuf *icons[MAX_ICONS];

/** 
 * Initializes the feed list handling.
 */
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

/** statistic counter handling methods */
int feedlist_get_unread_item_count(void);
int feedlist_get_new_item_count(void);

/* feed list manipulation */
void feedlist_reset_new_item_count(void);
void feedlist_update_counters(gint unreadDiff, gint newDiff);

/**
 * Helper function to query the feed list root node.
 *
 * @returns the feed list root node
 */
nodePtr feedlist_get_root(void);

/**
 * Removes the given node from the feed list and 
 * triggers the permanent removal of the node from cache.
 *
 * @param node		the node to remove
 */
void feedlist_remove_node(nodePtr node);

/**
 * Synchronously saves the feed list. Only to be used upon exit!
 */
void feedlist_save(void);

/**
 * Schedules a save requests for the feed list within the next 5s.
 * Triggers state saving for all feed list sources.
 */
void feedlist_schedule_save(void);

/**
 * Resets the update counter of all childs of the given node
 *
 * @param node		the node (or NULL for whole feed list)
 */
void feedlist_reset_update_counters (nodePtr node);

/* feed list iterating interface */

/**
 * Helper function to recursivly call node methods for all
 * nodes in the feed list. This method is just a wrapper for
 * node_foreach_child().
 *
 * @param func		the function to process all found elements
 */
#define feedlist_foreach(func) node_foreach_child(feedlist_get_root(), func)

/**
 * Helper function to recursivly call node methods for all
 * nodes in the feed list. This method is just a wrapper for
 * node_foreach_child_data().
 *
 * @param func		the function to process all found elements
 * @param user_data	specifies the second argument that func should be passed
 */
#define feedlist_foreach_data(func, user_data) node_foreach_child_data(feedlist_get_root(), func, user_data)

/* UI callbacks */

/**
 * Callback for feed list selection change .
 *
 * @param node		the new selected node
 */
void feedlist_selection_changed(nodePtr node);

/** 
 * Tries to find the first node with an unread item in the given folder.
 *
 * @param folder	the folder to search
 * 
 * @return a found node or NULL
 */
nodePtr	feedlist_find_unread_feed(nodePtr folder);

/* menu callbacks */
void on_menu_delete(GtkWidget *widget, gpointer user_data);

void on_menu_update(GtkWidget *widget, gpointer user_data);
void on_menu_update_all(GtkWidget *widget, gpointer user_data);

void on_menu_allread(GtkWidget *widget, gpointer user_data);
void on_menu_allfeedsread(GtkWidget *widget, gpointer user_data);

#endif
