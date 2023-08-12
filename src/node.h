/*
 * @file node.h  hierarchic feed list node interface
 * 
 * Copyright (C) 2003-2022 Lars Windolf <lars.windolf@gmx.de>
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
#include "node_view.h"

/* Liferea's GUI consists of three parts. Feed list, item list
   and HTML view. The feed list is a view of some or all available
   nodes. The feed list allows nodes of different types defining
   different UI behaviours.

   According to the node's type this interface propagates
   user interaction to the feed list node type implementation
   and allows the implementation to change the nodes state. 
 
   This interface is to hide the node type and node source type
   complexity for the GUI, scripting and updating functionality.
 */

/* generic feed list node structure */
typedef struct node {
	gpointer		data;		/*<< node type specific data structure */
	struct subscription	*subscription;	/*<< subscription attached to this node (or NULL) */
	struct nodeType		*type;		/*<< node type implementation */
	struct nodeSource	*source;	/*<< the feed list source handling this node */
	gchar			*iconFile;	/*<< the path of the favicon file */

	/* feed list state properties of this node */
	struct node		*parent;	/*<< the parent node (or NULL if at root level) */
	GSList			*children;	/*<< ordered list of node children */
	gchar			*id;		/*<< unique node identifier string */

	guint			itemCount;	/*<< number of items */
	guint			unreadCount;	/*<< number of unread items */
	guint			popupCount;	/*<< number of items to be notified */
	guint			newCount;	/*<< number of recently downloaded items */

	gchar			*title;		/*<< the label of the node in the feed list */
	gpointer		icon;		/*<< favicon GdkPixBuf (or NULL) */
	gboolean		available;	/*<< availability of this node (usually the last downloading state) */
	gboolean		expanded;	/*<< expansion state (for nodes with childs) */

	/* item list state properties of this node */
	nodeViewSortType	sortColumn;	/*<< Node specific item view sort attribute. */
	gboolean		sortReversed;	/*<< Sort in the reverse order? */
	
	/* rendering behaviour of this node */
	gboolean	loadItemLink;	/*<< if TRUE do automatically load the item link into the HTML pane */
	
	/* current state of this node */	
	gboolean	needsUpdate;	/*<< if TRUE: the item list has changed and the nodes feed list representation needs to be updated */
	gboolean	needsRecount;	/*<< if TRUE: the number of unread/total items is currently unknown and needs recounting */

} *nodePtr;

/**
 * node_new: (skip)
 * Creates a new node structure.
 *
 * Returns: (transfer full): the new node
 */
nodePtr node_new (struct nodeType *type);

/**
 * node_is_used_id: (skip)
 * @id:	the node id to check
 *
 * Can be used to check whether an id is used or not.
 *
 * Returns: (transfer none) (nullable): the node with the given id (or NULL)
 */
nodePtr node_is_used_id (const gchar *id);

/**
 * node_from_id: (skip)
 * @id:	the node id to look up
 *
 * Node lookup by node id. Will report an error if the queried
 * id does not exist.
 *
 * Returns: (transfer none) (nullable): the node with the given id (or NULL)
 */
nodePtr node_from_id (const gchar *id);

/**
 * node_set_parent: (skip)
 * @node:		the node
 * @parent: (nullable):	the parent node (optional can be NULL)
 * @position:   	insert position (optional can be 0)
 *
 * Sets a nodes parent. If no parent node is given the 
 * parent node of the currently selected feed or the 
 * selected folder will be used.
 *
 * To be used before calling feedlist_node_added()
 */
void node_set_parent (nodePtr node, nodePtr parent, gint position);

/**
 * node_reparent: (skip)
 * @node:		the node
 * @new_parent: 	nodes new parent
 *
 * Set a node's new parent and update UI. If a node already has a parent, 
 * it will be removed from its parent children list. 
 */ 
void node_reparent (nodePtr node, nodePtr new_parent);

/**
 * node_remove: (skip)
 * @node:		the node
 *
 * Removes all data associated with the given node.
 */
void node_remove (nodePtr node);

/**
 * node_set_data: (skip)
 * @node: 	the node to attach to
 * @data:	the structure
 *
 * Attaches a data structure to the given node.
 */
void node_set_data(nodePtr node, gpointer data);

/**
 * node_set_subscription: (skip)
 * @node:		the node
 * @subscription:	the subscription
 *
 * Attaches the subscription to the given node.
 */
void node_set_subscription (nodePtr node, struct subscription *subscription);

/**
 * node_update_subscription: (skip)
 * @node:		the node
 * @user_data:  	update flags
 *
 * Helper function to be used with node_foreach_child()
 * to mass-update subscriptions.
 */
void node_update_subscription (nodePtr node, gpointer user_data);

/**
 * node_auto_update_subscription: (skip)
 * @node:		the node
 *
 * Helper function to be used with node_foreach_child()
 * to mass-auto-update subscriptions.
 */
void node_auto_update_subscription (nodePtr node);

/**
 * node_reset_update_counter: (skip)
 * @node:		the node
 * @now:		the current timestamp
 *
 * Helper function to be used with node_foreach_child()
 * to mass-auto-update subscriptions.
 */
void node_reset_update_counter (nodePtr node, guint64 *now);

/**
 * node_is_ancestor: (skip)
 * @node1:		the possible ancestor
 * @node2:		the possible child
 *
 * Determines whether node1 is an ancestor of node2
 *
 * Returns: TRUE if node1 is ancestor of node2
 */
gboolean node_is_ancestor(nodePtr node1, nodePtr node2);

/** 
 * node_get_title: (skip)
 * @node:	the node
 *
 * Query the node's title for the feed list.
 *
 * Returns: the title
 */
const gchar * node_get_title(nodePtr node);

/**
 * node_set_title: (skip)
 * @node:	the node
 * @title:	the title
 *
 * Sets the node's title for the feed list.
 */
void node_set_title(nodePtr node, const gchar *title);

/**
 * node_update_counters: (skip)
 * @node:	the node
 *
 * Update the number of items and unread items of a node from
 * the DB. This method ensures propagation to parent folders.
 */
void node_update_counters(nodePtr node);

/**
 * node_mark_all_read: (skip)
 * @node:	the node to process
 *
 * Recursively marks all items of the given node as read.
 */
void node_mark_all_read(nodePtr node);

/**
 * node_set_icon: (skip)
 * @node:		the node
 * @icon: (nullable):	a pixmap or NULL
 *
 * Assigns a new pixmaps as the favicon representing this node.
 */
void node_set_icon(nodePtr node, gpointer icon);

/**
 * node_get_icon: (skip)
 *
 * Returns an appropriate icon for the given node. If the node
 * is unavailable the "unavailable" icon will be returned. If
 * the node is available an existing favicon or the node type
 * specific default icon will be returned.
 *
 * Returns: (nullable): a pixmap or NULL
 */
gpointer node_get_icon (nodePtr node);

/**
 * node_get_large_icon: (skip)
 *
 * Returns a large icon for the node. Does not return any default
 * icons like node_get_icon() does.
 *
 * Returns: (nullable): a pixmap or NULL
 */
gpointer node_get_large_icon (nodePtr node);

/**
 * node_get_favicon_file: (skip)
 * @node:		the node
 *
 * Returns the name of the favicon cache file for the given node.
 * If there is no favicon a default icon file name will be returned.
 *
 * Returns: a file name
 */
const gchar * node_get_favicon_file(nodePtr node);

/**
 * node_new_id:
 *
 * Returns a new unused unique node id.
 *
 * Returns: (transfer full): new id (to be free'd using g_free)
 */
gchar * node_new_id (void);

/**
 * node_get_id: (skip)
 * @node:	the node
 *
 * Query the unique id string of the node.
 *
 * Returns: id string
 */
const gchar *node_get_id (nodePtr node);

/** 
 * node_set_id: (skip)
 * @node:	the node
 * @id: 	the id string
 *
 * Set the unique id string of the node.
 */
void node_set_id(nodePtr node, const gchar *id);

/** 
 * node_free: (skip)
 * @node: the node to free
 *
 * Frees a given node structure.
 */
void node_free(nodePtr node);

/**
 * node_default_render: (skip)
 * @node:		the node to render
 *
 * Helper function for generic node rendering. Performs
 * a generic node serialization to XML and passes the
 * generated XML source document to the XSLT stylesheet
 * with the same name as the node type id.
 *
 * Returns: XHTML string
 */
gchar * node_default_render(nodePtr node);

/**
 * node_save: (skip)
 * @node:	the node
 *
 * Saves the given node to cache.
 */
void node_save(nodePtr node);

/**
 * node_get_itemset: (skip)
 * @node:	the node
 *
 * Loads all items of the given node into memory.
 * The caller needs to free the item set using itemset_free()
 *
 * Returns: the item set
 */
itemSetPtr node_get_itemset(nodePtr node);

/**
 * node_render: (skip)
 * @node:	the node
 *
 * Node content rendering
 *
 * Returns: string with node rendered in HTML
 */
gchar * node_render(nodePtr node);

/**
 * node_update_favicon: (skip)
 * @node:		the node
 *
 * Called when updating favicons is requested.
 */
void node_update_favicon (nodePtr node);

/**
 * node_load_icon: (skip)
 * @node:	the node
 *
 * Load node icon in memory. Should be called only once on startup
 * and when the node icon has changed.
 */
void node_load_icon (nodePtr node);

/**
 * node_set_sort_column: (skip)
 * @node:		the node
 * @sortColumn: 	sort column id
 * @reversed:   	TRUE if order should be reversed
 *
 * Change/Set the sort column of a given node.
 *
 * Returns: TRUE if the passed settings were different from the previous ones
 */
gboolean node_set_sort_column (nodePtr node, nodeViewSortType sortColumn, gboolean reversed);

/**
 * node_get_base_url: (skip)
 * @node:	the node
 *
 * Returns the base URL for the given node.
 * If it is a mixed item set NULL will be returned.
 *
 * Returns: base URL
 */
const gchar * node_get_base_url(nodePtr node);

/**
 * node_can_add_child_feed: (skip)
 * @node:	the node
 *
 * Query whether a feed be added to the given node.
 *
 * Returns: TRUE if a feed can be added
 */
gboolean node_can_add_child_feed (nodePtr node);

/**
 * node_can_add_child_folder: (skip)
 * @node:	the node
 *
 * Query whether a folder be added to the given node.
 *
 * Returns: TRUE if a folder can be added
 */
gboolean node_can_add_child_folder (nodePtr node);

/**
 * node_save_items_to_file: (skip)
 * @node:	the node
 * @filename:	the destination file name
 * @error:	(nullable): a GError that will receive error information on failure
 *
 * Exports all items in this node as a RSS2 feed.
 */
void node_save_items_to_file(nodePtr node, const gchar *filename, GError **error);


/* child nodes iterating interface */

typedef void 	(*nodeActionFunc)	(nodePtr node);
typedef void 	(*nodeActionDataFunc)	(nodePtr node, gpointer user_data);

/*
 * Do not call this method directly! Do use
 * node_foreach_child() or node_foreach_child_data()!
 */
void node_foreach_child_full(nodePtr ptr, gpointer func, gint params, gpointer user_data);

/**
 * node_foreach_child: (skip)
 * @node:	node pointer whose children should be processed
 * @func:	the function to process all found elements
 *
 * Helper function to call node methods for all
 * children of a given node. The given function may
 * modify the children list.
 */
#define node_foreach_child(node, func) node_foreach_child_full(node,func,0,NULL)

/**
 * node_foreach_child_data: (skip)
 * @node:	node pointer whose children should be processed
 * @func:	the function to process all found elements
 * @user_data:  specifies the second argument that func should be passed
 *
 * Helper function to call node methods for all
 * children of a given node. The given function may 
 * modify the children list.
 */
#define node_foreach_child_data(node, func, user_data) node_foreach_child_full(node,func,1,user_data)

#endif
