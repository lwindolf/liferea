/*
 * @file liferea_htmlview.h  Liferea embedded HTML rendering
 *
 * Copyright (C) 2003-2018 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _LIFEREA_HTMLVIEW_H
#define _LIFEREA_HTMLVIEW_H

#include <gtk/gtk.h>

#include "net.h"

G_BEGIN_DECLS

#define LIFEREA_HTMLVIEW_TYPE		(liferea_htmlview_get_type ())
G_DECLARE_FINAL_TYPE (LifereaHtmlView, liferea_htmlview, LIFEREA, HTMLVIEW, GObject)

/**
 * liferea_htmlview_new: (skip)
 * @forceInternalBrowsing:		TRUE to act as fully fledged browser
 *
 * Function to set up a new html view widget for any purpose.
 *
 * Returns: a new Liferea HTML widget
 */
LifereaHtmlView * liferea_htmlview_new (gboolean forceInternalBrowsing);

/**
 * liferea_htmlview_set_headline_view:
 *
 * Make this LifereaHtmlView instance a headline view. This causes
 * an additional "go back" step for the history tab allowing to go back
 * from Web content to the headline when browsing inline.
 */
void liferea_htmlview_set_headline_view (LifereaHtmlView *htmlview);

/**
 * liferea_htmlview_get_widget:
 * @htmlview:	the HTML view
 *
 * Returns the rendering widget for a HTML view. Only
 * to be used by liferea_shell.c for widget reparenting.
 *
 * Returns: (transfer none): the rendering widget
 */
GtkWidget *liferea_htmlview_get_widget (LifereaHtmlView *htmlview);

/**
 * liferea_htmlview_clear: (skip)
 * @htmlview:	the HTML view widget to clear
 *
 * Loads a emtpy HTML page. Resets any item view state.
 */
void	liferea_htmlview_clear (LifereaHtmlView *htmlview);

/**
 * liferea_htmlview_write: (skip)
 * @htmlview:	The htmlview widget to be set
 * @string:		HTML source
 * @base:		base url for resolving relative links
 *
 * Method to display the passed HTML source to the HTML widget.
 */
void	liferea_htmlview_write (LifereaHtmlView *htmlview, const gchar *string, const gchar *base);

/**
 * liferea_html_view_on_url: (skip)
 * @htmlview:		the htmlview causing the event
 * @url:		new URL (or empty string)
 *
 * Callback for plugins to process on-url events. Depending on
 * the link type the link will be copied to the status bar.
 */
void liferea_htmlview_on_url (LifereaHtmlView *htmlview, const gchar *url);

void liferea_htmlview_title_changed (LifereaHtmlView *htmlview, const gchar *title);

void liferea_htmlview_location_changed (LifereaHtmlView *htmlview, const gchar *location);

/**
 * liferea_htmlview_handle_URL: (skip)
 * @htmlview:		the HTML view to use
 * @url:		URL to launch
 *
 * Launches the specified URL in the external browser or handles
 * a special URL by triggering HTML generation. Otherwise returns
 * FALSE to indicate the HTML widget should launch the link.
 *
 * To enforce a launching behaviour do use
 *
 *    liferea_htmlview_launch_URL_internal(htmlview, url)
 *
 * or
 *
 *    browser_launch_URL_external(url)
 *
 * instead of this method.
 *
 * Returns: FALSE if link is to be launched by browser widget
 */
gboolean liferea_htmlview_handle_URL (LifereaHtmlView *htmlview, const gchar *url);

/**
 * liferea_htmlview_launch_URL_internal: (skip)
 * @htmlview:		the HTML view to use
 * @url:		the URL to load
 *
 * Enforces loading of the given URL in the given browser widget.
 */
void liferea_htmlview_launch_URL_internal (LifereaHtmlView *htmlview, const gchar *url);

/**
 * liferea_htmlview_set_zoom:
 * @zoom:	New zoom
 *
 * Function to change the zoom level of the HTML widget.
 * 1.0 is a 1:1 zoom.
 *
 */
void liferea_htmlview_set_zoom (LifereaHtmlView *htmlview, gfloat zoom);

/**
 * liferea_htmlview_get_zoom:
 * @htmlview:	htmlview to examine
 *
 * Function to determine the current zoom level.
 *
 * Returns: the currently set zoom level
 */
gfloat liferea_htmlview_get_zoom (LifereaHtmlView *htmlview);

/**
 * liferea_htmlview_scroll:
 * @htmlview:	htmlview to scroll
 *
 * Function scrolls down the given HTML view if possible.
 *
 */
void liferea_htmlview_scroll (LifereaHtmlView *htmlview);

/**
 * liferea_htmlview_do_zoom:
 * @htmlview:	the html view
 * @zoom:	1 for zoom in, -1 for zoom out, 0 for reset
 *
 * To be called when HTML view needs to change the text size
 * of the rendering widget implementation.
 */
void liferea_htmlview_do_zoom (LifereaHtmlView *htmlview, gint zoom);

G_END_DECLS

/* interface for HTML rendering support implementation */
typedef struct htmlviewImpl {
	void 		(*init)			(void);
	GtkWidget*	(*create)		(LifereaHtmlView *htmlview);
	void		(*write)		(GtkWidget *widget, const gchar *string, guint length, const gchar *base, const gchar *contentType);
	void		(*launch)		(GtkWidget *widget, const gchar *url);
	gfloat		(*zoomLevelGet)		(GtkWidget *widget);
	void		(*zoomLevelSet)		(GtkWidget *widget, gfloat zoom);
	gboolean	(*hasSelection)		(GtkWidget *widget);
	void		(*copySelection)	(GtkWidget *widget);
	void		(*setProxy)		(ProxyDetectMode mode, const gchar *hostname, guint port, const gchar *username, const gchar *password);
	void		(*scrollPagedown)	(GtkWidget *widget);
	void		(*setOffLine)		(gboolean offline);
} *htmlviewImplPtr;

/**
 * htmlview_get_impl: (skip)
 */
extern htmlviewImplPtr htmlview_get_impl(void);

/* Use this macro to declare a html rendering support implementation. */
#define DECLARE_HTMLVIEW_IMPL(impl) \
        htmlviewImplPtr htmlview_get_impl(void) { \
                return &impl; \
        }

#endif
