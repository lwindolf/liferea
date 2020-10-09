/**
 * @file htmlview.h  item view interface for HTML rendering
 *
 * Copyright (C) 2006-2020 Lars Windolf <lars.windolf@gmx.de>
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

#include "ui/itemview.h"
#include "ui/liferea_htmlview.h"

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

/**
 * Invokes an update of the href attribute in WebView's <link> tag
 * 
 * @param htmlview  current HTML view
 */
void    htmlview_update_style_element (LifereaHtmlView *htmlview);

#endif
