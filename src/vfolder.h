/**
 * @file vfolder.h search folder node type
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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
#include "rule.h"

/* The search folder implementation of Liferea is similar to the
   one in Evolution. Search folders are effectivly permanent searches.

   Each search folder instance is a set of rules applied to all items
   of all other feeds (excluding other search folders). Each search
   folder instance can be represented by a single node in the feed list. 
   
   The search folder concept also constitutes an own itemset type
   with special update propagation and removal handling. 
   (FIXME: above still true?) */

/** search vfolder data structure */
typedef struct vfolder {
	GSList		*rules;		/**< list of rules of this search folder */
	struct node	*node;		/**< the feed list node of this search folder (or NULL) */
	gboolean	viewExists;	/**< TRUE if DB view was created */
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
 * Method thats adds a rule to a search folder. To be used
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
 * Method that removes a rule from the rule list of a given
 * search folder. To be used when deleting search folders 
 * or editing its rules.
 *
 * vfolder_refresh() needs to called to update the item matches
 *
 * @param vfolder	search folder
 * @param rule		rule to remove
 */
void vfolder_remove_rule (vfolderPtr vfolder, rulePtr rule);

/**
 * Method to invoke a callback for all search folders that do use
 * the given rule.
 *
 * @param ruleName	the rule type
 * @param func		callback
 */
void vfolder_foreach_with_rule (const gchar *ruleName, nodeActionFunc func);

/**
 * Method to invoke callbacks for all search folders that are
 * affected by a modification of the given item. The given rule
 * name is to be used to select the relevant search folders. 
 * For each affected search folder it is check if the item matches
 * the search folder rules. If yes then positiveFunc is called,
 * otherwise negativeFunc is called. For unaffected search folders
 * neither is called.
 *
 * @param item		the item that changed
 * @param ruleName	the rule type that matches the item change
 * @param positiveFunc	function to call if the search folder rules do match
 * @param negativeFunc	function to call if the search folder rules do not match
 */
void vfolder_foreach_with_item (itemPtr item, const gchar *ruleName, nodeActionFunc positiveFunc, nodeActionFunc negativeFunc);

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

/**
 * Called when a search folder is processed by node_free()
 * to get rid of the search folder items.
 *
 * @param vfolder	search folder to free
 */
void vfolder_free (gpointer data);

/* implementation of the node type interface */
struct nodeType * vfolder_get_node_type (void);

#endif
