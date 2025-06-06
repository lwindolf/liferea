/*
 * item_list_view.h  presenting items in a GtkTreeView
 *
 * Copyright (C) 2004-2025 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _ITEM_LIST_VIEW_H
#define _ITEM_LIST_VIEW_H

#include <gtk/gtk.h>

#include "feedlist.h"
#include "item.h"
#include "itemlist.h"
#include "node_view.h"

/* This class realizes an GtkTreeView based item view. Instances
   of ItemListView are managed by the ItemListView. This class hides
   the GtkTreeView and implements performance optimizations. */

G_BEGIN_DECLS

#define ITEM_LIST_VIEW_TYPE (item_list_view_get_type ())
G_DECLARE_FINAL_TYPE (ItemListView, item_list_view, ITEM_LIST, VIEW, GObject)

/**
 * item_list_view_create: (skip)
 * @feedlist:	the FeedList
 * @itemlist:	the ItemList
 * @wide:	TRUE if ItemListView should be optimized for wide view
 *
 * Create a new ItemListView instance.
 *
 * Returns: (transfer none):	the ItemListView instance
 */
ItemListView * item_list_view_create (FeedList *feedlist, ItemList *itemlist, gboolean wide);

/**
 * item_list_view_get_widget:
 *
 * Returns the GtkWidget used by the ItemListView instance.
 *
 * Returns: (transfer none): a GtkWidget
 */
GtkWidget * item_list_view_get_widget (ItemListView *ilv);

/**
 * item_list_view_move_cursor:
 * @ilv: 	the ItemListView
 * @step:	move distance
 *
 * Moves the cursor in the item list step times.
 * Negative value means moving backwards.
 */
void item_list_view_move_cursor (ItemListView *ilv, int step);

/**
 * item_list_view_move_cursor_to_first:
 * @ilv: 	the ItemListView
 *
 * Moves the cursor to the first element.
 */
void item_list_view_move_cursor_to_first (ItemListView *ilv);

/**
 * item_list_view_contains_id:
 * @ilv:	the ItemListView
 * @id: 	the item id
 *
 * Checks whether the given id is in the ItemListView.
 *
 * Returns: TRUE if the item is in the ItemListView
 */
gboolean item_list_view_contains_id (ItemListView *ilv, gulong id);

/**
 * item_list_view_set_sort_column:
 * @ilv:		the ItemListView
 * @sortType:   	new sort type
 * @sortReversed:	TRUE for ascending order
 *
 * Changes the sorting type (and direction).
 */
void item_list_view_set_sort_column (ItemListView *ilv, nodeViewSortType sortType, gboolean sortReversed);

/**
 * item_list_view_select: (skip)
 * @ilv:	the ItemListView
 * @item:	the item to select
 *
 * Selects the given item (if it is in the ItemListView).
 */
void item_list_view_select (ItemListView *ilv, itemPtr item);

/**
 * item_list_view_remove_item: (skip)
 * @ilv:	the ItemListView
 * @item:	the item to remove
 *
 * Remove an item from an ItemListView. This method is expensive
 * and is to be used only for items removed by background updates
 * (usually cache drops).
 */
void item_list_view_remove_item (ItemListView *ilv, itemPtr item);

/**
 * item_list_view_enable_favicon:
 * @ilv:		the ItemListView
 * @enabled:    	TRUE if column is to be visible
 *
 * Enable the favicon column of the currently displayed itemlist.
 */
void item_list_view_enable_favicon_column (ItemListView *ilv, gboolean enabled);

/**
 * item_list_view_update_item: (skip)
 * @ilv:	the ItemListView
 * @item:	the item
 *
 * Update a single item of a ItemListView
 */
void item_list_view_update_item (ItemListView *ilv, itemPtr item);

/**
 * item_list_view_find_unread_item: (skip)
 * @ilv:               the ItemListView
 * @startId:           0 or the item id to start from
 *
 * Finds and selects the next unread item starting at the given
 * item in a ItemListView according to the current GtkTreeView sorting order.
 *
 * Returns: (nullable): unread item (or NULL)
 */
itemPtr item_list_view_find_unread_item (ItemListView *ilv, gulong startId);

#endif
