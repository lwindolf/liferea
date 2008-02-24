/**
 * @file webkit.c WebKit browser module for Liferea
 *
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2008 Lars Strojny <larsml@strojny.net>
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

#include "conf.h"
#include "common.h"
#include "ui/ui_htmlview.h"

/**
 * HTML plugin init callback
 */
static void
liferea_webkit_init (void)
{
	g_print ("Note: WebKit HTML rendering support is experimental and\n");
	g_print ("not everything is working properly with WebKit right now!!!\n");
}

/**
 * HTML plugin de-init callback
 */
static void
liferea_webkit_deinit (void)
{
	g_print ("Shutting down WebKit\n");
}

/**
 * Load HTML string
 *
 * Load an HTML string into the view frame. This is used to render newsfeed entries.
 */
static void
webkit_write_html (GtkWidget   *scrollpane,
					const gchar *string,
		           guint       length,
		           const gchar *base,
		           const gchar *content_type)
{
	GtkWidget *htmlwidget = gtk_bin_get_child (GTK_BIN (scrollpane));
	/**
	 * WebKit does not like content type application/xhtml+xml. See
	 * http://bugs.webkit.org/show_bug.cgi?id=9677 for details.
	 */
	content_type =
		g_ascii_strcasecmp (content_type, "application/xhtml+xml") == 0
		? "application/xhtml"
		: content_type;
	webkit_web_view_load_string (WEBKIT_WEB_VIEW (htmlwidget), string, content_type, "UTF-8", base);
}

static void
webkit_title_changed (
	WebKitWebView *view,
	const gchar* title,
	const gchar* url,
	gpointer user_data
)
{
	ui_tabs_set_title (GTK_WIDGET (view), title);
}

static void
webkit_progress_changed (WebKitWebView *view, gint progress, gpointer user_data)
{
}

/**
 * Action executed when user hovers over a link
 */
static void
webkit_on_url (WebKitWebView *view, const gchar *title, const gchar *url, gpointer user_data)
{
	LifereaHtmlView	*htmlview;
	gchar		    *selectedURL;

	htmlview    = g_object_get_data (G_OBJECT (view), "htmlview");
	selectedURL = g_object_get_data (G_OBJECT (view), "selectedURL");

	selectedURL = url ? g_strdup (url) : g_strdup ("");

	/* overwrite or clear last status line text */
	liferea_htmlview_on_url (htmlview, selectedURL);

	g_object_set_data (G_OBJECT (view), "selectedURL", selectedURL);
	g_free (selectedURL);
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
	const gchar *uri;

	g_return_if_fail (WEBKIT_IS_WEB_VIEW (view));
	g_return_if_fail (WEBKIT_IS_NETWORK_REQUEST (request));


	if (!conf_get_bool_value (BROWSE_INSIDE_APPLICATION)) {

		uri = webkit_network_request_get_uri (WEBKIT_NETWORK_REQUEST (request));
		liferea_htmlview_launch_in_external_browser (uri);
		return TRUE;
	}

	return FALSE;
}

/**
 * Initializes WebKit
 *
 * Initializes the WebKit HTML rendering engine. Creates a GTK scrollpane widget
 * and embeds WebKitWebView into it.
 */
static GtkWidget *
webkit_new (LifereaHtmlView *htmlview, gboolean forceInternalBrowsing)
{
	gulong	  handler;
	GtkWidget *view;
	GtkWidget *scrollpane;
	WebKitWebSettings *settings;

	scrollpane = gtk_scrolled_window_new (NULL, NULL);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrollpane), GTK_POLICY_AUTOMATIC, GTK_POLICY_ALWAYS);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (scrollpane), GTK_SHADOW_IN);

	/** Create HTML widget and pack it into the scrolled window */
	view = webkit_web_view_new ();

	settings = webkit_web_settings_new ();
	g_object_set (settings, "default-font-size", 11, NULL);
	g_object_set (settings, "minimum-font-size", 7, NULL);
	/**
	 * FIXME: JavaScript might be disabled
	 * g_object_set (settings, "javascript-enabled", !conf_get_bool_value (DISABLE_JAVASCRIPT), NULL);
	 */
	webkit_web_view_set_settings (view, settings);

	gtk_container_add (GTK_CONTAINER (scrollpane), GTK_WIDGET (view));

	/** Pass LifereaHtmlView into the WebKitWebView object */
	g_object_set_data (G_OBJECT (view), "htmlview", htmlview);
	/** Pass internal browsing param */
	g_object_set_data (G_OBJECT (view), "internal_browsing", GINT_TO_POINTER (forceInternalBrowsing));

	/** Connect signal callbacks */
	g_signal_connect (view, "title-changed", G_CALLBACK (webkit_title_changed), view);
	g_signal_connect (view, "load-progress-changed", G_CALLBACK (webkit_progress_changed), view);
	g_signal_connect (view, "hovering-over-link", G_CALLBACK (webkit_on_url), view);
	g_signal_connect (view, "navigation-requested", G_CALLBACK (webkit_link_clicked), view);

	gtk_widget_show (view);
	return scrollpane;
}

/**
 * Launch URL
 */
static void
webkit_launch_url (GtkWidget *scrollpane, const gchar *url)
{
	webkit_web_view_open (WEBKIT_WEB_VIEW (gtk_bin_get_child (GTK_BIN (scrollpane))), url);
}

static gboolean
webkit_launch_inside_possible (void)
{
	return TRUE;
}

/**
 * FIXME: No API, see http://bugs.webkit.org/show_bug.cgi?id=14998
 */
static void
webkit_change_zoom_level (GtkWidget *scrollpane, gfloat zoomLevel)
{}

/**
 * FIXME: No API, see http://bugs.webkit.org/show_bug.cgi?id=14998
 */
static gfloat
webkit_get_zoom_level (GtkWidget *scrollpane)
{
	return 1.0;
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
	GtkAdjustment     *vertical_adjustment;
	gdouble           old_value;
	gdouble	          new_value;
	gdouble	          limit;

	itemview = GTK_SCROLLED_WINDOW (scrollpane);
	g_assert (NULL != itemview);
	vertical_adjustment = gtk_scrolled_window_get_vadjustment (itemview);
	old_value = gtk_adjustment_get_value (vertical_adjustment);
	new_value = old_value + vertical_adjustment->page_increment;
	limit = vertical_adjustment->upper - vertical_adjustment->page_size;
	if (new_value > limit)
		new_value = limit;
	gtk_adjustment_set_value (vertical_adjustment, new_value);
	gtk_scrolled_window_set_vadjustment (GTK_SCROLLED_WINDOW (itemview), vertical_adjustment);
	return (new_value > old_value);
}

static struct htmlviewPlugin webkitInfo = {
	.api_version	= HTMLVIEW_PLUGIN_API_VERSION,
	.name		= "WebKit",
	.priority	= 100,
	.externalCss	= FALSE,
	.plugin_init	= liferea_webkit_init,
	.plugin_deinit	= liferea_webkit_deinit,
	.create		= webkit_new,
	.write		= webkit_write_html,
	.launch		= webkit_launch_url,
	.launchInsidePossible = webkit_launch_inside_possible,
	.zoomLevelGet	= webkit_get_zoom_level,
	.zoomLevelSet	= webkit_change_zoom_level,
	.scrollPagedown	= webkit_scroll_pagedown,
	.setProxy	= NULL,
	.setOffLine	= NULL
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"WebKit Rendering Plugin",
	PLUGIN_TYPE_HTML_RENDERER,
	&webkitInfo
};

DECLARE_PLUGIN (pi);
DECLARE_HTMLVIEW_PLUGIN (webkitInfo);
