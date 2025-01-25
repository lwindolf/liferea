/*
 * @file liferea_browser.c  Liferea embedded browser
 *
 * Copyright (C) 2003-2025 Lars Windolf <lars.windolf@gmx.de>
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
#include <jsc/jsc.h>
#include <webkit/webkit.h>
#include "ui/liferea_browser.h"

#include <string.h>
#if !defined (G_OS_WIN32) || defined (HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#include <glib.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <locale.h>

#include "browser.h"
#include "browser_history.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "net_monitor.h"
#include "ui/browser_tabs.h"
#include "ui/item_list_view.h"
#include "ui/itemview.h"
#include "webkit/liferea_webkit.h"

/* The LifereaBrowser is a complex widget used to present both internally
   rendered content as well as serving as a browser widget. It automatically
   switches on a toolbar for history and URL navigation when browsing
   external content.

   This causes quite some complexity outlined in below table

   Use Case           Intern Rendering    Pre-Download       URL bar
   ---------------------------------------------------------------------
   item/node view     yes                 yes (feed-cache)   off
   item/node view     yes                 yes (feed-cache)   off
   local help files   no                  no                 on
   internet URL       no                  no                 on
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
	const gchar	*url = gtk_entry_buffer_get_text (gtk_entry_get_buffer (GTK_ENTRY (widget)));

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

	g_autoptr(GtkEntryBuffer) buffer = gtk_entry_buffer_new (url, -1);
	gtk_entry_set_buffer (GTK_ENTRY (browser->urlentry), buffer);
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

	g_autoptr(GtkEntryBuffer) buffer = gtk_entry_buffer_new (url, -1);
	gtk_entry_set_buffer (GTK_ENTRY (browser->urlentry), buffer);
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
	GtkWidget *widget;

	browser->content = NULL;
	browser->url = NULL;
	browser->renderWidget = liferea_webkit_new (browser);
	browser->container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	g_object_ref_sink (browser->container);
	browser->history = browser_history_new ();
	browser->toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

	widget = gtk_button_new_from_icon_name ("go-previous");
	gtk_box_append (GTK_BOX (browser->toolbar), widget);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_liferea_browser_history_back), (gpointer)browser);
	gtk_widget_set_sensitive (widget, FALSE);
	browser->back = widget;

	widget = gtk_button_new_from_icon_name ("go-next");
	gtk_box_append (GTK_BOX (browser->toolbar), widget);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_liferea_browser_history_forward), (gpointer)browser);
	gtk_widget_set_sensitive (widget, FALSE);
	browser->forward = widget;

	widget = gtk_entry_new ();
	gtk_widget_set_vexpand (widget, TRUE);
	gtk_widget_set_hexpand (widget, TRUE);
	gtk_box_append (GTK_BOX (browser->toolbar), widget);
	g_signal_connect ((gpointer)widget, "activate", G_CALLBACK (on_liferea_browser_url_entry_activate), (gpointer)browser);
	browser->urlentry = widget;

	gtk_widget_set_vexpand (browser->renderWidget, TRUE);
	gtk_widget_set_hexpand (browser->renderWidget, TRUE);

	gtk_box_append (GTK_BOX (browser->container), browser->toolbar);
	gtk_box_append (GTK_BOX (browser->container), browser->renderWidget);
}

static void
liferea_browser_online_status_changed (NetworkMonitor *nm, gboolean online, gpointer userdata)
{
	// FIXME: not yet supported
}

static void
liferea_browser_proxy_changed (NetworkMonitor *nm, gpointer userdata)
{
	liferea_webkit_set_proxy (network_get_proxy_detect_mode ());
}

static void
liferea_browser_update_stylesheet (LifereaBrowser *browser)
{
	g_autofree gchar *defaultCSS = NULL;
	g_autofree gchar *userCSS = NULL;
	gchar *filename;
	
	filename = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, "css", "liferea.css", NULL);
	g_file_get_contents (filename, &defaultCSS, NULL, NULL);
	g_free (filename);

	filename = common_create_config_filename ("liferea.css");
	g_file_get_contents (filename, &userCSS, NULL, NULL);
	g_free (filename);

	liferea_webkit_reload_style (browser->renderWidget, userCSS, defaultCSS);
}

LifereaBrowser *
liferea_browser_new (gboolean forceInternalBrowsing)
{
	LifereaBrowser *browser;

	browser = LIFEREA_BROWSER (g_object_new (LIFEREA_BROWSER_TYPE, NULL));
	browser->forceInternalBrowsing = forceInternalBrowsing;

	//conf_get_bool_value (ENABLE_READER_MODE, &(browser->readerMode));

	liferea_browser_clear (browser);

	g_signal_connect (network_monitor_get (), "online-status-changed",
	                  G_CALLBACK (liferea_browser_online_status_changed),
	                  browser);
	g_signal_connect (network_monitor_get (), "proxy-changed",
	                  G_CALLBACK (liferea_browser_proxy_changed),
	                  browser);

	debug (DEBUG_NET, "Setting initial HTML widget proxy...");
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
	gtk_widget_set_visible (browser->toolbar, FALSE);
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
		g_autoptr(GtkEntryBuffer) buffer = gtk_entry_buffer_new (browser->url, -1);
		gtk_entry_set_buffer (GTK_ENTRY (browser->urlentry), buffer);

		browser_history_add_location (browser->history, browser->url);

		gtk_widget_set_sensitive (browser->forward, browser_history_can_go_forward (browser->history));
		gtk_widget_set_sensitive (browser->back,    browser_history_can_go_back (browser->history));	

		/* We show the toolbar as it should be visible when loading external content */
		gtk_widget_set_visible (browser->toolbar, TRUE);
	} else {
		gtk_widget_set_visible (browser->toolbar, FALSE);
	}

	g_signal_emit_by_name (browser, "location-changed", location);
}

gboolean
liferea_browser_handle_URL (LifereaBrowser *browser, const gchar *url)
{
	gboolean browse_inside_application;

	g_return_val_if_fail (browser, TRUE);
	g_return_val_if_fail (url, TRUE);

	conf_get_bool_value (BROWSE_INSIDE_APPLICATION, &browse_inside_application);

	debug (DEBUG_GUI, "handle URL: %s %s",
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
		//conf_get_bool_value (ENABLE_READER_MODE, &(browser->readerMode));

		/* Save URL here as the Webkit location does not always reflect the URL.
		   For reader mode it is just liferea:// which doesn't help us to set 
		   the URL bar */
		g_free (browser->url);
		browser->url = g_strdup (url);
	}

	gtk_widget_set_sensitive (browser->forward, browser_history_can_go_forward (browser->history));
	gtk_widget_set_sensitive (browser->back,    browser_history_can_go_back (browser->history));

		g_autoptr(GtkEntryBuffer) buffer = gtk_entry_buffer_new (url, -1);
		gtk_entry_set_buffer (GTK_ENTRY (browser->urlentry), buffer);

	liferea_webkit_launch_url (browser->renderWidget, url);
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

/** render parameter type */
typedef struct renderParam {
	gchar	**params;
	guint	len;
} *renderParamPtr;

static void
render_parameter_add (renderParamPtr paramSet, const gchar *fmt, ...)
{
	gchar	*new, *value, *name;
	va_list args;

	g_assert (NULL != fmt);
	g_assert (NULL != paramSet);

	va_start (args, fmt);
	new = g_strdup_vprintf (fmt, args);
	va_end (args);

	name = new;
	value = strchr (new, '=');
	g_assert (NULL != value);
	*value = 0;
	value++;

	paramSet->len += 2;
	paramSet->params = (gchar **)g_realloc (paramSet->params, (paramSet->len + 1)*sizeof(gchar *));
	paramSet->params[paramSet->len] = NULL;
	paramSet->params[paramSet->len-2] = g_strdup (name);
	paramSet->params[paramSet->len-1] = g_strdup (value);

	g_free (new);
}

static gchar *
liferea_browser_get_template (LifereaBrowser *browser, const gchar *name) {
	static renderParamPtr	langParams = NULL;	/* the current locale settings (for localization stylesheet) */
	static GHashTable	*templates = NULL;	/* template cache */

	xsltStylesheetPtr	i18n_filter;
	xmlDocPtr		xsltDoc, templateDoc, resultDoc;

	g_autofree gchar	*tname = g_strdup_printf ("/org/gnome/liferea/templates/%s.xml", name);
	g_autoptr(GBytes)	b1 = g_resources_lookup_data (tname, 0, NULL);
	g_autoptr(GBytes)	b2 = g_resources_lookup_data ("/org/gnome/liferea/templates/i18n-filter.xslt", 0, NULL);

	if (!langParams) {
		/* Prepare localization parameters */
		gchar   **shortlang = NULL;	/* e.g. "de" */
		gchar	**lang = NULL;		/* e.g. "de_AT" */

		debug (DEBUG_HTML, "XSLT localisation: setlocale(LC_MESSAGES, NULL) reports '%s'", setlocale(LC_MESSAGES, NULL));
		lang = g_strsplit (setlocale (LC_MESSAGES, NULL), "@", 0);
		shortlang = g_strsplit (setlocale (LC_MESSAGES, NULL), "_", 0);

		langParams = g_new0 (struct renderParam, 1);
		render_parameter_add (langParams, "lang='%s'", lang[0]);
		render_parameter_add (langParams, "shortlang='%s'", shortlang[0]);
		debug (DEBUG_HTML, "XSLT localisation: lang='%s' shortlang='%s'", lang[0], shortlang[0]);

		g_strfreev (shortlang);
		g_strfreev (lang);
	}

	if (!templates)
		templates = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

	gchar *cached = g_hash_table_lookup (templates, name);
	if (cached)
		return cached;

	/* 1. load localization stylesheet */
	xsltDoc = xmlParseMemory (g_bytes_get_data (b2, NULL), g_bytes_get_size (b2));
	if (!xsltDoc) {
		g_warning ("fatal: could not parse localization stylesheet!");
		return NULL;
	}

	i18n_filter = xsltParseStylesheetDoc (xsltDoc);
	if (!i18n_filter) {
		g_warning ("fatal: could not load localization stylesheet!");
		return NULL;
	}

	/* 2. run localization on template */
	templateDoc = xmlParseMemory (g_bytes_get_data (b1, NULL), g_bytes_get_size (b1));
	if (!templateDoc) {
		g_warning ("fatal: could not parse template XML!");
		return NULL;
	}

	resultDoc = xsltApplyStylesheet (i18n_filter, templateDoc, (const gchar **)langParams->params);
	if (!resultDoc) {
		g_warning ("fatal: could not apply localization stylesheet!");
		return NULL;
	}

	xmlChar *result = NULL;
	int size = 0;

	xsltSaveResultToString (&result, &size, resultDoc, i18n_filter);
	if (result)
		g_hash_table_insert (templates, g_strdup (name), (gchar *)result);

	xmlFreeDoc (resultDoc);
	xmlFreeDoc (templateDoc);
	xsltFreeStylesheet (i18n_filter);

	return (gchar *)result;
}

void
liferea_browser_set_view (LifereaBrowser *browser, const gchar *name, const gchar *json, const gchar *baseURL, const gchar *direction)
{
	g_autofree gchar	*script = NULL;
	g_autofree gchar 	*encoded_json = NULL;
	g_autoptr(GString)	tmp;
	
	g_free (browser->url);
	browser->url = NULL;

	/* escape JSON and create a JS command */
	encoded_json = g_uri_escape_string (json, NULL, TRUE);
	script = g_strdup_printf ("load_%s('%s', '%s', '%s');\n", name, encoded_json, baseURL, direction);

	/* Each template has a script insertion marker which we need to replace */
	tmp = g_string_new (liferea_browser_get_template (browser, name));
	g_string_replace (tmp, "REPLACE_MARKER", script, 1);

	// do not use liferea_browser_write() as we need to write XHTML here
	// which is produced by intltool
	liferea_webkit_write_html (browser->renderWidget, tmp->str, strlen (tmp->str), baseURL, "text/html");
}