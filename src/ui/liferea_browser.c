/*
 * @file liferea_browser.c  Liferea embedded browser
 *
 * Copyright (C) 2003-2022 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2005-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "ui/liferea_browser.h"

#include <string.h>
#if !defined (G_OS_WIN32) || defined (HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#include <glib.h>

#include "browser.h"
#include "browser_history.h"
#include "comments.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "enclosure.h"
#include "feed.h"
#include "feedlist.h"
#include "html.h"
#include "itemlist.h"
#include "net_monitor.h"
#include "social.h"
#include "render.h"
#include "update.h"
#include "ui/browser_tabs.h"
#include "ui/item_list_view.h"
#include "ui/itemview.h"
#include "webkit/webkit.h"

/* The LifereaBrowser is a complex widget used to present both internally
   rendered content as well as serving as a browser widget. It automatically
   switches on a toolbar for history and URL navigation when browsing
   external content.

   The widget also manages "reader mode" be it on by default or ad-hoc 
   requested by the user. To do so it ad-hoc injects Readability.js into the
   rendering. For increased performance and security external content is pre-fetched
   and passed directly to Readability.js (thus eliminating all 3rd party 
   script execution).

   This causes quite some complexity outlined in below table

   Use Case           Intern Rendering    Reader Mode    Pre-Download       URL bar
   --------------------------------------------------------------------------------
   item/node view     yes                 yes            yes (feed-cache)   off
   item/node view     yes                 no             yes (feed-cache)   off
   local help files   no                  no             no                 on
   internet URL       no                  no             no                 on
   internet URL       yes                 yes            yes                on
 */

struct _LifereaBrowser {
	GObject	parent_instance;

	GtkWidget	*renderWidget;		/*<< The HTML widget (e.g. Webkit widget) */
	GtkWidget	*container;		/*<< Outer container including render widget and toolbar */
	GtkWidget	*toolbar;		/*<< The navigation toolbar */

	GtkWidget	*forward;		/*<< The forward button */
	GtkWidget	*back;			/*<< The back button */
	GtkWidget	*urlentry;		/*<< The URL entry widget */
	browserHistory	*history;		/*<< The browser history */

	gboolean	forceInternalBrowsing;	/*<< TRUE if clicked links should be force loaded in a new tab (regardless of global preference) */
	gboolean	readerMode;		/*<< TRUE if Readability.js is to be used */
	gint		viewMode;		/*<< current view mode for internal viewing */

	gchar		*url;			/*<< the URL of the content rendered right now */
	gchar 		*content;		/*<< current HTML content (excluding decorations, content passed to Readability.js) */
};

enum {
	STATUSBAR_CHANGED,
	TITLE_CHANGED,
	LOCATION_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_NONE,
	PROP_RENDER_WIDGET
};

/* LifereaBrowser toolbar callbacks */

static gboolean
on_liferea_browser_url_entry_activate (GtkWidget *widget, gpointer user_data)
{
	LifereaBrowser	*browser = LIFEREA_BROWSER (user_data);
	gchar	*url;

	url = (gchar *)gtk_entry_get_text (GTK_ENTRY (widget));
	liferea_browser_launch_URL_internal (browser, url);

	return TRUE;
}

static void
on_liferea_browser_history_back (GtkWidget *widget, gpointer user_data)
{
	LifereaBrowser	*browser = LIFEREA_BROWSER (user_data);
	gchar		*url;

	url = browser_history_back (browser->history);

	gtk_widget_set_sensitive (browser->forward, browser_history_can_go_forward (browser->history));
	gtk_widget_set_sensitive (browser->back,    browser_history_can_go_back (browser->history));

	liferea_browser_launch_URL_internal (browser, url);
	gtk_entry_set_text (GTK_ENTRY (browser->urlentry), url);
}

static void
on_liferea_browser_history_forward (GtkWidget *widget, gpointer user_data)
{
	LifereaBrowser	*browser = LIFEREA_BROWSER (user_data);
	gchar		*url;

	url = browser_history_forward (browser->history);

	gtk_widget_set_sensitive (browser->forward, browser_history_can_go_forward (browser->history));
	gtk_widget_set_sensitive (browser->back,    browser_history_can_go_back (browser->history));

	liferea_browser_launch_URL_internal (browser, url);
	gtk_entry_set_text (GTK_ENTRY (browser->urlentry), url);
}


/* LifereaBrowser class */

static guint liferea_browser_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (LifereaBrowser, liferea_browser, G_TYPE_OBJECT);

static void
liferea_browser_finalize (GObject *object)
{
	LifereaBrowser *browser = LIFEREA_BROWSER (object);

	browser_history_free (browser->history);
	g_clear_object (&browser->container);
	g_free (browser->content);

	g_signal_handlers_disconnect_by_data (network_monitor_get (), object);
}

static void
liferea_browser_get_property (GObject *gobject, guint prop_id, GValue *value, GParamSpec *pspec)
{
	LifereaBrowser *self = LIFEREA_BROWSER (gobject);

	switch (prop_id) {
		case PROP_RENDER_WIDGET:
			g_value_set_object (value, self->renderWidget);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (gobject, prop_id, pspec);
			break;
	}
}

static void
liferea_browser_class_init (LifereaBrowserClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = liferea_browser_get_property;
	object_class->finalize = liferea_browser_finalize;

	/* LifereaBrowser:renderWidget: */
	g_object_class_install_property (
			object_class,
			PROP_RENDER_WIDGET,
			g_param_spec_object (
				"renderwidget",
				"GtkWidget",
				"GtkWidget object",
				GTK_TYPE_WIDGET,
				G_PARAM_READABLE));

	liferea_browser_signals[STATUSBAR_CHANGED] =
		g_signal_new ("statusbar-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING);

	liferea_browser_signals[TITLE_CHANGED] =
		g_signal_new ("title-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING);

	liferea_browser_signals[LOCATION_CHANGED] =
		g_signal_new ("location-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING);
}

static void
liferea_browser_init (LifereaBrowser *browser)
{
	GtkWidget *widget, *image;

	browser->content = NULL;
	browser->url = NULL;
	browser->readerMode = FALSE;
	browser->renderWidget = liferea_webkit_new (browser);
	browser->container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	g_object_ref_sink (browser->container);
	browser->history = browser_history_new ();
	browser->toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	image = gtk_image_new_from_icon_name ("go-previous", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (widget), image);
	gtk_box_pack_start (GTK_BOX (browser->toolbar), widget, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_liferea_browser_history_back), (gpointer)browser);
	gtk_widget_set_sensitive (widget, FALSE);
	browser->back = widget;

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON(widget), GTK_RELIEF_NONE);
	image = gtk_image_new_from_icon_name ("go-next", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (widget), image);
	gtk_box_pack_start (GTK_BOX (browser->toolbar), widget, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_liferea_browser_history_forward), (gpointer)browser);
	gtk_widget_set_sensitive (widget, FALSE);
	browser->forward = widget;

	widget = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (browser->toolbar), widget, TRUE, TRUE, 0);
	g_signal_connect ((gpointer)widget, "activate", G_CALLBACK (on_liferea_browser_url_entry_activate), (gpointer)browser);
	browser->urlentry = widget;

	gtk_box_pack_start (GTK_BOX (browser->container), browser->toolbar, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (browser->container), browser->renderWidget, TRUE, TRUE, 0);

	gtk_widget_show_all (browser->container);
}

static void
liferea_browser_online_status_changed (NetworkMonitor *nm, gboolean online, gpointer userdata)
{
	// FIXME: not yet supported
}

static void
liferea_browser_proxy_changed (NetworkMonitor *nm, gpointer userdata)
{
	liferea_webkit_set_proxy (
		network_get_proxy_detect_mode (),
		network_get_proxy_host (),
		network_get_proxy_port (),
		network_get_proxy_username (),
		network_get_proxy_password ()
	);
}

LifereaBrowser *
liferea_browser_new (gboolean forceInternalBrowsing)
{
	LifereaBrowser *browser;

	browser = LIFEREA_BROWSER (g_object_new (LIFEREA_BROWSER_TYPE, NULL));
	browser->forceInternalBrowsing = forceInternalBrowsing;

	conf_get_bool_value (ENABLE_READER_MODE, &(browser->readerMode));

	liferea_browser_clear (browser);

	g_signal_connect (network_monitor_get (), "online-status-changed",
	                  G_CALLBACK (liferea_browser_online_status_changed),
	                  browser);
	g_signal_connect (network_monitor_get (), "proxy-changed",
	                  G_CALLBACK (liferea_browser_proxy_changed),
	                  browser);

	debug0 (DEBUG_NET, "Setting initial HTML widget proxy...");
	liferea_browser_proxy_changed (network_monitor_get (), browser);

	liferea_browser_update_stylesheet (browser);

	return browser;
}

/* Needed when adding widget to a parent and querying GTK theme */
GtkWidget *
liferea_browser_get_widget (LifereaBrowser *browser)
{
	return browser->container;
}

void
liferea_browser_write (LifereaBrowser *browser, const gchar *string, const gchar *base)
{
	const gchar	*baseURL = base;
	const gchar	*errMsg = "ERROR: Invalid encoded UTF8 buffer passed to HTML widget! This shouldn't happen.";

	if (!browser)
		return;

	if (baseURL == NULL)
		baseURL = "file:///";

	if (!g_utf8_validate (string, -1, NULL)) {
		/* It is really a bug if we get invalid encoded UTF-8 here!!! */
		liferea_webkit_write_html (browser->renderWidget, errMsg, strlen (errMsg), baseURL, "text/plain");
	} else {
		liferea_webkit_write_html (browser->renderWidget, string, strlen (string), baseURL, "text/html");
	}

	/* We hide the toolbar as it should only be shown when loading external content */
	gtk_widget_hide (browser->toolbar);
}

void
liferea_browser_clear (LifereaBrowser *browser)
{
	liferea_browser_write (browser, "<html><body></body></html>", NULL);
}

struct internalUriType {
	const gchar	*suffix;
	void		(*func)(itemPtr item);
};

void
liferea_browser_on_url (LifereaBrowser *browser, const gchar *url)
{
	g_signal_emit_by_name (browser, "statusbar-changed", url);
}

void
liferea_browser_title_changed (LifereaBrowser *browser, const gchar *title)
{
	g_signal_emit_by_name (browser, "title-changed", title);
}

void
liferea_browser_progress_changed (LifereaBrowser *browser, gdouble progress)
{
	double bar_progress = (progress == 1.0)?0.0:progress;
	gtk_entry_set_progress_fraction (GTK_ENTRY (browser->urlentry), bar_progress);
}

void
liferea_browser_location_changed (LifereaBrowser *browser, const gchar *location)
{
	if (browser->url && !g_str_has_prefix (browser->url, "liferea://")) {
		browser_history_add_location (browser->history, browser->url);

		gtk_widget_set_sensitive (browser->forward, browser_history_can_go_forward (browser->history));
		gtk_widget_set_sensitive (browser->back,    browser_history_can_go_back (browser->history));

		gtk_entry_set_text (GTK_ENTRY (browser->urlentry), browser->url);

		/* We show the toolbar as it should be visible when loading external content */
		gtk_widget_show_all (browser->toolbar);
	} else {
		gtk_widget_hide (browser->toolbar);
	}

	g_signal_emit_by_name (browser, "location-changed", location);
}

/*
 * Loading callback to check wether a loading error happened. If yes and reader was
 * on try loading without reader mode.
 */
static void
liferea_browser_load_finished_cb (GObject *object, GAsyncResult *result, gpointer user_data)
{
	WebKitJavascriptResult *js_result;
	JSCValue               *value;
	GError                 *error = NULL;

	js_result = webkit_web_view_run_javascript_finish (WEBKIT_WEB_VIEW (object), result, &error);
	if (!js_result) {
		debug1 (DEBUG_HTML, "Error running javascript: %s", error->message);
		g_error_free (error);
		return;
	}

	value = webkit_javascript_result_get_js_value (js_result);
	if (jsc_value_is_boolean (value)) {
		LifereaBrowser *browser = LIFEREA_BROWSER (user_data);
		gboolean result = jsc_value_to_boolean (value);

		if (!result && browser->readerMode && browser->url) {
			debug0 (DEBUG_HTML, "loadContent() reader mode fail -> reloading without reader");
			browser->readerMode = FALSE;
			liferea_browser_launch_URL_internal (browser, browser->url);
		}
	} else {
		g_warning ("Error running javascript: unexpected return value");
	}
	webkit_javascript_result_unref (js_result);
}

void
liferea_browser_load_finished (LifereaBrowser *browser, const gchar *location)
{
	/*
	    Add Readability.js handling
	    - for external content: if user chose so
	    - for internal content: always (Readability is enable on demand here)
	 */
	if (browser->readerMode || (location == strstr (location, "liferea://"))) {
		g_autoptr(GBytes) b1 = NULL, b2 = NULL, b3 = NULL;

		// Return Readability.js and Liferea specific loader code
		b1 = g_resources_lookup_data ("/org/gnome/liferea/readability/Readability-readerable.js", 0, NULL);
		b2 = g_resources_lookup_data ("/org/gnome/liferea/readability/Readability.js", 0, NULL);
		b3 = g_resources_lookup_data ("/org/gnome/liferea/htmlview.js", 0, NULL);

		g_assert(b1 != NULL);
		g_assert(b2 != NULL);
		g_assert(b3 != NULL);

		debug1 (DEBUG_GUI, "Enabling reader mode for '%s'", location);
		liferea_webkit_run_js (
			browser->renderWidget,
			g_strdup_printf ("%s\n%s\n%s\nloadContent(%s, '%s');\n",
		        (gchar *)g_bytes_get_data (b1, NULL),
			(gchar *)g_bytes_get_data (b2, NULL),
			(gchar *)g_bytes_get_data (b3, NULL),
			(browser->readerMode?"true":"false"),
			browser->content != NULL ? browser->content : ""),
			liferea_browser_load_finished_cb
		);
	}
}

/* Asynchronously download website for loading into Readability.js */
static void
liferea_browser_load_reader_content_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	LifereaBrowser *browser = LIFEREA_BROWSER (userdata);
	gchar *html;
	
	if (!result->data) {
		browser->content = g_uri_escape_string (_("Content download failed! Try disabling reader mode."), NULL, TRUE);
	} else {
		// HTML5 content extraction
		html = html_get_article (result->data, result->source);
		
		// HTML fallback
		if (!html)
			html = html_get_body (result->data, result->source);
		
		if (html) {
			browser->content = g_uri_escape_string (html, NULL, TRUE);
			g_free (html);
		} else {
			browser->content = g_uri_escape_string(_("Content extraction failed! Try disabling reader mode."), NULL, TRUE);
		}
	}
		
	liferea_webkit_run_js (browser->renderWidget,
	                       g_strdup_printf ("setBase('%s');loadContent(true, '<body>%s</body>');\n",
	                                        result->source,
	                                        browser->content),
	                       liferea_browser_load_finished_cb);
}

static void
liferea_browser_load_reader_content (LifereaBrowser *browser, const gchar *url)
{
	UpdateRequest	*request;
	
	/* Drop pending render loading requests */
	update_job_cancel_by_owner (browser);
	
	request = update_request_new (
		url,
		NULL, 	// No update state needed? How do we prevent an endless redirection loop?
		NULL 	// no auth needed/supported here
	);
	update_execute_request (browser, request, liferea_browser_load_reader_content_cb, browser, FEED_REQ_NO_FEED);
}

/* Render layout for presenting an external website in reader mode */
static gchar *
liferea_browser_render_reader (const gchar *url)
{
	xmlDocPtr	doc;
	xmlNodePtr	rootNode;
	gchar		*result;
	
	doc = xmlNewDoc (BAD_CAST"1.0");
	rootNode = xmlNewDocNode (doc, NULL, BAD_CAST"website", NULL);
	xmlDocSetRootElement (doc, rootNode);
	xmlNewTextChild (rootNode, NULL, BAD_CAST"source", BAD_CAST url);
	result = render_xml (doc, "reader", NULL);
	xmlFreeDoc (doc);
	
	return result;
}

gboolean
liferea_browser_handle_URL (LifereaBrowser *browser, const gchar *url)
{
	gboolean browse_inside_application;

	g_return_val_if_fail (browser, TRUE);
	g_return_val_if_fail (url, TRUE);

	conf_get_bool_value (BROWSE_INSIDE_APPLICATION, &browse_inside_application);

	debug2 (DEBUG_GUI, "handle URL: %s %s",
	        browse_inside_application?"true":"false",
	        browser->forceInternalBrowsing?"true":"false");

	if(browser->forceInternalBrowsing || browse_inside_application) {
		liferea_browser_launch_URL_internal (browser, url);
		return FALSE;
	} else {
		(void)browser_launch_URL_external (url);
	}

	return TRUE;
}

void
liferea_browser_launch_URL_internal (LifereaBrowser *browser, const gchar *url)
{
	/* For new URLs: reset any intermediate reader mode change via browser context menu */
	if (!browser->url || !g_str_equal (url, browser->url)) {
		conf_get_bool_value (ENABLE_READER_MODE, &(browser->readerMode));

		/* Save URL here as the Webkit location does not always reflect the URL.
		   For reader mode it is just liferea:// which doesn't help us to set 
		   the URL bar */
		g_free (browser->url);
		browser->url = g_strdup (url);
	}

	if (browser->readerMode) {
		liferea_browser_write (browser, liferea_browser_render_reader (url), NULL);
		liferea_browser_load_reader_content (browser, url);
	} else {
		gtk_widget_set_sensitive (browser->forward, browser_history_can_go_forward (browser->history));
		gtk_widget_set_sensitive (browser->back,    browser_history_can_go_back (browser->history));

		gtk_entry_set_text (GTK_ENTRY (browser->urlentry), url);

		liferea_webkit_launch_url (browser->renderWidget, url);
	}
}

void
liferea_browser_set_zoom (LifereaBrowser *browser, gfloat diff)
{
	liferea_webkit_change_zoom_level (browser->renderWidget, diff);
}

gfloat
liferea_browser_get_zoom (LifereaBrowser *browser)
{
	return liferea_webkit_get_zoom_level (browser->renderWidget);
}

void
liferea_browser_scroll (LifereaBrowser *browser)
{
	liferea_webkit_scroll_pagedown (browser->renderWidget);
}

void
liferea_browser_do_zoom (LifereaBrowser *browser, gint zoom)
{
	if (!zoom)
		liferea_browser_set_zoom (browser, 1.0);
	else if (zoom > 0)
		liferea_browser_set_zoom (browser, 1.2 * liferea_browser_get_zoom (browser));
	else
		liferea_browser_set_zoom (browser, 0.8 * liferea_browser_get_zoom (browser));

}

static void
liferea_browser_start_output (GString *buffer,
                               const gchar *base)
{
	/* Prepare HTML boilderplate */
	g_string_append (buffer, "<!DOCTYPE html>\n");
	g_string_append (buffer, "<html>\n");
	g_string_append (buffer, "<head>\n<title>HTML View</title>");

	// FIXME: consider adding CSP meta tag here as e.g. Firefox reader mode page does
	g_string_append (buffer, "<meta http-equiv=\"Content-Type\" content=\"text/html; charset=utf-8\" />");

	if (base) {
		gchar *escBase = g_markup_escape_text (base, -1);
		g_string_append (buffer, "<base href=\"");
		g_string_append (buffer, escBase);
		g_string_append (buffer, "\" />\n");
		g_free (escBase);
	}

	g_string_append (buffer, "</head><body></body></html>");
}

/* renders headlines & node info */
static void
liferea_browser_refresh (LifereaBrowser *browser, guint mode)
{
	GString		*output;
	nodePtr		node = NULL;
	itemPtr		item = NULL;
	gchar		*baseURL = NULL;
	gchar		*content = NULL;

	/* determine base URL */
	switch (mode) {
		case ITEMVIEW_SINGLE_ITEM:
			item = itemlist_get_selected ();
			if(item) {
				baseURL = (gchar *)node_get_base_url (node_from_id (item->nodeId));
				item_unload (item);
			}
			break;
		case ITEMVIEW_NODE_INFO:
			node = feedlist_get_selected ();
			if (!node)
				return;
			baseURL = (gchar *) node_get_base_url (node);
			break;
	}

	if (baseURL)
		baseURL = g_markup_escape_text (baseURL, -1);

	output = g_string_new (NULL);
	liferea_browser_start_output (output, baseURL);

	switch (mode) {
		case ITEMVIEW_SINGLE_ITEM:
			item = itemlist_get_selected ();
			if (item) {
				content = item_render (item, mode);
				item_unload (item);
			}
			break;
		case ITEMVIEW_NODE_INFO:
			if (node)
				content = node_render (node);
			break;
		default:
			g_warning ("HTML view: invalid viewing mode!!!");
			break;
	}

	if (debug_level & DEBUG_HTML) {
		gchar *filename = common_create_cache_filename (NULL, "output", "html");
		g_file_set_contents (filename, content, -1, NULL);
		g_free (filename);
	}

	g_free (browser->content);
	g_free (browser->url);
	browser->content = NULL;
	browser->url = NULL;
	browser->viewMode = mode;

	if (content) {
		/* URI escape our content for safe transfer to Readability.js
		   URI escaping is needed for UTF-8 conservation and for JS stringification */
		browser->content = g_uri_escape_string (content, NULL, TRUE);
		g_free (content);
	} else {
		browser->content = g_uri_escape_string ("", NULL, TRUE);
	}

	liferea_browser_write (browser, output->str, baseURL);
	g_string_free (output, TRUE);
	g_free (baseURL);
}

/* reset reader state and load new item/node */
void
liferea_browser_update (LifereaBrowser *browser, guint mode)
{
	/* Reset any intermediate reader mode change via browser context menu */
	conf_get_bool_value (ENABLE_READER_MODE, &(browser->readerMode));

	liferea_browser_refresh (browser, mode);
}

void
liferea_browser_set_reader_mode (LifereaBrowser *browser, gboolean readerMode)
{
	browser->readerMode = readerMode;

	/* Toggling reader mode can happen in different situations
	   for which we need to trigger different re-renderings:

		What is shown           How to re-render it
		-------------------------------------------
		item/node view          liferea_browser_refresh
		local help files 	liferea_browser_handle_URL_internal
		internet URL     	liferea_browser_handle_URL_internal
	*/
	if (browser->url)
		liferea_browser_launch_URL_internal (browser, browser->url);
	else
		liferea_browser_refresh (browser, browser->viewMode);
}

gboolean
liferea_browser_get_reader_mode (LifereaBrowser *browser)
{
	return browser->readerMode;
}

void
liferea_browser_update_stylesheet (LifereaBrowser *browser)
{
	liferea_webkit_reload_style (browser->renderWidget);
}
