/**
 * @file vfolder.h VFolder functionality
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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
#include "item.h"
#include "rule.h"

/* The vfolder implementation of Liferea is similar to the
   one in Evolution. Vfolders are effectivly permanent searches.

   Each vfolder instance is a set of rules applied to all items
   of all other feeds (excluding other vfolders). Each vfolder
   instance can have a single node in the feed list. 
   
   The vfolder concept also constitutes an own itemset type
   with special update propagation and removal handling. */

typedef struct vfolder {
	gchar		*title;			/**< vfolder title */
	GSList		*rules;			/**< list of rules if this is a vfolder */
	struct node	*node;			/**< the node of the vfolder, needed for merging */
} *vfolderPtr;

/**
 * Initialize vfolder handling.
 */
void	vfolder_init(void);

/**
 * Sets up a new vfolder structure.
 *
 * @returns a new vfolder
 */
vfolderPtr vfolder_new(void);

/**
 * Vfolder specific feed list import parsing.
 *
 * @param np	the node to import
 * @param cur	DOM node to parse
 * @returns pointer to resulting plugin
 */
gpointer vfolder_import(struct node *np, xmlNodePtr cur);

/**
 * Vfolder specific feed list export.
 *
 * @param np	the node to export
 * @param cur	DOM node to write to
 */
void vfolder_export(vfolderPtr vp, xmlNodePtr cur);

/* set/get of vfolder title */
const gchar * vfolder_get_title(vfolderPtr vp);
void vfolder_set_title(vfolderPtr vp, const gchar * title);

/**
 * Method thats adds a rule to a vfolder. To be used
 * on loading time or when creating searching. Does 
 * not process items. Just sets up the vfolder.
 *  
 * @param fp		vfolder the rule belongs to
 * @param ruleId	id string for this rule type
 * @param value		argument string for this rule
 * @param additive	indicates positive or negative logic
 */
void	vfolder_add_rule(vfolderPtr vp, const gchar *ruleId, const gchar *value, gboolean additive);

/** 
 * Method that removes a rule from a vfolder. To be used
 * when deleting or changing vfolders. Does not process
 * items. 
 *
 * @param vp	vfolder
 * @param rp	rule to remove
 */
void	vfolder_remove_rule(vfolderPtr vp, rulePtr rp);

/**
 * Method that applies the rules of the given vfolder to 
 * all existing items. To be used for creating search
 * results or new vfolders. Not to be used when loading
 * vfolders from cache. 
 *
 * @param vp	vfolder
 */
void	vfolder_refresh(vfolderPtr vp);

/**
 * Method to be called when a item was updated. This maybe
 * after user interaction or updated item contents 
 *
 * @param ip	item of a feed to check
 */
void	vfolder_update_item(itemPtr ip);

/**
 * Method to be called when a new item needs to be checked
 * against all vfolder rules. To be used upon feed list loading
 * and when new items are downloaded.
 *
 * This method may also be used to recheck an vfolder item
 * copy again. This may remove the item copy if it does not
 * longer match the vfolder rules.
 *
 * @param item	node item to check
 * @returns TRUE if item (still) added, FALSE otherwise
 */
gboolean vfolder_check_item(itemPtr item);

/**
 * Method to be called during initial node loading to check
 * all items of a single node for vfolder rule macthing.
 *
 * @param node	the node whose items are to be checked
 */
void vfolder_check_node(struct node *node);

/** 
 * Searches all vfolders for copies of the given item and
 * removes them. Used for item remove propagation. If a
 * vfolder item copy is passed it is removed directly.
 *
 * @param ip	feed item or vfolder item copy to remove
 */
void	vfolder_remove_item(itemPtr ip);

/**
 * Called when a vfolder is processed by feed_free
 * to get rid of the vfolder items.
 *
 * @param vp	vfolder to free
 */
void	vfolder_free(vfolderPtr vp);

/* implementation of the node type interface */
struct nodeType * vfolder_get_node_type(void);

#endif
