/**
 * @file liferea_htmlview.h  Liferea embedded HTML rendering
 *
 * Copyright (C) 2003-2010 Lars Windolf <lars.windolf@gmx.de>
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

G_BEGIN_DECLS

#define LIFEREA_HTMLVIEW_TYPE		(liferea_htmlview_get_type ())
#define LIFEREA_HTMLVIEW(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_HTMLVIEW_TYPE, LifereaHtmlView))
#define LIFEREA_HTMLVIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_HTMLVIEW_TYPE, LifereaHtmlViewClass))
#define IS_LIFEREA_HTMLVIEW(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_HTMLVIEW_TYPE))
#define IS_LIFEREA_HTMLVIEW_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_HTMLVIEW_TYPE))

typedef struct LifereaHtmlView		LifereaHtmlView;
typedef struct LifereaHtmlViewClass	LifereaHtmlViewClass;
typedef struct LifereaHtmlViewPrivate	LifereaHtmlViewPrivate;

struct LifereaHtmlView
{
	GObject		parent;
	
	/*< private >*/
	LifereaHtmlViewPrivate	*priv;
};

struct LifereaHtmlViewClass 
{
	GObjectClass parent_class;
};

GType liferea_htmlview_get_type	(void);

/** 
 * liferea_htmlview_new:
 *
 * Function to set up a new html view widget for any purpose. 
 *
 * @param forceInternalBrowsing		TRUE to act as fully fledged browser
 *
 * @returns a new Liferea HTML widget
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
 *
 * Returns the rendering widget for a HTML view. Only
 * to be used by liferea_shell.c for widget reparenting.
 *
 * @htmlview:	the HTML view
 *
 * Returns: (transfer none): the rendering widget
 */
GtkWidget *liferea_htmlview_get_widget (LifereaHtmlView *htmlview);

/** 
 * liferea_htmlview_clear:
 *
 * Loads a emtpy HTML page. Resets any item view state.
 *
 * @param htmlview	the HTML view widget to clear
 */
void	liferea_htmlview_clear (LifereaHtmlView *htmlview);

/**
 * liferea_htmlview_write:
 *
 * Method to display the passed HTML source to the HTML widget.
 *
 * @param htmlview	The htmlview widget to be set
 * @param string	HTML source
 * @param base		base url for resolving relative links
 */
void	liferea_htmlview_write (LifereaHtmlView *htmlview, const gchar *string, const gchar *base);

/**
 * Callback for plugins to process on-url events. Depending on 
 * the link type the link will be copied to the status bar.
 *
 * @param htmlview	the htmlview causing the event
 * @param url		new URL (or empty string)
 */
void liferea_htmlview_on_url (LifereaHtmlView *htmlview, const gchar *url);

void liferea_htmlview_title_changed (LifereaHtmlView *htmlview, const gchar *title);

void liferea_htmlview_location_changed (LifereaHtmlView *htmlview, const gchar *location);

/**
 * liferea_htmlview_handle_URL:
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
 * @param htmlview	the HTML view to use
 * @param url		URL to launch
 *
 * @returns FALSE if link is to be launched by browser widget
 */
gboolean liferea_htmlview_handle_URL (LifereaHtmlView *htmlview, const gchar *url);

/**
 * liferea_htmlview_launch_URL_internal:
 *
 * Enforces loading of the given URL in the given browser widget.
 *
 * @param htmlview	the HTML view to use
 * @param url		the URL to load
 */
void liferea_htmlview_launch_URL_internal (LifereaHtmlView *htmlview, const gchar *url);

/**
 * liferea_htmlview_set_zoom:
 *
 * Function to change the zoom level of the HTML widget.
 * 1.0 is a 1:1 zoom.
 *
 * @param diff	New zoom
 */
void liferea_htmlview_set_zoom (LifereaHtmlView *htmlview, gfloat zoom);

/**
 * liferea_htmlview_get_zoom:
 *
 * Function to determine the current zoom level.
 *
 * @param htmlview	htmlview to examine
 *
 * @return the currently set zoom level 
 */
gfloat liferea_htmlview_get_zoom (LifereaHtmlView *htmlview);

/**
 * liferea_htmlview_scroll:
 *
 * Function scrolls down the given HTML view if possible.
 *
 * @param htmlview	htmlview to scroll
 *
 * @return FALSE if the scrolled window vertical scroll position is at
 * the maximum and TRUE if the vertical adjustment was increased.
 */
gboolean liferea_htmlview_scroll (LifereaHtmlView *htmlview);

/**
 * liferea_htmlview_prepare_context_menu:
 *
 * Prepares a GtkMenu to be used as a context menu for the HTML view.
 *
 * @param htmlview	the html view
 * @param menu		the menu to fill
 * @param linkUri	NULL or a valid URL string if this is 
 *			to be a link context menu
 * @param imageUri	NULL or a valid image URL if this is 
 * 			to be an image context menu
 */
void liferea_htmlview_prepare_context_menu (LifereaHtmlView *htmlview, GtkMenu *menu, const gchar *linkUri, const gchar *imageUri);

/**
 * liferea_htmlview_do_zoom:
 *
 * To be called when HTML view needs to change the text size
 * of the rendering widget implementation.
 *
 * @param htmlview	the html view
 * @param in		TRUE if zoom is to be increased
 */
void liferea_htmlview_do_zoom (LifereaHtmlView *htmlview, gboolean in);

G_END_DECLS

/** interface for HTML rendering support implementation */
typedef struct htmlviewImpl {
	void 		(*init)			(void);
	GtkWidget*	(*create)		(LifereaHtmlView *htmlview);
	void		(*write)		(GtkWidget *widget, const gchar *string, guint length, const gchar *base, const gchar *contentType);
	void		(*launch)		(GtkWidget *widget, const gchar *url);
	gfloat		(*zoomLevelGet)		(GtkWidget *widget);
	void		(*zoomLevelSet)		(GtkWidget *widget, gfloat zoom);
	gboolean	(*hasSelection)		(GtkWidget *widget);
	void		(*copySelection)	(GtkWidget *widget);
	gboolean	(*scrollPagedown)	(GtkWidget *widget);
	void		(*setProxy)		(const gchar *hostname, guint port, const gchar *username, const gchar *password);
	void		(*setOffLine)		(gboolean offline);
} *htmlviewImplPtr;

extern htmlviewImplPtr htmlview_get_impl(void);

/* Use this macro to declare a html rendering support implementation. */
#define DECLARE_HTMLVIEW_IMPL(impl) \
        htmlviewImplPtr htmlview_get_impl(void) { \
                return &impl; \
        }

#endif
