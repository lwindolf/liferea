/*
 * @file itemview.h  viewing feed content in different presentation modes
 *
 * Copyright (C) 2006-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "item.h"
#include "itemset.h"
#include "node.h"
#include "ui/liferea_browser.h"

/* Liferea presents items in a dynamic view. The view layout
   changes according to the subscription preferences and if
   the user requests it on-the-fly. Also the view contents
   are refreshed automatically.

   The view consist of an optional GtkTreeView presenting
   the list of the relevant items and a HTML widget rendering
   a feed info, a single item or multiple items at once. */

G_BEGIN_DECLS

#define ITEM_VIEW_TYPE (itemview_get_type ())
G_DECLARE_FINAL_TYPE (ItemView, itemview, ITEM, VIEW, GObject)

/**
 * itemview_clear: (skip)
 *
 * Removes all currently loaded items from the item view.
 */
void itemview_clear (void);

/**
 * itemview_set_displayed_node: (skip)
 * @node:	the node whose items are to be presented
 *
 * Prepares the view for displaying items of the given node.
 */
void itemview_set_displayed_node (nodePtr node);

/* item view display mode type */
typedef enum {
	ITEMVIEW_SINGLE_ITEM,	/*<< 3 panes, item view shows the selected item only in HTML view */
	ITEMVIEW_NODE_INFO	/*<< 3 panes, item view shows the selected node description in HTML view*/
} itemViewMode;

/**
 * itemview_set_mode:
 * @mode:		item view mode constant
 *
 * Set/unset the display mode of the item view.
 */
void itemview_set_mode (itemViewMode mode);

/**
 * itemview_add_item: (skip)
 * @item:		the item to add
 *
 * Adds an item to the view for rendering. The item must belong
 * to the item set that was announced with itemview_set_displayed_node().
 *
 * TODO: use item merger signal instead
 */
void itemview_add_item (itemPtr item);

/**
 * itemview_remove_item: (skip)
 * @item:	the item to remove
 *
 * Removes a given item from the view.
 *
 * TODO: use item merger signal instead
 */
void itemview_remove_item (itemPtr item);

/**
 * itemview_select_item: (skip)
 * @item: the item to select
 *
 * Selects a given item in the view. The item must be
 * added using itemview_add_item before selecting.
 */
void itemview_select_item (itemPtr item);

/**
 * itemview_select_enclosure:
 * @position:	the position to select
 *
 * Selects the nth enclosure in the enclosure list view currently presented.
 */
void itemview_select_enclosure (guint position);

/**
 * itemview_open_next_enclosure:
 * @view: The ItemView
 *
 * Selects and open the next enclosure in the list.
 */
void itemview_open_next_enclosure (ItemView *view);

/**
 * itemview_update_item: (skip)
 * @item:	the item to update
 *
 * Requests updating the rendering of a given item.
 */
void itemview_update_item (itemPtr item);

/**
 * itemview_update_all_items:
 *
 * Requests updating the rendering of a all displayed items.
 */
void itemview_update_all_items (void);

/**
 * itemview_update_node_info: (skip)
 * @node:	the node whose info view is to be updated
 *
 * Requests updating the rendering of the node info view.
 *
 * TODO: register for signal at feed merger instead
 */
void itemview_update_node_info (struct node *node);

/**
 * itemview_update: (skip)
 *
 * Refreshes the item view. Needs to be called after each
 * add, remove or update of one or more items.
 *
 * TODO: register for signal at item merger instead
 */
void itemview_update (void);

/**
 * itemview_find_unread_item: (skip)
 * @startId:	the item id to start at (or NULL for starting at the top)
 *
 * Finds the next unread item.
 *
 * Returns: (transfer none): the item found (or NULL)
 */
itemPtr itemview_find_unread_item (gulong startId);

/**
 * itemview_scroll:
 *
 * Paging/skimming the item view. If possible scrolls
 * down otherwise it triggers Next-Unread.
 */
void itemview_scroll (void);

/**
 * itemview_move_cursor:
 * @step:	moving steps
 *
 * Moves the cursor in the item list step times.
 * Negative value means moving backwards.
 */
void itemview_move_cursor (int step);

/**
 * itemview_move_cursor_to_first:
 *
 * Moves the cursor in the item list to the first element.
 */
void itemview_move_cursor_to_first (void);

/**
 * itemview_set_layout:
 * @newMode:	new view mode (NODE_VIEW_MODE_*)
 *
 * Switches the layout for the given viewing mode.
 */
void itemview_set_layout (nodeViewType newMode);

/**
 * itemview_get_layout:
 *
 * Returns the viewing mode property of the currently displayed item set.
 *
 * Returns: viewing mode (0 = normal, 1 = wide, 2 = auto)
 */
guint itemview_get_layout (void);

/**
 * itemview_create: (skip)
 * @window:		parent window widget
 *
 * Creates the item view singleton instance.
 *
 * Returns: (transfer none):	the item view instance
 */
ItemView * itemview_create (GtkWidget *window);

/**
 * itemview_launch_URL:
 * @url:	        the link to load
 * @internal:	TRUE if internal browsing is to be enforced
 *
 * Launch the given URL in the currently active HTML view.
 *
 */
void itemview_launch_URL (const gchar *url, gboolean internal);

/**
 * itemview_do_zoom:
 * @zoom:	1 for zoom in, -1 for zoom out, 0 for reset
 *
 * Requests the item view to change zoom level.
 */
void itemview_do_zoom (gint zoom);

/**
 * itemview_style_update:
 *
 * Invokes a change of the href attribute in WebView's <link> tag
 */
void itemview_style_update (void);

G_END_DECLS

#endif
