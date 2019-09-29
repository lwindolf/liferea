/*
 * @file feedlist.h  subscriptions as an hierarchic tree
 *
 * Copyright (C) 2005-2019 Lars Windolf <lars.windolf@gmx.de>
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

#include <glib-object.h>
#include <glib.h>

#include "node.h"
#include "update.h"

G_BEGIN_DECLS

#define FEED_LIST_TYPE (feedlist_get_type ())
G_DECLARE_FINAL_TYPE (FeedList, feedlist, FEED, LIST, GObject)

/**
 * feedlist_create: (skip)
 *
 * Set up the feed list.
 *
 * Returns: (transfer full): the feed list instance
 */
FeedList * feedlist_create (gpointer feedListView);

/**
 * feedlist_get_selected: (skip)
 *
 * Get currently selected feed list node
 *
 * Returns: (transfer none) (nullable): selected node (or NULL)
 */
nodePtr feedlist_get_selected (void);

/**
 * feedlist_get_unread_item_count:
 *
 * Query overall number of unread items.
 *
 * Returns: overall number of unread items.
 */
guint feedlist_get_unread_item_count (void);

/**
 * feedlist_get_new_item_count:
 *
 * Query overall number of new items.
 *
 * Note: result might be slightly off, but error
 * won't aggregate over time.
 *
 * Returns: overall number of new items.
 */
guint feedlist_get_new_item_count (void);

/**
 * feedlist_reset_new_item_count:
 *
 * Reset the global feed list new item counter.
 *
 * TODO: use signal instead
 */
void feedlist_reset_new_item_count (void);

/**
 * feedlist_node_was_updated: (skip)
 * @node:		the updated node
 *
 * To be called when a feed is updated and has
 * new or dropped items forcing a node unread count
 * update for all affected nodes in the feed list.
 *
 */
void feedlist_node_was_updated (nodePtr node);

/**
 * feedlist_get_root: (skip)
 *
 * Helper function to query the feed list root node.
 *
 * Returns: (transfer none): the feed list root node
 */
nodePtr feedlist_get_root (void);

typedef enum {
	NODE_BY_URL,
	NODE_BY_ID,
	FOLDER_BY_TITLE
} feedListFindType;

/**
 * feedlist_find_node: (skip)
 * @parent: (nullable): 	parent node to traverse from (or NULL)
 * @type:		        NODE_BY_(URL|FOLDER_TITLE|ID)
 * @str:		        string to compare to
 *
 * Search trough list of subscriptions for a node matching exactly
 * to an criteria defined by the find type and comparison string.
 * Searches recursively from a given parent node or the root node.
 * Always returns just the first occurence in traversal order.
 *
 * Returns: (nullable) (transfer none): a nodePtr or NULL
 */
nodePtr feedlist_find_node (nodePtr parent, feedListFindType type, const gchar *str);

/**
 * feedlist_add_subscription: (skip)
 * @source:	        the subscriptions source URL
 * @filter: (nullable):	NULL or the filter for the subscription
 * @options: (nullable): NULL or the update options
 * @flags:		download request flags
 *
 * Adds a new subscription to the feed list.
 */
void feedlist_add_subscription (const gchar *source, const gchar *filter, updateOptionsPtr options, gint flags);

/**
 * feedlist_add_subscription_check_duplicate: (skip)
 * @source:	        the subscriptions source URL
 * @filter: (nullable):	NULL or the filter for the subscription
 * @options: (nullable): NULL or the update options
 * @flags:		download request flags
 *
 * Adds a new subscription to the feed list, if there are no subscriptions with
 * the same URL, or opens a confirmation dialog else.
 */
void feedlist_add_subscription_check_duplicate(const gchar *source, const gchar *filter, updateOptionsPtr options, gint flags);

/**
 * feedlist_add_folder:
 * @title:		the title of the new folder.
 *
 * Adds a folder to the feed list without any user interaction.
 */
void feedlist_add_folder (const gchar *title);

/**
 * feedlist_node_added: (skip)
 * @node:		the new node
 *
 * Notifies the feed list controller that a new node
 * was added to the feed list. This method will insert
 * the new node into the feed list view and select
 * the new node.
 *
 * This method is used for all node types (feeds, folders...).
 *
 * Before calling this method the node must be given
 * a parent node using node_set_parent().
 *
 */
void feedlist_node_added (nodePtr node);

/**
 * feedlist_node_imported: (skip)
 * @node:		the new node
 *
 * Notifies the feed list controller that a new node
 * was added to the feed list. Similar to feedlist_node_added()
 * the new node will be added to the feed list but the
 * selection won't be changed.
 *
 * This method is used for all node types (feeds, folders...).
 *
 * Before calling this method the node must be given
 * a parent node using node_set_parent().
 *
 */
void feedlist_node_imported (nodePtr node);

/**
 * feedlist_remove_node: (skip)
 * @node:		the node to remove
 *
 * Removes the given node from the feed list.
 */
void feedlist_remove_node (nodePtr node);

/**
 * feedlist_node_removed: (skip)
 * @node:		the removed node
 *
 * Notifies the feed list controller that an existing
 * node was removed from it's source (feed list subtree)
 * and is to be destroyed and to be removed from the
 * feed list view.
 *
 */
void feedlist_node_removed (nodePtr node);

/**
 * feedlist_schedule_save: (skip)
 *
 * Schedules a save requests for the feed list within the next 5s.
 * Triggers state saving for all feed list sources.
 */
void feedlist_schedule_save (void);

/**
 * feedlist_reset_update_counters: (skip)
 * @node: (nullable):	the node (or NULL for whole feed list)
 *
 * Resets the update counter of all childs of the given node
 *
 */
void feedlist_reset_update_counters (nodePtr node);

gboolean feedlist_is_writable (void);

/**
 * feedlist_mark_all_read: (skip)
 * @node:		the node to start with
 *
 * Triggers a recursive mark-all-read on the given node
 * and updates the feed list afterwards.
 *
 */
void feedlist_mark_all_read (nodePtr node);

/* feed list iterating interface */

/**
 * feedlist_foreach: (skip)
 * @func:		the function to process all found elements
 *
 * Helper function to recursivly call node methods for all
 * nodes in the feed list. This method is just a wrapper for
 * node_foreach_child().
 */
#define feedlist_foreach(func) node_foreach_child(feedlist_get_root(), func)

/**
 * feedlist_foreach_data: (skip)
 * @func:		the function to process all found elements
 * @user_data:  	specifies the second argument that func should be passed
 *
 * Helper function to recursivly call node methods for all
 * nodes in the feed list. This method is just a wrapper for
 * node_foreach_child_data().
 *
 */
#define feedlist_foreach_data(func, user_data) node_foreach_child_data(feedlist_get_root(), func, user_data)

/**
 * feedlist_find_unread_feed: (skip)
 * @folder:	the folder to search
 *
 * Tries to find the first node with an unread item in the given folder.
 *
 * Returns: (nullable) (transfer none): a found node or NULL
 */
nodePtr	feedlist_find_unread_feed (nodePtr folder);

/**
 * feedlist_new_items: (skip)
 * @newCount:	number of new and unread items
 *
 * To be called when node subscription update gained new items.
 */
void feedlist_new_items (guint newCount);

G_END_DECLS

#endif
