/**
 * @file node.h common feed list node handling interface
 * 
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#include "item.h"
#include "itemset.h"
#include "update.h"

/* Liferea's GUI consists of three parts. Feed list, item list
   and HTML view. The feed list is a view of all available
   nodes. The feed list allows nodes of different types.

   According to the node's type the interface propagates
   user interaction to the feed list node type implementation
   and allows the implementation to change the nodes state. */

/** node types (also used for feed list tree store) */
enum node_types {
	FST_INVALID 	= 0,		/**< invalid type */
	FST_FOLDER 	= 1,		/**< the folder type */
	FST_ROOT	= 2,		/**< the feed list root type */

	FST_VFOLDER 	= 9,		/**< special type for VFolders */
	FST_FEED	= 10,		/**< any type of feed */
	FST_PLUGIN	= 11		/**< plugin root node */
};

/** generic feed list node structure */
typedef struct node {
	gpointer		data;		/**< node type specific data structure */
	guint			type;		/**< node type */
	struct flNodeHandler_	*handler;	/**< the feed list plugin and node type instance handling this node */

	/* feed list state properties of this node */
	struct node		*parent;	/**< the parent node (or NULL if at root level) */
	GSList			*children;	/**< ordered list of node children */
	gchar			*id;		/**< unique node identifier string */

	guint			unreadCount;	/**< number of items not yet read */
	guint			popupCount;	/**< number of items to be notified */
	guint			newCount;	/**< number of recently downloaded items */

	gchar			*title;		/**< the label of the node in the feed list */
	gpointer		icon;		/**< pointer to pixmap, if there is a favicon */
	guint			loaded;		/**< counter which is non-zero if items are to be kept in memory */
	gboolean		available;	/**< availability of this node (usually the last downloading state) */
	gboolean		needsCacheSave;	/**< flag set when the feed's cache needs to be resaved */

	/* item list state properties of this node */
	itemSetPtr	itemSet;	/**< The set of items belonging to this node */
	gboolean	twoPane;	/**< Flag if three pane or condensed mode is set for this feed */
	gint		sortColumn;	/**< Sorting column. Set to either IS_TITLE, or IS_TIME */
	gboolean	sortReversed;	/**< Sort in the reverse order? */

} *nodePtr;

/** node type interface */
typedef struct nodeTypeInfo {
	/* For method documentation see the wrappers defined below! */
	void	(*initial_load)		(nodePtr node);
	void	(*load)			(nodePtr node);
	void 	(*save)			(nodePtr node);
	void	(*unload)		(nodePtr node);
	void	(*reset_update_counter)	(nodePtr node);
	void	(*request_update)	(nodePtr node, guint flags);
	void 	(*request_auto_update)	(nodePtr node);
	void	(*schedule_update)	(nodePtr node, guint flags);
	void	(*remove)		(nodePtr node);
	void 	(*mark_all_read)	(nodePtr node);
	void 	(*render)		(nodePtr node);
} *nodeTypeInfo;

#define NODE(node)	((flNodeHandler *)(node->handler))->nodeType

/**
 * Registers a new node type. Can be used by feed list
 * plugins to register own node types.
 *
 * @param info	node type info 
 * @param type	node type constant
 */
void node_register_type(nodeTypeInfo *info, guint type);
 
/**
 * Creates a new node structure.
 *
 * @returns the new node
 */
nodePtr node_new(void);

/**
 * Interactive node adding (e.g. feed menu->new subscription), 
 * launches some dialog that upon success calls 
 * node_request_automatic_add() to add feeds or an own
 * method to add other node types.
 *
 * @param type		the node type
 */
void node_request_interactive_add(guint type);

/**
 * Automatic subscription adding (e.g. URL DnD), creates a new node
 * or reuses the given one and creates a new feed without any user 
 * interaction and finally calls node_add().
 *
 * @param node		NULL or a node to reuse
 * @param source	the subscriptions source URL
 * @param title		NULL or the node title
 * @param filter	NULL or the filter for the subscription
 * @param flags		download request flags
 */
void node_request_automatic_add(nodePtr node, gchar *source, gchar *title, gchar *filter, gint flags);
	
/** 
 * Adds the given node to the feed list to the currently
 * selected folder.
 *
 * @param node		the node to add
 */
void node_add(nodePtr node);

/**
 * Removes the given node from the feed list.
 *
 * @param parent	the node
 */
void node_remove(nodePtr np);

/**
 * Attaches a data structure to the given node.
 *
 * @param np 	the node to attach to
 * @param type	the structure type
 * @param data	the structure
 */
void node_add_data(nodePtr np, guint type, gpointer data);

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
 * @param np	the node
 *
 * @returns the title
 */
const gchar * node_get_title(nodePtr np);

/**
 * Sets the node's title for the feed list.
 *
 * @param np	the node
 * @param title	the title
 */
void node_set_title(nodePtr np, const gchar *title);

/**
 * Query the number of unread items of a node.
 *
 * @param np	the node
 * 
 * @returns the number of unread items
 */
guint node_get_unread_count(nodePtr np);

/**
 * Update the number of unread items of a node.
 * This method ensures propagation to parent
 * folders.
 *
 * @param np	the node
 * @param diff	the difference to the current unread count
 */
void node_update_unread_count(nodePtr np, gint diff);

/**
 * Recursively marks all items of the given node as read.
 *
 * @param np	the node to process
 */
void node_mark_all_read(nodePtr np);

/**
 * Returns a new unique node id.
 *
 * @returns new id
 */
gchar * node_new_id(void);

/**
 * Query the unique id string of the node.
 *
 * @param np	the node
 *
 * @returns id string
 */
const gchar *node_get_id(nodePtr np);

/** 
 * Set the unique id string of the node.
 *
 * @param np	the node
 * @param id 	the id string
 */
void node_set_id(nodePtr np, const gchar *id);

/** 
 * Maps node type to string. For feed nodes
 * it maps to the feed type string.
 *
 * @param np	the node 
 * @returns type string (or NULL if unknown)
 */
const gchar *node_type_to_str(nodePtr np);

/** 
 * Maps node type string to type constant.
 *
 * @param type string	the node type as string
 * @returns node type constant
 */
guint node_str_to_type(const gchar *str);

/** 
 * Frees a given node structure.
 *
 * @param the node to free
 */
void node_free(nodePtr np);

/**
 * Loads the given node from cache.
 *
 * @param np	the node
 */
void node_load(nodePtr np);

/**
 * Saves the given node to cache.
 *
 * @param np	the node
 */
void node_save(nodePtr np);

/**
 * Unload the given node from memory.
 *
 * @param np	the node
 */
void node_unload(nodePtr np);

/**
 * Merges the given item set into the item set of
 * the given node. Used for node updating.
 *
 * @param np	the node
 * @param sp	the item set
 */
void node_merge_items(nodePtr np, GList *items);

/**
 * Returns the item set of the given node.
 *
 * @param np	the node
 */
itemSetPtr node_get_itemset(nodePtr np);

/**
 * Assigns the given item set to the given node.
 *
 * @param np	the node
 * @param sp	the item set
 */
void node_set_itemset(nodePtr np, itemSetPtr sp);

/**
 * Node content rendering
 *
 * @param np	the node
 *
 * @returns string with node rendered in HTML
 */
gchar * node_render(nodePtr np);

/**
 * Node auto-update scheduling (feed list auto update).
 *
 * @param np	the node
 */
void node_request_auto_update(nodePtr np);

/**
 * Immediate node updating (user requested).
 *
 * @param np	the node
 * @param flags	update handling flags
 */
void node_request_update(nodePtr np, guint flags);

/**
 * Called from plugins to issue download requests.
 *
 * @param np		the node
 * @param callback	callback for results processing
 * @param source	the URL/cmd/file to download
 * @param flags		update handling flags
 */
void node_schedule_update(nodePtr np, request_cb callback, guint flags);

/**
 * Called when updating favicons is requested.
 *
 * @param np		the node
 */
void node_update_favicon(nodePtr np);

/**
 * Change/Set the sort column of a given node.
 *
 * @param np		the node
 * @param sortColumn	sort column id
 * @param reversed	TRUE if order should be reversed
 */
void node_set_sort_column(nodePtr np, gint sortColumn, gboolean reversed);

/**
 * Change/Set the 2/3 pane mode of a given node.
 *
 * @param np		the node
 * @param newMode	TRUE for two pane	
 */
void node_set_two_pane_mode(nodePtr np, gboolean newMode);

/**
 * Query the 2/3 pane mode setting of a given mode.
 *
 * @param np 	the node
 *
 * @returns TRUE for two pane
 */
gboolean node_get_two_pane_mode(nodePtr np);

/* child nodes iterating interface */

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
