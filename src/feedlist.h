/**
 * @file feedlist.h  subscriptions as an hierarchic tree
 *
 * Copyright (C) 2005-2014 Lars Windolf <lars.windolf@gmx.de>
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

#define FEEDLIST_TYPE		(feedlist_get_type ())
#define FEEDLIST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), FEEDLIST_TYPE, FeedList))
#define FEEDLIST_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), FEEDLIST_TYPE, FeedListClass))
#define IS_FEEDLIST(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), FEEDLIST_TYPE))
#define IS_FEEDLIST_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), FEEDLIST_TYPE))

typedef struct FeedList		FeedList;
typedef struct FeedListClass	FeedListClass;
typedef struct FeedListPrivate	FeedListPrivate;

extern FeedList *feedlist;

struct FeedList
{
	GObject		parent;
	
	/*< private >*/
	FeedListPrivate	*priv;
};

struct FeedListClass 
{
	GObjectClass parent_class;	
};

GType feedlist_get_type (void);

/**
 * Set up the feed list.
 *
 * @returns the feed list instance
 */
FeedList * feedlist_create (void);

/**
 * Get currently selected feed list node 
 *
 * @returns selected node (or NULL)
 */
nodePtr feedlist_get_selected (void);

/**
 * Query overall number of of unread items.
 *
 * @returns overall number of unread items.
 */
guint feedlist_get_unread_item_count (void);

/**
 * Query overall number of new items.
 *
 * Note: result might be slightly off, but error
 * won't aggregate over time.
 *
 * @returns overall number of new items.
 */
guint feedlist_get_new_item_count (void);

/**
 * Reset the global feed list new item counter.
 *
 * @todo: use signal instead
 */
void feedlist_reset_new_item_count (void);

/**
 * To be called when a feed is updated and has
 * new or dropped items forcing a node unread count
 * update for all affected nodes in the feed list.
 *
 * @param node		the updated node
 */
void feedlist_node_was_updated (nodePtr node);

/**
 * Helper function to query the feed list root node.
 *
 * @returns the feed list root node
 */
nodePtr feedlist_get_root (void);

typedef enum {
	NODE_BY_URL,
	NODE_BY_ID,
	FOLDER_BY_TITLE
} feedListFindType;

/**
 * Search trough list of subscriptions for a node matching exactly 
 * to an criteria defined by the find type and comparison string.
 * Searches recursively from a given parent node or the root node.
 * Always returns just the first occurence in traversal order.
 *
 * @param parent	parent node to traverse from (or NULL)
 * @param type		NODE_BY_(URL|FOLDER_TITLE|ID)
 * @param str		string to compare to
 *
 * @returns a nodePtr or NULL
 */
nodePtr feedlist_find_node (nodePtr parent, feedListFindType type, const gchar *str);

/**
 * Adds a new subscription to the feed list.
 *
 * @param source	the subscriptions source URL
 * @param filter	NULL or the filter for the subscription
 * @param options	NULL or the update options
 * @param flags		download request flags
 */
void feedlist_add_subscription (const gchar *source, const gchar *filter, updateOptionsPtr options, gint flags);

/**
 * Adds a folder to the feed list without any user interaction.
 *
 * @param title		the title of the new folder.
 */
void feedlist_add_folder (const gchar *title);

/**
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
 * @param node		the new node
 */
void feedlist_node_added (nodePtr node);

/**
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
 * @param node		the new node
 */
void feedlist_node_imported (nodePtr node);

/**
 * Removes the given node from the feed list.
 *
 * @param node		the node to remove
 */
void feedlist_remove_node (nodePtr node);

/**
 * Notifies the feed list controller that an existing
 * node was removed from it's source (feed list subtree)
 * and is to be destroyed and to be removed from the 
 * feed list view.
 *
 * @param node		the removed node
 */
void feedlist_node_removed (nodePtr node);

/**
 * Schedules a save requests for the feed list within the next 5s.
 * Triggers state saving for all feed list sources.
 */
void feedlist_schedule_save (void);

/**
 * Resets the update counter of all childs of the given node
 *
 * @param node		the node (or NULL for whole feed list)
 */
void feedlist_reset_update_counters (nodePtr node);

gboolean feedlist_is_writable (void);

/**
 * Triggers a recursive mark-all-read on the given node
 * and updates the feed list afterwards.
 *
 * @param node		the node to start with
 */
void feedlist_mark_all_read (nodePtr node);

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
void feedlist_selection_changed (nodePtr node);

/** 
 * Tries to find the first node with an unread item in the given folder.
 *
 * @param folder	the folder to search
 * 
 * @return a found node or NULL
 */
nodePtr	feedlist_find_unread_feed (nodePtr folder);

/**
 * To be called when node subscription update gained new items.
 *
 * @param newCount	number of new and unread items
 */
void feedlist_new_items (guint newCount);

G_END_DECLS

#endif
