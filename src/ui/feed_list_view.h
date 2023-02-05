/**
 * @file feed_list_view.h  the feed list in a GtkTreeView
 *
 * Copyright (C) 2004-2019 Lars Windolf <lars.windolf@gmx.de>
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

#define FEED_LIST_VIEW_TYPE (feed_list_view_get_type ())
G_DECLARE_FINAL_TYPE (FeedListView, feed_list_view, FEED_LIST, VIEW, GObject)

/* constants for attributes in feedstore */
enum {
	FS_LABEL,	/**< Displayed name */
	FS_ICON,	/**< Icon to use */
	FS_PTR,		/**< pointer to the folder or feed */
	FS_UNREAD,	/**< Number of unread items */
	FS_COUNT,	/**< Number of unread items as string */
	FS_LEN
};


enum feedlistViewMode {
	FEEDLIST_VIEW_MODE_NORMAL	= 0,
	FEEDLIST_VIEW_MODE_REDUCED	= 1,
	FEEDLIST_VIEW_MODE_FLAT		= 2,
};


/**
 * feed_list_view_select:
 *
 * Selects the given node in the feed list.
 *
 * @node:	the node to select
 */
void feed_list_view_select (nodePtr node);

/**
 * feed_list_view_create: (skip)
 *
 * Initializes the feed list. To be called only once.
 *
 * @treeview:	A treeview widget to use
 *
 * Returns: new FeedListView
 */
FeedListView * feed_list_view_create (GtkTreeView *treeview);

/**
 * feed_list_view_sort_folder:
 *
 * Sort the feeds of the given folder node.
 *
 * @folder:	the folder
 */
void feed_list_view_sort_folder (nodePtr folder);

void on_menu_delete (GSimpleAction *action, GVariant *parameter, gpointer user_data);

void on_menu_update (GSimpleAction *action, GVariant *parameter, gpointer user_data);
void on_menu_update_all (GSimpleAction *action, GVariant *parameter, gpointer user_data);

void on_action_mark_all_read (GSimpleAction *action, GVariant *parameter, gpointer user_data);

void on_menu_properties (GSimpleAction *action, GVariant *parameter, gpointer user_data);
void on_menu_feed_new (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data);
void on_menu_folder_new (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data);

void on_new_plugin_activate (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data);
void on_new_newsbin_activate (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data);
void on_new_vfolder_activate (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data);

void on_feedlist_view_mode_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data);
void on_titlefilter_entry_changed (GtkEditable *self, gpointer user_data);


/**
 * Determines the tree iter of a given node.
 *
 * @param nodeId	the node id
 */
GtkTreeIter * feed_list_view_to_iter (const gchar *nodeId);

/**
 * Updates the tree iter of a given node.
 *
 * @param nodeId	the node
 * @param iter		the new iter
 */
void feed_list_view_update_iter (const gchar *nodeId, GtkTreeIter *iter);

/**
 * Add a node to the feedlist tree view
 *
s * @param node		the node to add
 */
void feed_list_view_add_node (nodePtr node);

/**
 * Reload the UI feedlist by removing and readding each node
 */
void feed_list_view_reload_feedlist (void);

/**
 * Remove a node from the feedlist and free its ui_data.
 *
 * @param node	the node to free
 */
void feed_list_view_remove_node (nodePtr node);

/**
 * Adds an "empty" node to the given tree iter.
 *
 * @param parent	a tree iter
 */
void feed_list_view_add_empty_node (GtkTreeIter *parent);

/**
 * Removes an "empty" node from the given tree iter.
 *
 * @param parent	a tree iter
 */
void feed_list_view_remove_empty_node (GtkTreeIter *parent);

/**
 * Determines the expansion state of a feed list tree node.
 *
 * @param nodeId	the node id
 *
 * @returns TRUE if the node is expanded
 */
gboolean feed_list_view_is_expanded (const gchar *nodeId);

/**
 * Change the expansion/collapsing of the given folder node.
 *
 * @param folder	the folder node
 * @param expanded	new expansion state
 */
void feed_list_view_set_expansion (nodePtr folder, gboolean expanded);

/**
 * Updates the tree view entry of the given node.
 *
 * @param nodeId	the node id
 */
void feed_list_view_update_node (const gchar *nodeId);

/**
 * Open dialog to rename a given node.
 *
 * @param node		the node to rename
 */
void feed_list_view_rename_node (nodePtr node);

/**
 * Prompt the user for confirmation of a folder or feed, and
 * recursively remove the feed or folder if the user accepts. This
 * function does not block, so the folder/feeds will not have
 * been deleted when this function returns.
 *
 * @param node		the node to remove
 */
void feed_list_view_remove (nodePtr node);

/**
 * Prompt the user for confirmation and forces adding the node,
 * even though another node with the same URL exists.
 *
 * @param tempSubscription	the duplicate URL subscription
 * @param exNode			the existing node
 */
void feed_list_view_add_duplicate_url_subscription (subscriptionPtr tempSubscription, nodePtr exNode);

/**
 * Return the integer value associated to the string representing the view mode.
 * If the view mode is invalid or unrecognized, returns FEEDLIST_VIEW_MODE_NORMAL.
 *
 * @param str_mode A string representing a view mode.
 * @return A integer relative to this mode.
 */
enum feedlistViewMode feed_list_view_mode_string_to_value (const gchar *str_mode);

/**
 * Return a read-only string representing the given view mode.
 * The returned string must not be changed in any way.
 *
 * @param mode a FEEDLIST_VIEW_MODE_* integer.
 * @return an internally allocated const string.
 */
const gchar * feed_list_view_mode_value_to_string (enum feedlistViewMode mode);

#endif
