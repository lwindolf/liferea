/**
 * @file liferea_webkit.h  Webkit2 support for Liferea
 *
 * Copyright (C) 2021 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _LIFEREA_WEBKIT_H
#define _LIFEREA_WEBKIT_H

#include <webkit2/webkit2.h>

#include "ui/liferea_browser.h"

#define LIFEREA_TYPE_WEBKIT liferea_webkit_get_type ()

#define LIFEREA_WEBKIT(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_TYPE_WEBKIT, LifereaWebKit))
#define IS_LIFEREA_WEBKIT(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_TYPE_WEBKIT))
#define LIFEREA_WEBKIT_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_TYPE_WEBKIT, LifereaWebKitClass))
#define IS_LIFEREA_WEBKIT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_TYPE_WEBKIT))
#define LIFEREA_WEBKIT_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), LIFEREA_TYPE_WEBKIT, LifereaWebKitClass))

typedef struct _LifereaWebKit {
	GObject parent;
	GDBusServer 	*dbus_server;
	GList 		*dbus_connections;
} LifereaWebKit;

typedef struct _LifereaWebKitClass {
	GObjectClass parent_class;
} LifereaWebKitClass;

/**
 * liferea_webkit_new:
 * @htmlview: 	LifereaBrowser to connect
 *
 * Create new WebkitWebView object and connect signals to a LifereaBrowser
 */
GtkWidget *liferea_webkit_new (LifereaBrowser *htmlview);

/**
 * liferea_webkit_launch_url:
 * @webview:	the WebkitWebView
 * @url: 	URl string
 *
 * Launch URL in Webkit
 */
void liferea_webkit_launch_url (GtkWidget *webview, const gchar *url);

/**
 * liferea_webkit_change_zoom_level:
 * @webview:		the WebkitWebView
 * @zoom_level:		new zoom level
 *
 * Change zoom level of the HTML scrollpane
 */
void liferea_webkit_change_zoom_level (GtkWidget *webview, gfloat zoom_level);

/**
 * liferea_webkit_get_zoom_level:
 * @webview:		the WebkitWebView
 *
 * Return current zoom level as a float
 *
 * Returns: zoom level
 */
gfloat liferea_webkit_get_zoom_level (GtkWidget *webview);

/**
 * liferea_webkit_copy_selection:
 * @webview:	the WebkitWebView
 *
 * Copy selected text to the clipboard
 */
void liferea_webkit_copy_selection (GtkWidget *webview);

/**
 * liferea_webkit_scroll_pagedown:
 * @webview:	the WebkitWebView
 *
 * Scroll page down (to be triggered on shortcut key)
 */
void liferea_webkit_scroll_pagedown (GtkWidget *webview);

/**
 * liferea_webkit_set_proxy: (skip)
 */
void liferea_webkit_set_proxy (ProxyDetectMode mode, const gchar *host, guint port, const gchar *user, const gchar *pwd);

/**
 * liferea_webkit_reload_style:
 * @webview:	the WebkitWebView
 *
 * Force WebView to reload the applied stylesheet
 */
void liferea_webkit_reload_style (GtkWidget *webview);

/**
 * liferea_webkit_reload:
 * @webview:	the WebkitWebView
 *
 * Reload the current contents of webview
 */
void liferea_webkit_reload (GtkWidget *webview);

/**
 * liferea_webkit_write_html:
 * @webview:		the WebkitWebView
 * @string:		the HTML string
 * @length:		string length
 * @base: (nullable): 	base URI
 * @content_type:	HTTP content type string
 *
 * Load an HTML string into the web view. This is used to render
 * HTML documents created internally.
 */
void liferea_webkit_write_html (GtkWidget *webview, const gchar *string, const guint length, const gchar *base, const gchar *content_type);

/**
 * liferea_webkit_run_js:
 * @webview:		the WebkitWebView
 * @js:			the Javascript string
 * @cb:                 the JS callback
 *
 * Run an Javascript against the webview
 */
void liferea_webkit_run_js (GtkWidget *widget, gchar *js, GAsyncReadyCallback cb);

#endif
