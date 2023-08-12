/*
 * @file liferea_browser.h  Liferea embedded browser
 *
 * Copyright (C) 2003-2021 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _LIFEREA_BROWSER_H
#define _LIFEREA_BROWSER_H

#include <gtk/gtk.h>

#include "net.h"

G_BEGIN_DECLS

#define LIFEREA_BROWSER_TYPE		(liferea_browser_get_type ())
G_DECLARE_FINAL_TYPE (LifereaBrowser, liferea_browser, LIFEREA, BROWSER, GObject)

/**
 * liferea_browser_new: (skip)
 * @forceInternalBrowsing:		TRUE to act as fully fledged browser
 *
 * Function to set up a new html view widget for any purpose.
 *
 * Returns: a new Liferea HTML widget
 */
LifereaBrowser * liferea_browser_new (gboolean forceInternalBrowsing);

/**
 * liferea_browser_set_headline_view:
 *
 * Make this LifereaBrowser instance a headline view. This causes
 * an additional "go back" step for the history tab allowing to go back
 * from Web content to the headline when browsing inline.
 */
void liferea_browser_set_headline_view (LifereaBrowser *browser);

/**
 * liferea_browser_get_widget:
 * @browser:	the HTML view
 *
 * Returns the rendering widget for a HTML view. Only
 * to be used by liferea_shell.c for widget reparenting.
 *
 * Returns: (transfer none): the rendering widget
 */
GtkWidget *liferea_browser_get_widget (LifereaBrowser *browser);

/**
 * liferea_browser_clear: (skip)
 * @browser:	the HTML view widget to clear
 *
 * Loads a emtpy HTML page. Resets any item view state.
 */
void	liferea_browser_clear (LifereaBrowser *browser);

/**
 * liferea_browser_write: (skip)
 * @browser:		the browser widget to be set
 * @string:		HTML source
 * @base:		base url for resolving relative links
 *
 * Method to display the passed HTML source to the HTML widget.
 */
void	liferea_browser_write (LifereaBrowser *browser, const gchar *string, const gchar *base);

/**
 * liferea_html_view_on_url: (skip)
 * @browser:		the browser causing the event
 * @url:		new URL (or empty string)
 *
 * Callback for plugins to process on-url events. Depending on
 * the link type the link will be copied to the status bar.
 */
void liferea_browser_on_url (LifereaBrowser *browser, const gchar *url);

void liferea_browser_title_changed (LifereaBrowser *browser, const gchar *title);

void liferea_browser_progress_changed (LifereaBrowser *browser, gdouble progress);

void liferea_browser_location_changed (LifereaBrowser *browser, const gchar *location);

void liferea_browser_load_finished (LifereaBrowser *browser, const gchar *location);

/**
 * liferea_browser_handle_URL: (skip)
 * @browser:		the HTML view to use
 * @url:		URL to launch
 *
 * Launches the specified URL either in external browser or by passing
 * plain data to Readability.js or by unfiltered rendering. Alternativly it
 * handles a special URL by triggering HTML generation.
 *
 * Returns FALSE to indicate the HTML widget should launch the link itself.
 *
 * To enforce a launching behaviour do use
 *
 *    liferea_browser_launch_URL_internal (browser, url)
 *
 * or
 *
 *    browser_launch_URL_external (url)
 *
 * instead of this method.
 *
 * Returns: FALSE if link is to be launched by browser widget
 */
gboolean liferea_browser_handle_URL (LifereaBrowser *browser, const gchar *url);

/**
 * liferea_browser_launch_URL_internal: (skip)
 * @browser:		the HTML view to use
 * @url:		the URL to load
 *
 * Enforces loading of the given URL in the given browser widget.
 */
void liferea_browser_launch_URL_internal (LifereaBrowser *browser, const gchar *url);

/**
 * liferea_browser_set_zoom:
 * @zoom:	New zoom
 *
 * Function to change the zoom level of the HTML widget.
 * 1.0 is a 1:1 zoom.
 *
 */
void liferea_browser_set_zoom (LifereaBrowser *browser, gfloat zoom);

/**
 * liferea_browser_get_zoom:
 * @browser:	browser to examine
 *
 * Function to determine the current zoom level.
 *
 * Returns: the currently set zoom level
 */
gfloat liferea_browser_get_zoom (LifereaBrowser *browser);

/**
 * liferea_browser_set_reader_mode:
 * @browser:	browser to change
 * @readerMode:	new mode
 *
 * Allows to temporarily change the reader mode of the browser, will be
 * reset when navigating to another URL
 */
void liferea_browser_set_reader_mode (LifereaBrowser *browser, gboolean readerMode);

/**
 * liferea_browser_get_reader_mode:
 * @browser:	browser to get mode of
 *
 * Allows to query the currently active reader mode setting
 *
 * Returns: TRUE if reader mode is on
 */
gboolean liferea_browser_get_reader_mode (LifereaBrowser *browser);

/**
 * liferea_browser_scroll:
 * @browser:	browser to scroll
 *
 * Function scrolls down the given HTML view if possible.
 *
 */
void liferea_browser_scroll (LifereaBrowser *browser);

/**
 * liferea_browser_do_zoom:
 * @browser:	the html view
 * @zoom:	1 for zoom in, -1 for zoom out, 0 for reset
 *
 * To be called when HTML view needs to change the text size
 * of the rendering widget implementation.
 */
void liferea_browser_do_zoom (LifereaBrowser *browser, gint zoom);

/**
 * liferea_browser_update:
 *
 * Renders item or node info into the given HTML view.
 *
 * @param browser	HTML view to render to
 * @param mode		item view mode (see type itemViewMode)
 */
void liferea_browser_update (LifereaBrowser *browser, guint mode);

/**
 * liferea_browser_update_stylesheet:
 * @browser:	the html view
 *
 * Update the user stylesheet of the WebView
 */
void liferea_browser_update_stylesheet (LifereaBrowser *browser);

G_END_DECLS

#endif
