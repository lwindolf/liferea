/**
 * @file node.h common feed list node handling interface
 * 
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _NODE_H
#define _NODE_H

#include "itemset.h"
#include "update.h"

/* Liferea's GUI consists of three parts. Feed list, item list
   and HTML view. The feed list is a view of all available
   nodes. The feed list allows nodes of different types.

   According to the node's type the interface propagates
   user interaction to the feed list node type implementation
   and allows the implementation to change the nodes state. */

/** node view mode types */
enum {
	NODE_VIEW_MODE_NORMAL	= 0,
	NODE_VIEW_MODE_WIDE	= 1,
	NODE_VIEW_MODE_COMBINED	= 2
};

/** generic feed list node structure */
typedef struct node {
	gpointer		data;		/**< node type specific data structure */
	struct subscription	*subscription;	/**< subscription attached to this node (or NULL) */
	struct nodeType		*type;		/**< node type implementation */	
	struct nodeSource	*source;	/**< the feed list source handling this node */
	gchar			*iconFile;	/**< the path of the favicon file */

	/* feed list state properties of this node */
	struct node		*parent;	/**< the parent node (or NULL if at root level) */
	GSList			*children;	/**< ordered list of node children */
	gchar			*id;		/**< unique node identifier string */

	guint			itemCount;	/**< number of items */
	guint			unreadCount;	/**< number of unread items */
	guint			popupCount;	/**< number of items to be notified */
	guint			newCount;	/**< number of recently downloaded items */

	gchar			*title;		/**< the label of the node in the feed list */
	gpointer		icon;		/**< pointer to pixmap, if there is a favicon */
	gboolean		available;	/**< availability of this node (usually the last downloading state) */
	gboolean		expanded;	/**< expansion state (for nodes with childs) */

	/* item list state properties of this node */
	guint		viewMode;	/**< Viewing mode for this node (one of NODE_VIEW_MODE_*) */
	gint		sortColumn;	/**< Sorting column. Set to either IS_TITLE, IS_FAVICON, IS_ENCICON or IS_TIME */
	gboolean	sortReversed;	/**< Sort in the reverse order? */
	
	gboolean	needsUpdate;	/**< if TRUE: the item list has changed and the nodes feed list representation needs to be updated */
	gboolean	needsRecount;	/**< if TRUE: the number of unread/total items is currently unknown and needs recounting */

} *nodePtr;

enum {
	NODE_CAPABILITY_ADD_CHILDS		= (1<<0),	/**< allows adding new childs */
	NODE_CAPABILITY_REMOVE_CHILDS		= (1<<1),	/**< allows removing it's childs */
	NODE_CAPABILITY_SUBFOLDERS		= (1<<2),	/**< allows creating/removing sub folders */
	NODE_CAPABILITY_REMOVE_ITEMS		= (1<<3),	/**< allows removing of single items */
	NODE_CAPABILITY_RECEIVE_ITEMS		= (1<<4),	/**< is a DnD target for item copies */
	NODE_CAPABILITY_REORDER			= (1<<5),	/**< allows DnD to reorder childs */
	NODE_CAPABILITY_SHOW_UNREAD_COUNT	= (1<<6),	/**< display the unread item count in the feed list */
	NODE_CAPABILITY_SHOW_ITEM_COUNT		= (1<<7),	/**< display the absolute item count in the feed list */
	NODE_CAPABILITY_UPDATE			= (1<<8),	/**< node type always has a subscription and can be updated */
	NODE_CAPABILITY_UPDATE_CHILDS		= (1<<9)	/**< childs of this node type can be updated */
};

/**
 * Creates a new node structure.
 *
 * @returns the new node
 */
nodePtr node_new(void);

/**
 * Node lookup by node id.
 *
 * @returns the node with the given id (or NULL)
 */
nodePtr node_from_id(const gchar *id);

/**
 * Sets a nodes parent and updates the feed list. If no
 * parent node is given the parent node of the currently
 * selected feed or the selected folder will be used.
 *
 * @param parent	the parent node (optional can be NULL)
 * @param node		the node
 * @param position	insert position (optional can be 0)
 */
void node_add_child(nodePtr parent, nodePtr node, gint position);

/**
 * Automatic subscription adding (e.g. URL DnD), creates a new feed
 * node and creates a new feed without any user interaction.
 *
 * @param source	the subscriptions source URL
 * @param title		NULL or the node title
 * @param filter	NULL or the filter for the subscription
 * @param options	NULL or the update options
 * @param flags		download request flags
 */
void node_request_automatic_add(const gchar *source, const gchar *title, const gchar *filter, updateOptionsPtr options, gint flags);
	
/**
 * Removes the given node from the feed list.
 *
 * @param parent	the node
 */
void node_request_remove(nodePtr node);

/**
 * Attaches a data structure to the given node.
 *
 * @param node 	the node to attach to
 * @param data	the structure
 */
void node_set_data(nodePtr node, gpointer data);

/**
 * Attaches the subscription to the given node.
 *
 * @param node		the node
 * @param subscription	the subscription
 */
void node_set_subscription (nodePtr node, struct subscription *subscription);

/**
 * Helper function to be used with node_foreach_child()
 * to mass-update subscriptions.
 *
 * @param node		the node
 * @param user_data	update flags
 */
void node_update_subscription (nodePtr node, gpointer user_data);

/**
 * Helper function to be used with node_foreach_child()
 * to mass-auto-update subscriptions.
 *
 * @param node		the node
 */
void node_auto_update_subscription (nodePtr node);

/**
 * Helper function to be used with node_foreach_child()
 * to mass-auto-update subscriptions.
 *
 * @param node		the node
 * @param now		the current timestamp
 */
void node_reset_update_counter (nodePtr node, GTimeVal *now);

/**
 * Determines wether node1 is an ancestor of node2
 *
 * @param node1		the possible ancestor
 * @param node2		the possible child
 * @returns TRUE if node1 is ancestor of node2
 */
gboolean node_is_ancestor(nodePtr node1, nodePtr node2);

/** 
 * Query the node's title for the feed list.
 *
 * @param node	the node
 *
 * @returns the title
 */
const gchar * node_get_title(nodePtr node);

/**
 * Sets the node's title for the feed list.
 *
 * @param node	the node
 * @param title	the title
 */
void node_set_title(nodePtr node, const gchar *title);

/**
 * Update the number of items and unread items of a node from
 * the DB. This method ensures propagation to parent folders.
 *
 * @param node	the node
 */
void node_update_counters(nodePtr node);

/**
 * Recursively marks all items of the given node as read.
 *
 * @param node	the node to process
 */
void node_mark_all_read(nodePtr node);

/**
 * Assigns a new pixmaps as the favicon representing this node.
 *
 * @param node		the node
 * @param icon		a pixmap or NULL
 */
void node_set_icon(nodePtr node, gpointer icon);

/**
 * Returns an appropriate icon for the given node. If the node
 * is unavailable the "unavailable" icon will be returned. If
 * the node is available an existing favicon or the node type
 * specific default icon will be returned.
 *
 * @returns a pixmap or NULL
 */
gpointer node_get_icon(nodePtr node);

/**
 * Returns the name of the favicon cache file for the given node.
 * If there is no favicon a default icon file name will be returned.
 *
 * @param node		the node
 *
 * @return a file name
 */
const gchar * node_get_favicon_file(nodePtr node);

/**
 * Returns a new unique node id.
 *
 * @returns new id
 */
gchar * node_new_id(void);

/**
 * Query the unique id string of the node.
 *
 * @param node	the node
 *
 * @returns id string
 */
const gchar *node_get_id(nodePtr node);

/** 
 * Set the unique id string of the node.
 *
 * @param node	the node
 * @param id 	the id string
 */
void node_set_id(nodePtr node, const gchar *id);

/** 
 * Frees a given node structure.
 *
 * @param the node to free
 */
void node_free(nodePtr node);

/**
 * Refreshes all counters of the given node.
 *
 * @param node	node to update
 */
void node_update_counters(nodePtr node);

/**
 * Helper function for generic node rendering. Performs
 * a generic node serialization to XML and passes the
 * generated XML source document to the XSLT stylesheet
 * with the same name as the node type id.
 *
 * @param node		the node to render
 *
 * @returns XHTML string
 */
gchar * node_default_render(nodePtr node);

/**
 * Saves the given node to cache.
 *
 * @param node	the node
 */
void node_save(nodePtr node);

/**
 * Passes the result of a completed update request to the node
 * type specific processing callback.
 *
 * @param node		the node
 * @param result	the update request result
 * @param flags		update result processing flags
 */
void node_process_update_result (nodePtr node, const struct updateResult * const result, updateFlags flags);

/**
 * Resets the update interval for a given node.
 *
 * @param node	the node
 * @param now	current time
 */
void node_reset_update_counter(nodePtr node, GTimeVal *now);

/**
 * Loads all items of the given node into memory.
 * The caller needs to free the item set using itemset_free()
 *
 * @param node	the node
 *
 * @returns the item set
 */
itemSetPtr node_get_itemset(nodePtr node);

/**
 * Node content rendering
 *
 * @param node	the node
 *
 * @returns string with node rendered in HTML
 */
gchar * node_render(nodePtr node);

/**
 * Called when updating favicons is requested.
 *
 * @param node		the node
 * @param now		current time
 */
void node_update_favicon (nodePtr node, GTimeVal *now);

/**
 * Change/Set the sort column of a given node.
 *
 * @param node		the node
 * @param sortColumn	sort column id
 * @param reversed	TRUE if order should be reversed
 */
void node_set_sort_column(nodePtr node, gint sortColumn, gboolean reversed);

/**
 * Change/Set the viewing mode of a given node.
 *
 * @param node		the node
 * @param newMode	viewing mode (0 = normal, 1 = wide, 2 = combined)
 */
void node_set_view_mode(nodePtr node, guint newMode);

/**
 * Query the viewing mode setting of a given mode.
 *
 * @param node 	the node
 *
 * @returns viewing mode (0 = normal, 1 = wide, 2 = combined)
 */
gboolean node_get_view_mode(nodePtr node);

/**
 * Allows to check wether an node requires to load
 * the item link or the content after selecting an item.
 *
 * @param node	the node
 *
 * @returns TRUE if the item link is to be loaded
 */
gboolean node_load_link_preferred(nodePtr node);

/**
 * Returns the base URL for the given node.
 * If it is a mixed item set NULL will be returned.
 *
 * @param node	the node
 *
 * @returns base URL
 */
const gchar * node_get_base_url(nodePtr node);

/* child nodes iterating interface */

typedef void 	(*nodeActionFunc)	(nodePtr node);
typedef void 	(*nodeActionDataFunc)	(nodePtr node, gpointer user_data);

/**
 * Do not call this method directly! Do use
 * node_foreach_child() or node_foreach_child_data()!
 */
void node_foreach_child_full(nodePtr ptr, gpointer func, gint params, gpointer user_data);

/**
 * Helper function to call node methods for all
 * children of a given node. The given function may
 * modify the children list.
 *
 * @param node	node pointer whose children should be processed
 * @param func	the function to process all found elements
 */
#define node_foreach_child(node, func) node_foreach_child_full(node,func,0,NULL)

/**
 * Helper function to call node methods for all
 * children of a given node. The given function may 
 * modify the children list.
 *
 * @param node	node pointer whose children should be processed
 * @param func	the function to process all found elements
 * @param user_data specifies the second argument that func should be passed
 */
#define node_foreach_child_data(node, func, user_data) node_foreach_child_full(node,func,1,user_data)

#endif
