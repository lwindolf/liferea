/**
 * @file feed_list_view.h  the feed list in a GtkListBox
 *
 * Copyright (C) 2004-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "feedlist.h"
#include "node.h"
#include "subscription.h"

#define FEED_LIST_VIEW_TYPE (feed_list_view_get_type ())
G_DECLARE_FINAL_TYPE (FeedListView, feed_list_view, FEED_LIST, VIEW, GObject)

/**
 * feed_list_view_create: (skip)
 *
 * Initializes the feed list. To be called only once.
 *
 * @listbox:	A listbox widget to use
 * @feedlist:	The feed list to display
 *
 * Returns: new FeedListView
 */
FeedListView * feed_list_view_create (GtkListBox *listbox, FeedList *feedlist);

/**
 * Move feed list selection by one row.
 */
void feed_list_view_move_cursor (FeedListView *flv, gint step);

/**
 * feed_list_view_sort_folder:
 *
 * Sort the feeds of the given folder node.
 *
 * @folder:	the folder
 */
void feed_list_view_sort_folder (Node *folder);

/**
 * Determines the expansion state of a feed list tree node.
 *
 * @param nodeId	the node id
 *
 * @returns TRUE if the node is expanded
 */
gboolean feed_list_view_is_expanded (const gchar *nodeId);

/**
 * Open dialog to rename a given node.
 *
 * @param node		the node to rename
 */
void feed_list_view_rename_node (Node *node);

/**
 * Prompt the user for confirmation of a folder or feed, and
 * recursively remove the feed or folder if the user accepts. This
 * function does not block, so the folder/feeds will not have
 * been deleted when this function returns.
 *
 * @param node		the node to remove
 */
void feed_list_view_remove (Node *node);

/**
 * Prompt the user for confirmation and forces adding the node,
 * even though another node with the same URL exists.
 *
 * @param tempSubscription	the duplicate URL subscription
 * @param exNode			the existing node
 */
void feed_list_view_add_duplicate_url_subscription (subscriptionPtr tempSubscription, Node *exNode);

/**
 * feed_list_view_clear_feedlist:
 * @newReduceMode: TRUE to reduce the feed list view
 * 
 * Change reduced mode mode of the feed list view
 */
void feed_list_view_set_reduce_mode (gboolean newReduceMode);

/**
 * feed_list_view_reparent:
 * @node: the node to reparent
 * 
 * Reparent the given node in the feed list view.
 */
void feed_list_view_reparent (Node *node);

#endif
