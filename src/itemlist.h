/**
 * @file itemlist.h itemlist handling
 *
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _ITEMLIST_H
#define _ITEMLIST_H

#include <gtk/gtk.h>
#include "item.h"
#include "itemset.h"
#include "vfolder.h"

/* This is a simple controller implementation for itemlist handling. 
   It manages the currently displayed itemset and provides synchronisation
   for backend and GUI access to this itemset.  
   
   Bypass only for read-only item access! */

/**
 * Returns the currently displayed node.
 *
 * @returns displayed node (or NULL)
 */
// FIXME: drop me in favour of itemlist_get_displayed_itemset()
struct node * itemlist_get_displayed_node(void);

/**
 * Returns the currently displayed item set.
 *
 * @returns displayed item set (or NULL)
 */
struct itemSet * itemlist_get_displayed_itemset(void);

/**
 * Returns the currently selected item.
 * Note: selected = displayed item
 *
 * @returns displayed item (or NULL)
 */
itemPtr itemlist_get_selected(void);

/**
 * To be called whenever a feed was updated. If it is a somehow
 * displayed feed it is loaded this method decides if the
 * and how the item list GUI needs to be updated.
 *
 * @param itemSet	the item set to be merged
 */
void itemlist_merge_itemset(itemSetPtr itemSet);

/** 
 * Loads the passed feeds items into the itemlist.
 *
 * @param itemSet 	the item set to be loaded
 */
void itemlist_load(itemSetPtr itemSet);

/**
 * Clears the item list. Unsets the currently
 * displayed item set. Optionally marks every item read.
 *
 * @param markRead	if TRUE all items are marked as read
 */
void itemlist_unload(gboolean markRead);

/**
 * Changes the 2/3-pane mode property of the item list.
 * Do not use this method to change the 2/3-pane mode
 * of a displayed node!
 *
 * @param newMode	TRUE for 2-pane
 */
void itemlist_set_two_pane_mode(gboolean newMode);

/**
 * Returns the two/three pane mode property of the
 * currently displayed item set.
 *
 * @returns TRUE for two pane
 */
gboolean itemlist_get_two_pane_mode(void);

/**
 * Menu callback that toggles the two pane mode
 *
 * @param menuitem	the clicked menu item
 * @param user_data	unused
 */
void on_toggle_condensed_view_activate(GtkToggleAction *menuitem, gpointer user_data);

/* item handling functions */

void itemlist_update_item(itemPtr item);

void itemlist_request_remove_item(itemPtr item);

/**
 * To be called whenever the user wants to remove
 * a single item. If necessary the item will be unselected.
 * The item will be removed immediately.
 *
 * @param item	the item
 */
void itemlist_remove_item(itemPtr item);

/**
 * To be called whenever the user wants to remove 
 * all items of a node. Item list selection will be
 * resetted. All items are removed immediately.
 *
 * @param itemSet	the item set whose items should be removed
 */
void itemlist_remove_items(itemSetPtr itemSet);

/**
 * Marks all items of the item set as read.
 *
 * @param itemSet	the item set to be marked read
 */
void itemlist_mark_all_read(itemSetPtr itemSet);

/**
 * Resets the new flag for all items of the given item set.
 *
 * @param itemSet	the itemset
 */
void itemlist_mark_all_old(itemSetPtr itemSet);

void itemlist_update_vfolder(vfolderPtr vfolder);

/**
 * Called from GUI when item list selection changes.
 *
 * @param item	new selected item 
 */
void itemlist_selection_changed(itemPtr item);

/** 
 * Force the itemlist to re-create the displayed dates based on the
 * current date format setting.
 */
void itemlist_reset_date_format(void);

/**
 * Tries to select the next unread item that is currently in the
 * item list. Or does nothing if there are no unread items left.
 */
void itemlist_select_next_unread(void);

/**
 * Sets the flag status of the given item.
 *
 * @param item		the item
 * @param newStatus	new flag status
 */
void itemlist_set_flag(itemPtr item, gboolean newStatus);

/**
 * Sets the read status of the given item.
 *
 * @param item		the item
 * @param newStatus	new read status
 */
void itemlist_set_read_status(itemPtr item, gboolean newStatus);

/**
 * Sets the update status of the given item.
 *
 * @param item		the item
 * @param newStatus	new update status
 */
void itemlist_set_update_status(itemPtr item, const gboolean newStatus);

/**
 * Toggle the flag of the given item.
 *
 * @param item		the item
 */
void itemlist_toggle_flag(itemPtr item);

/**
 * Toggle the read status of the given item.
 *
 * @param item		the item
 */
void itemlist_toggle_read_status(itemPtr item);

#endif
