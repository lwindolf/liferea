/**
 * @file itemview.h    item display interface abstraction
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _ITEMVIEW_H
#define _ITEMVIEW_H

#include <gtk/gtk.h>
#include "item.h"
#include "itemset.h"
#include "node.h"
 
 /* Liferea knows two ways to present items: with a GTK
    tree view and with a HTML rendering widget. This 
    interface generalizes item adding, removing and 
    updating for these two presentation types. */

/**
 * Initial setup of the item view.
 */
void	itemview_init(void);

/** 
 * Removes all currently loaded items from the item view.
 */
void	itemview_clear(void);
    
/**
 * Prepares the view for displaying items of the given node.
 *
 * @param node	the node whose items are to be presented
 */
void	itemview_set_displayed_node(nodePtr node);

/** item view display mode type */
typedef enum {
	ITEMVIEW_SINGLE_ITEM,	/**< 3 panes, item view shows the selected item only in HTML view */
	ITEMVIEW_LOAD_LINK,	/**< 3 panes, item view loads the link of selected item into HTML view */
	ITEMVIEW_ALL_ITEMS,	/**< 2 panes, item view shows all items combined in HTML view */
	ITEMVIEW_NODE_INFO	/**< 3 panes, item view shows the selected node description in HTML view*/
} itemViewMode;

/**
 * Set/unset the display mode of the item view.
 *
 * @param mode		item view mode constant
 */
void	itemview_set_mode(itemViewMode mode);

/**
 * Adds an item to the view for rendering. The item must belong
 * to the item set that was announced with itemview_set_displayed_node().
 *
 * @param item		the item to add
 */
void	itemview_add_item(itemPtr item);

/**
 * Removes a given item from the view.
 *
 * @param item	the item to remove
 */
void	itemview_remove_item(itemPtr item);

/**
 * Selects a given item in the view. The item must be
 * added using itemview_add_item before selecting.
 *
 * @param item	the item to select
 */
void	itemview_select_item(itemPtr item);

/**
 * Requests updating the rendering of a given item.
 *
 * @param item	the item to update
 */
void	itemview_update_item(itemPtr item);

/**
 * Requests updating the rendering of a all displayed items.
 */
void	itemview_update_all_items(void);

/**
 * Requests updating the rendering of the node info view.
 *
 * @node node	the node whose info view is to be updated
 */
void	itemview_update_node_info(struct node *node);

/**
 * Refreshes the item view. Needs to be called after each
 * add, remove or update of one or more items.
 */
void	itemview_update(void);

/**
 * Generic date formatting function. Uses either the 
 * nice formatting method to print age dependant date
 * strings or the user defined format string.
 *
 * @param date	a date to represent
 *
 * @returns newly allocated date string
 */
gchar *	itemview_format_date(time_t date);

#endif
