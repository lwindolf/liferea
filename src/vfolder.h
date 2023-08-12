/**
 * @file vfolder.h  search folder node type
 *
 * Copyright (C) 2003-2022 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _VFOLDER_H
#define _VFOLDER_H

#include <glib.h>

#include "itemset.h"
#include "node_type.h"

/* The search folder implementation of Liferea is similar to the
   one in Evolution. Search folders are effectivly permanent searches.

   As Liferea realizes filtered lists of items using rule based itemsets,
   search folders are effectively persistent rule based itemsets.

   GUI wise a search folder is a type of node in the subscription list.
*/

/** search folder data structure */
typedef struct vfolder {
	struct node	*node;		/**< the feed list node of this search folder */

	itemSetPtr	itemset;	/**< the itemset with the rules and matching items */
	gpointer	loader;		/**< vfolder loader (if currently rebuilding, otherwise NULL) */

	gboolean	unreadOnly;	/**< TRUE if only unread items are to be shown in the item list */
	gboolean	reloading;	/**< TRUE if the search folder is in async reloading */
	gulong		loadOffset;	/**< when in reloading: current offset */
} *vfolderPtr;

/**
 * Sets up a new search folder structure.
 *
 * @param node 	the feed list node of the search folder
 *
 * @returns a new search folder structure
 */
vfolderPtr vfolder_new (struct node *node);

/**
 * Method to unconditionally invoke an node callback for all search folders.
 *
 * @param func		callback
 */
void vfolder_foreach (nodeActionFunc func);

typedef void 	(*vfolderActionDataFunc)	(vfolderPtr vfolder, itemPtr item);

/**
 * Returns a list of all search folders currently matching the given item.
 *
 * @param item		the item
 *
 * @returns a list of vfolderPtr (to be free'd using g_slist_free())
 */
GSList * vfolder_get_all_with_item_id (itemPtr item);

/**
 * Returns a list of all search folders currently not matching the given item.
 *
 * @param item		the item
 *
 * @returns a list of vfolderPtr (to be free'd using g_slist_free())
 */
GSList * vfolder_get_all_without_item_id (itemPtr item);

/**
 * Resets vfolder state. Drops all items from it.
 * To be called after vfolder_(add|remove)_rule().
 *
 * @param vfolder	search folder to reset
 */
void vfolder_reset (vfolderPtr vfolder);

/**
 * Rebuilds a search folder by scanning all existing items.
 *
 * @param vfolder	search folder to rebuild
 */
void vfolder_rebuild (nodePtr node);

/* implementation of the node type interface */

#define IS_VFOLDER(node) (node->type == vfolder_get_node_type ())

/**
 * Returns the node type implementation for search folder nodes.
 */
nodeTypePtr vfolder_get_node_type (void);

#endif
