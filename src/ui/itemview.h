/**
 * @file itemview.h  viewing feed content in different presentation modes
 * 
 * Copyright (C) 2006-2009 Lars Lindner <lars.lindner@gmail.com>
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
#include "ui/enclosure_list_view.h"
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
 * Removes all currently loaded items from the item view.
 */
void itemview_clear (void);
    
/**
 * Prepares the view for displaying items of the given node.
 *
 * @param node	the node whose items are to be presented
 */
void itemview_set_displayed_node (nodePtr node);

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
void itemview_set_mode (itemViewMode mode);

/**
 * Adds an item to the view for rendering. The item must belong
 * to the item set that was announced with itemview_set_displayed_node().
 *
 * @param item		the item to add
 *
 * @todo: use item merger signal instead
 */
void itemview_add_item (itemPtr item);

/**
 * Removes a given item from the view.
 *
 * @param item	the item to remove
 *
 * @todo: use item merger signal instead
 */
void itemview_remove_item (itemPtr item);

/**
 * Selects a given item in the view. The item must be
 * added using itemview_add_item before selecting.
 *
 * @param item	the item to select
 */
void itemview_select_item (itemPtr item);

/**
 * Requests updating the rendering of a given item.
 *
 * @param item	the item to update
 */
void itemview_update_item (itemPtr item);

/**
 * Requests updating the rendering of a all displayed items.
 */
void itemview_update_all_items (void);

/**
 * Requests updating the rendering of the node info view.
 *
 * @node node	the node whose info view is to be updated
 *
 * @todo: register for signal at feed merger instead
 */
void itemview_update_node_info (struct node *node);

/**
 * Refreshes the item view. Needs to be called after each
 * add, remove or update of one or more items.
 *
 * @todo: register for signal at item merger instead
 */
void itemview_update (void);

/**
 * Sets an info display in the item view HTML widget.
 * Used for special functionality like search result info.
 *
 * @param html	HTML to present
 */
void itemview_display_info (const gchar *html);

/**
 * Paging/skimming the item view. If possible scrolls
 * down otherwise it triggers Next-Unread.
 */
void itemview_scroll (void);

/**
 * Moves the cursor in the item list step times.
 * Negative value means moving backwards.
 * 
 * @param step	moving steps
 */
void itemview_move_cursor (int step);

/**
 * Moves the cursor in the item list to the first element.
 */
void itemview_move_cursor_to_first (void);

/**
 * Switches the layout for the given viewing mode.
 *
 * @param newMode	new view mode (NODE_VIEW_MODE_*)
 */
void itemview_set_layout (nodeViewType newMode);

/**
 * Creates the item view singleton instance.
 *
 * @param container	parent widget
 *
 * @returns the item view instance
 */
ItemView * itemview_create (GtkWidget *container);

/**
 * Returns the itemview GtkStyle. To be used by the
 * HTML rendering code to determine theme colors.
 */
GtkStyle * itemview_get_style (void);

/**
 * Launch the given URL in the currently active HTML view.
 *
 * @param url		the link to load
 * @param forceInternal	TRUE if internal browsing is to be enforced
 */
void itemview_launch_URL (const gchar *url, gboolean internal);

/**
 * Requests the item view to change zoom level.
 *
 * @param in	TRUE if zooming in, FALSE for zooming out
 */
void itemview_do_zoom (gboolean in);

G_END_DECLS

#endif
