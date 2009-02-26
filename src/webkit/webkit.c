/**
 * @file webkit.c  WebKit browser module for Liferea
 *
 * Copyright (C) 2007-2009 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2008 Lars Strojny <lars@strojny.net>
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

#include <webkit/webkit.h>
#include <string.h>

#include "browser.h"
#include "conf.h"
#include "common.h"
#include "ui/liferea_htmlview.h"

/**
 * Load HTML string into the rendering scrollpane
 *
 * Load an HTML string into the web view. This is used to render
 * HTML documents created internally.
 */
static void
webkit_write_html (
	GtkWidget *scrollpane,
	const gchar *string,
	const guint length,
	const gchar *base,
	const gchar *content_type
)
{
	GtkWidget *htmlwidget;
	
	htmlwidget = gtk_bin_get_child (GTK_BIN (scrollpane));

	/* Note: we explicitely ignore the passed base URL
	   because we don't need it as Webkit supports <div href="">
	   and throws a security exception when accessing file://
	   with a non-file:// base URL */
#if WEBKIT_MINOR_VERSION > 0
	webkit_web_view_load_html_string (WEBKIT_WEB_VIEW (htmlwidget), string, "file://");
#else
	webkit_web_view_load_string (WEBKIT_WEB_VIEW (htmlwidget), string, content_type, "UTF-8", "file://");
#endif
}

static void
webkit_title_changed (
	WebKitWebView *view,
	WebKitWebFrame *frame,
	const gchar *title,
	gpointer user_data
)
{
	LifereaHtmlView	*htmlview;

	htmlview = g_object_get_data (G_OBJECT (view), "htmlview");
	liferea_htmlview_title_changed (htmlview, title);
}

static void
webkit_progress_changed (WebKitWebView *view, gint progress, gpointer user_data)
{
}

static void
webkit_location_changed (WebKitWebView *view, const gchar *location)
{
	LifereaHtmlView	*htmlview;

	htmlview = g_object_get_data (G_OBJECT (view), "htmlview");
	liferea_htmlview_location_changed (htmlview, location);
}

/**
 * Action executed when user hovers over a link
 */
static void
webkit_on_url (WebKitWebView *view, const gchar *title, const gchar *url, gpointer user_data)
{
	LifereaHtmlView	*htmlview;
	gchar *selected_url;

	htmlview    = g_object_get_data (G_OBJECT (view), "htmlview");
	selected_url = g_object_get_data (G_OBJECT (view), "selected_url");
	if (selected_url)
		g_free (selected_url);

	selected_url = url ? g_strdup (url) : g_strdup ("");

	/* overwrite or clear last status line text */
	liferea_htmlview_on_url (htmlview, selected_url);

	g_object_set_data (G_OBJECT (view), "selected_url", selected_url);
}

/**
 * A link has been clicked
 *
 * When a link has been clicked the the link management is dispatched to Liferea core in
 * order to manage the different filetypes, remote URLs.
 */
static gboolean
webkit_link_clicked (WebKitWebView *view, WebKitWebFrame *frame, WebKitNetworkRequest *request)
{
	const gchar	*uri;

	g_return_val_if_fail (WEBKIT_IS_WEB_VIEW (view), FALSE);
	g_return_val_if_fail (WEBKIT_IS_NETWORK_REQUEST (request), FALSE);

	uri = webkit_network_request_get_uri (WEBKIT_NETWORK_REQUEST (request));
	return liferea_htmlview_handle_URL (g_object_get_data (G_OBJECT (view), "htmlview"), uri);
}

/**
 * WebKitWebView::populate-popup:
 * @web_view: the object on which the signal is emitted
 * @menu: the context menu
 *
 * When a context menu is about to be displayed this signal is emitted.
 *
 * Add menu items to #menu to extend the context menu.
 */
static void
webkit_on_menu (WebKitWebView *view, GtkMenu *menu)
{
	LifereaHtmlView	*htmlview;
	gchar		*selected_url;

	htmlview = g_object_get_data (G_OBJECT (view), "htmlview");
	
	selected_url = g_object_get_data (G_OBJECT (view), "selected_url");	

	/* don't pass empty URLs */
	if (selected_url && strlen (selected_url) == 0)
		selected_url = NULL;
	else
		selected_url = g_strdup (selected_url);
		
	liferea_htmlview_prepare_context_menu (htmlview, menu, selected_url);
}

/**
 * Initializes WebKit
 *
 * Initializes the WebKit HTML rendering engine. Creates a GTK scrollpane widget
 * and embeds WebKitWebView into it.
 */
static GtkWidget *
webkit_new (LifereaHtmlView *htmlview)
{
	WebKitWebView *view;
	GtkWidget *scrollpane;
	WebKitWebSettings *settings;

	scrollpane = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (
		GTK_SCROLLED_WINDOW (scrollpane),
		GTK_POLICY_AUTOMATIC,
		GTK_POLICY_ALWAYS
	);
	gtk_scrolled_window_set_shadow_type (
		GTK_SCROLLED_WINDOW (scrollpane),
		GTK_SHADOW_IN
	);

	/** Create HTML widget and pack it into the scrolled window */
	view = WEBKIT_WEB_VIEW (webkit_web_view_new ());

	settings = webkit_web_settings_new ();
	g_object_set (
		settings,
		"default-font-size",
		11,
		NULL
	);
	g_object_set (
		settings,
		"minimum-font-size",
		7,
		NULL
	);
	g_object_set (
		settings,
		"enable-scripts",
		!conf_get_bool_value (DISABLE_JAVASCRIPT),
		NULL
	);
	g_object_set (
		settings,
		"enable-plugins",
		FALSE,
		NULL
	);
	
	webkit_web_view_set_settings (view, WEBKIT_WEB_SETTINGS (settings));
	webkit_web_view_set_full_content_zoom (WEBKIT_WEB_VIEW (view), TRUE);

	gtk_container_add (GTK_CONTAINER (scrollpane), GTK_WIDGET (view));

	/** Pass LifereaHtmlView into the WebKitWebView object */
	g_object_set_data (
		G_OBJECT (view),
		"htmlview",
		htmlview
	);

	/** Connect signal callbacks */
	g_signal_connect (
		view,
		"title-changed",
		G_CALLBACK (webkit_title_changed),
		view
	);
	g_signal_connect (
		view,
		"load-progress-changed",
		G_CALLBACK (webkit_progress_changed),
		view
	);
	g_signal_connect (
		view,
		"hovering-over-link",
		G_CALLBACK (webkit_on_url),
		view
	);
	g_signal_connect (
		view,
		"navigation-requested",
		G_CALLBACK (webkit_link_clicked),
		view
	);
	g_signal_connect (
		view,
		"populate-popup",
		G_CALLBACK (webkit_on_menu),
		view
	);

	gtk_widget_show (GTK_WIDGET (view));
	return scrollpane;
}

/**
 * Launch URL
 */
static void
webkit_launch_url (GtkWidget *scrollpane, const gchar *url)
{
	// FIXME: hack to make URIs like "gnome.org" work
	// https://bugs.webkit.org/show_bug.cgi?id=24195
	gchar *http_url;
	if (!strstr (url, "://")) {
		http_url = g_strdup_printf ("http://%s", url);
	} else {
		http_url = g_strdup (url);
	}

	// Register the new url for the history
	webkit_location_changed (
		WEBKIT_WEB_VIEW (gtk_bin_get_child (GTK_BIN (scrollpane))),
		http_url
	);

	webkit_web_view_open (
		WEBKIT_WEB_VIEW (gtk_bin_get_child (GTK_BIN (scrollpane))),
		http_url
	);

	g_free (http_url);
}

/**
 * Change zoom level of the HTML scrollpane
 */
static void
webkit_change_zoom_level (GtkWidget *scrollpane, gfloat zoom_level)
{
	WebKitWebView *view;
	view = WEBKIT_WEB_VIEW (gtk_bin_get_child (GTK_BIN (scrollpane)));
	webkit_web_view_set_zoom_level (view, zoom_level);
}

/**
 * Return current zoom level as a float
 */
static gfloat
webkit_get_zoom_level (GtkWidget *scrollpane)
{
	WebKitWebView *view;
	view = WEBKIT_WEB_VIEW (gtk_bin_get_child (GTK_BIN (scrollpane)));
	return webkit_web_view_get_zoom_level (view);
}

/**
 * Scroll page down (via shortcut key)
 *
 * Copied from gtkhtml/gtkhtml.c
 */
static gboolean
webkit_scroll_pagedown (GtkWidget *scrollpane)
{
	GtkScrolledWindow *itemview;
	GtkAdjustment *vertical_adjustment;
	gdouble old_value;
	gdouble	new_value;
	gdouble	limit;

	itemview = GTK_SCROLLED_WINDOW (scrollpane);
	g_assert (NULL != itemview);
	vertical_adjustment = gtk_scrolled_window_get_vadjustment (itemview);
	old_value = gtk_adjustment_get_value (vertical_adjustment);
	new_value = old_value + vertical_adjustment->page_increment;
	limit = vertical_adjustment->upper - vertical_adjustment->page_size;
	if (new_value > limit) {
		new_value = limit;
	}
	gtk_adjustment_set_value (vertical_adjustment, new_value);
	gtk_scrolled_window_set_vadjustment (
		GTK_SCROLLED_WINDOW (itemview),
		vertical_adjustment
	);
	return (new_value > old_value);
}

static void
webkit_set_proxy (const gchar *host, guint port, const gchar *user, const gchar *pwd)
{
	g_warning ("FIXME: Webkit rendering does not support proxies yet!");
}

static struct
htmlviewImpl webkitImpl = {
	.create		= webkit_new,
	.write		= webkit_write_html,
	.launch		= webkit_launch_url,
	.zoomLevelGet	= webkit_get_zoom_level,
	.zoomLevelSet	= webkit_change_zoom_level,
	.scrollPagedown	= webkit_scroll_pagedown,
	.setProxy	= webkit_set_proxy,
	.setOffLine	= NULL
};

DECLARE_HTMLVIEW_IMPL (webkitImpl);
