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
#include "feed.h"
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
struct node * itemlist_get_displayed_node(void);

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
 * @param sp	the item set to be merged
 */
void itemlist_merge_itemset(itemSetPtr sp);

/** 
 * Loads the passed feeds items into the itemlist.
 *
 * @param sp 	the item set to be loaded
 */
void itemlist_load(itemSetPtr sp);

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
 * Changes the 2/3-pane mode setting of the displayed
 * item set and its node. Uses itemlist_set_two_pane_mode().
 *
 * @param newMode	TRUE for 2-pane
 */
void itemlist_change_two_pane_mode(gboolean newMode);

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

void itemlist_update_item(itemPtr ip);

void itemlist_remove_item(itemPtr ip);

/**
 * To be called whenever the user wants to remove 
 * all items of a node.
 *
 * @param np	the node which items should be removed
 */
void itemlist_remove_items(itemSetPtr sp);

/**
 * Toggle the flag of the item currently selected in the itemlist
 */
void itemlist_toggle_flag(itemPtr ip);

void itemlist_toggle_read_status(itemPtr ip);

void itemlist_set_read_status(itemPtr ip, gboolean newStatus);

/**
 * Unsets bot the unread and update flag for all items
 * of the given feed.
 */
void itemlist_mark_all_read(itemSetPtr sp);

void itemlist_update_vfolder(vfolderPtr vp);

/**
 * Called from GUI when item list selection changes.
 *
 * @param ip	new selected item 
 */
void itemlist_selection_changed(itemPtr ip);

/** Force the itemlist to re-create the displayed dates based on the
 *  current date format setting 
 */
void itemlist_reset_date_format(void);

#endif
