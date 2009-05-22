/**
 * @file vfolder.h  search folder node type
 *
 * Copyright (C) 2003-2009 Lars Lindner <lars.lindner@gmail.com>
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
#include "node_type.h"

/* The search folder implementation of Liferea is similar to the
   one in Evolution. Search folders are effectivly permanent searches.

   Each search folder instance is a set of rules applied to all items
   of all other feeds (excluding other search folders). Each search
   folder instance can be represented by a single node in the feed list. 
   The search feature is realized using a temporary search folder.
*/

/** search vfolder data structure */
typedef struct vfolder {
	GSList		*rules;		/**< list of rules of this search folder */
	struct node	*node;		/**< the feed list node of this search folder (or NULL) */
	gboolean	anyMatch;	/**< TRUE means only one of the rules must match for item inclusion */
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
 * Method that creates and adds a rule to a search folder. To be used
 * on loading time, when creating searches or when editing
 * search folder properties.
 *  
 * vfolder_refresh() needs to called to update the item matches
 *
 * @param vfolder	search folder the rule belongs to
 * @param ruleId	id string for this rule type
 * @param value		argument string for this rule
 * @param additive	indicates positive or negative logic
 */
void vfolder_add_rule (vfolderPtr vfolder, const gchar *ruleId, const gchar *value, gboolean additive);

/**
 * Method to unconditionally invoke a callback for all search folders.
 *
 * @param func		callback
 */
void vfolder_foreach (nodeActionFunc func);

/**
 * Method that updates the unread and item count for the given
 * search folder node.
 *
 * @param node		the search folder node
 */
void vfolder_update_counters (nodePtr node);

/**
 * Method that "refreshes" the DB view according
 * to the search folder rules. To be called after
 * vfolder_(add|remove)_rule().
 *
 * @param vfolder	search folder to rebuild
 */
void vfolder_refresh (vfolderPtr vfolder);

/* implementation of the node type interface */

#define IS_VFOLDER(node) (node->type == vfolder_get_node_type ())

/**
 * Returns the node type implementation for search folder nodes.
 */
nodeTypePtr vfolder_get_node_type (void);

#endif
