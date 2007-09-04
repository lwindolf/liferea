/**
 * @file htmlview.h implementation of the item view interface for HTML rendering
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
 
#ifndef _HTMLVIEW_H
#define _HTMLVIEW_H

#include "item.h"
#include "itemview.h"
#include "node.h"
#include "ui/ui_htmlview.h"

/* interface for item and item set HTML rendering */

/**
 * Initialize the HTML view 
 */
void	htmlview_init (void);

/**
 * To be called to clear the HTML view 
 */
void	htmlview_clear (void);

/**
 * Prepares the HTML view for displaying items of the given node.
 *
 * @param node	the node whose items will be rendered
 */
void	htmlview_set_displayed_node (nodePtr node);

/**
 * Adds an item to the HTML view for rendering. The item must belong
 * to the item set that was announced with htmlview_set_displayed_node().
 *
 * This method _DOES NOT_ update the rendering output.
 *
 * @param item		the item to add to the rendering output
 */
void	htmlview_add_item (itemPtr item);

/**
 * Removes a given item from the HTML view rendering.
 *
 * This method _DOES NOT_ update the rendering output.
 *
 * @param item		the item to remove from the rendering output
 */
void	htmlview_remove_item (itemPtr item);

/**
 * Updates the output of the selected item in the HTML view rendering.
 *
 * This method _DOES NOT_ update the rendering output.
 *
 * @param item		the item to mark for update
 */
void	htmlview_select_item (itemPtr item);

/**
 * Updates the output of a given item in the HTML view rendering.
 *
 * This method _DOES NOT_ update the rendering output.
 *
 * @param item		the item to mark for update
 */
void	htmlview_update_item (itemPtr item);

/**
 * Like htmlview_update_item(), processes all items.
 */
void	htmlview_update_all_items (void);

/**
 * Renders all added items to the given HTML view. To be called
 * after one or more calls of htmlview_(add|remove|update)_item.
 *
 * @param htmlview	HTML view to render to
 * @param mode		item view mode
 */
void	htmlview_update (LifereaHtmlView *htmlview, itemViewMode mode);

/** helper methods for HTML output */

/**
 * Function to add HTML source header to create a valid HTML source.
 *
 * @param buffer	buffer to add the HTML to
 * @param base		base URL of HTML content
 * @param css		TRUE if CSS definitions are to be added
 * @param script	TRUE if item menu scripts are to be added
 */
void	htmlview_start_output (GString *buffer, const gchar *base, gboolean css, gboolean script);

/**
 * Function to add HTML source footer to create a valid HTML source.
 *
 * @param buffer	buffer to add the HTML to
 */
void	htmlview_finish_output (GString *buffer);

#endif
