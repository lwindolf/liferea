/*
 * @file itemview.h  viewing feed content in different presentation modes
 * 
 * Copyright (C) 2006-2012 Lars Windolf <lars.windolf@gmx.de>
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
#include "ui/liferea_htmlview.h"

/* Liferea presents items in a dynamic view. The view layout
   changes according to the subscription preferences and if
   the user requests it on-the-fly. Also the view contents
   are refreshed automatically.
   
   The view consist of an optional GtkTreeView presenting
   the list of the relevant items and a HTML widget rendering
   a feed info, a single item or multiple items at once. */

G_BEGIN_DECLS

#define ITEMVIEW_TYPE		(itemview_get_type ())
#define ITEMVIEW(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), ITEMVIEW_TYPE, ItemView))
#define ITEMVIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), ITEMVIEW_TYPE, ItemViewClass))
#define IS_ITEMVIEW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), ITEMVIEW_TYPE))
#define IS_ITEMVIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), ITEMVIEW_TYPE))

typedef struct ItemView		ItemView;
typedef struct ItemViewClass	ItemViewClass;
typedef struct ItemViewPrivate	ItemViewPrivate;

struct ItemView
{
	GObject		parent;
	
	/*< private >*/
	ItemViewPrivate	*priv;
};

struct ItemViewClass 
{
	GObjectClass parent_class;	
};

GType itemview_get_type (void);

/** 
 * itemview_clear:
 *
 * Removes all currently loaded items from the item view.
 */
void itemview_clear (void);
    
/**
 * itemview_set_displayed_node:
 *
 * Prepares the view for displaying items of the given node.
 *
 * @param node	the node whose items are to be presented
 */
void itemview_set_displayed_node (nodePtr node);

/** item view display mode type */
typedef enum {
	ITEMVIEW_SINGLE_ITEM,	/**< 3 panes, item view shows the selected item only in HTML view */
	ITEMVIEW_ALL_ITEMS,	/**< 2 panes, item view shows all items combined in HTML view */
	ITEMVIEW_NODE_INFO	/**< 3 panes, item view shows the selected node description in HTML view*/
} itemViewMode;

/**
 * itemview_set_mode:
 *
 * Set/unset the display mode of the item view.
 *
 * @param mode		item view mode constant
 */
void itemview_set_mode (itemViewMode mode);

/**
 * itemview_add_item:
 *
 * Adds an item to the view for rendering. The item must belong
 * to the item set that was announced with itemview_set_displayed_node().
 *
 * @param item		the item to add
 *
 * TODO: use item merger signal instead
 */
void itemview_add_item (itemPtr item);

/**
 * itemview_remove_item:
 *
 * Removes a given item from the view.
 *
 * @param item	the item to remove
 *
 * TODO: use item merger signal instead
 */
void itemview_remove_item (itemPtr item);

/**
 * itemview_select_item:
 *
 * Selects a given item in the view. The item must be
 * added using itemview_add_item before selecting.
 *
 * @item: the item to select
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
 * itemview_update_item:
 *
 * Requests updating the rendering of a given item.
 *
 * @item:	the item to update
 */
void itemview_update_item (itemPtr item);

/**
 * itemview_update_all_items:
 *
 * Requests updating the rendering of a all displayed items.
 */
void itemview_update_all_items (void);

/**
 * itemview_update_node_info:
 *
 * Requests updating the rendering of the node info view.
 *
 * @node:	the node whose info view is to be updated
 *
 * TODO: register for signal at feed merger instead
 */
void itemview_update_node_info (struct node *node);

/**
 * itemview_update:
 *
 * Refreshes the item view. Needs to be called after each
 * add, remove or update of one or more items.
 *
 * TODO: register for signal at item merger instead
 */
void itemview_update (void);

/**
 * itemview_display_info:
 *
 * Sets an info display in the item view HTML widget.
 * Used for special functionality like search result info.
 *
 * @html:	HTML to present
 */
void itemview_display_info (const gchar *html);

/**
 * itemview_find_unread_item: (skip)
 *
 * Finds the next unread item.
 *
 * @startId:	the item id to start at (or NULL for starting at the top)
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
 *
 * Moves the cursor in the item list step times.
 * Negative value means moving backwards.
 * 
 * @step:	moving steps
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
 *
 * Switches the layout for the given viewing mode.
 *
 * @newMode:	new view mode (NODE_VIEW_MODE_*)
 */
void itemview_set_layout (nodeViewType newMode);

/**
 * itemview_create:
 * @window:		parent window widget
 *
 * Creates the item view singleton instance.
 *
 * Returns: (transfer none):	the item view instance
 */
ItemView * itemview_create (GtkWidget *window);

/**
 * itemview_launch_URL:
 *
 * Launch the given URL in the currently active HTML view.
 *
 * @param url		the link to load
 * @param forceInternal	TRUE if internal browsing is to be enforced
 */
void itemview_launch_URL (const gchar *url, gboolean internal);

/**
 * itemview_do_zoom:
 *
 * Requests the item view to change zoom level.
 *
 * @param in	TRUE if zooming in, FALSE for zooming out
 */
void itemview_do_zoom (gboolean in);

G_END_DECLS

#endif
