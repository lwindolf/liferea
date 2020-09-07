/*
 * @file liferea_htmlview.c  Liferea embedded HTML rendering
 *
 * Copyright (C) 2003-2015 Lars Windolf <lars.windolf@gmx.de>
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

#include "ui/liferea_htmlview.h"

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
#include "net_monitor.h"
#include "social.h"
#include "render.h"
#include "htmlview.h"
#include "ui/browser_tabs.h"
#include "ui/item_list_view.h"

/* The LifereaHtmlView is a complex widget used to present both internally
   rendered content as well as serving as a browser widget. It automatically
   switches on a toolbar for history and URL navigation when browsing
   external content.

   When serving internal content it reacts to different internal link schemata
   to trigger functionality inside Liferea. To avoid websites hijacking this we
   keep a flag to support the link schema only on liferea_htmlview_write()
 */


#define RENDERER(htmlview)	(htmlview->impl)

struct _LifereaHtmlView {
	GObject	parent_instance;

	GtkWidget	*renderWidget;		/*<< The HTML widget (e.g. Webkit widget) */
	GtkWidget	*container;			/*<< Outer container including render widget and toolbar */
	GtkWidget	*toolbar;			/*<< The navigation toolbar */

	GtkWidget	*forward;			/*<< The forward button */
	GtkWidget	*back;				/*<< The back button */
	GtkWidget	*urlentry;			/*<< The URL entry widget */
	browserHistory	*history;		/*<< The browser history */

	gboolean	internal;			/*<< TRUE if internal view presenting generated HTML with special links */
	gboolean	forceInternalBrowsing;	/*<< TRUE if clicked links should be force loaded in a new tab (regardless of global preference) */

	htmlviewImplPtr impl;			/*<< Browser widget support implementation */
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

/* LifereaHtmlView toolbar callbacks */

static gboolean
on_htmlview_url_entry_activate (GtkWidget *widget, gpointer user_data)
{
	LifereaHtmlView	*htmlview = LIFEREA_HTMLVIEW (user_data);
	gchar	*url;

	url = (gchar *)gtk_entry_get_text (GTK_ENTRY (widget));
	liferea_htmlview_launch_URL_internal (htmlview, url);

	return TRUE;
}

static void
on_htmlview_history_back (GtkWidget *widget, gpointer user_data)
{
	LifereaHtmlView	*htmlview = LIFEREA_HTMLVIEW (user_data);
	gchar		*url;

	url = browser_history_back (htmlview->history);

	gtk_widget_set_sensitive (htmlview->forward, browser_history_can_go_forward (htmlview->history));
	gtk_widget_set_sensitive (htmlview->back,    browser_history_can_go_back (htmlview->history));

	liferea_htmlview_launch_URL_internal (htmlview, url);
	gtk_entry_set_text (GTK_ENTRY (htmlview->urlentry), url);
}

static void
on_htmlview_history_forward (GtkWidget *widget, gpointer user_data)
{
	LifereaHtmlView	*htmlview = LIFEREA_HTMLVIEW (user_data);
	gchar		*url;

	url = browser_history_forward (htmlview->history);

	gtk_widget_set_sensitive (htmlview->forward, browser_history_can_go_forward (htmlview->history));
	gtk_widget_set_sensitive (htmlview->back,    browser_history_can_go_back (htmlview->history));

	liferea_htmlview_launch_URL_internal (htmlview, url);
	gtk_entry_set_text (GTK_ENTRY (htmlview->urlentry), url);
}


/* LifereaHtmlView class */

static guint liferea_htmlview_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (LifereaHtmlView, liferea_htmlview, G_TYPE_OBJECT);

static void
liferea_htmlview_finalize (GObject *object)
{
	LifereaHtmlView *htmlview = LIFEREA_HTMLVIEW (object);

	browser_history_free (htmlview->history);
	g_clear_object (&htmlview->container);

	g_signal_handlers_disconnect_by_data (network_monitor_get (), object);
}

static void
liferea_htmlview_get_property (GObject *gobject, guint prop_id, GValue *value, GParamSpec *pspec)
{
	LifereaHtmlView *self = LIFEREA_HTMLVIEW (gobject);

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
liferea_htmlview_class_init (LifereaHtmlViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->get_property = liferea_htmlview_get_property;
	object_class->finalize = liferea_htmlview_finalize;

	/* LifereaHtmlView:renderWidget: */
	g_object_class_install_property (
			object_class,
			PROP_RENDER_WIDGET,
			g_param_spec_object (
				"renderwidget",
				"GtkWidget",
				"GtkWidget object",
				GTK_TYPE_WIDGET,
				G_PARAM_READABLE));

	liferea_htmlview_signals[STATUSBAR_CHANGED] =
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

	liferea_htmlview_signals[TITLE_CHANGED] =
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

	liferea_htmlview_signals[LOCATION_CHANGED] =
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

	htmlview_get_impl ()->init ();
}

static void
liferea_htmlview_init (LifereaHtmlView *htmlview)
{
	GtkWidget *widget, *image;

	htmlview->internal = FALSE;
	htmlview->impl = htmlview_get_impl ();
	htmlview->renderWidget = RENDERER (htmlview)->create (htmlview);
	htmlview->container = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);
	g_object_ref_sink (htmlview->container);
	htmlview->history = browser_history_new ();
	htmlview->toolbar = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON (widget), GTK_RELIEF_NONE);
	image = gtk_image_new_from_icon_name ("go-previous", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (widget), image);
	gtk_box_pack_start (GTK_BOX (htmlview->toolbar), widget, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_htmlview_history_back), (gpointer)htmlview);
	gtk_widget_set_sensitive (widget, FALSE);
	htmlview->back = widget;

	widget = gtk_button_new ();
	gtk_button_set_relief (GTK_BUTTON(widget), GTK_RELIEF_NONE);
	image = gtk_image_new_from_icon_name ("go-next", GTK_ICON_SIZE_BUTTON);
	gtk_widget_show (image);
	gtk_container_add (GTK_CONTAINER (widget), image);
	gtk_box_pack_start (GTK_BOX (htmlview->toolbar), widget, FALSE, FALSE, 0);
	g_signal_connect ((gpointer)widget, "clicked", G_CALLBACK (on_htmlview_history_forward), (gpointer)htmlview);
	gtk_widget_set_sensitive (widget, FALSE);
	htmlview->forward = widget;

	widget = gtk_entry_new ();
	gtk_box_pack_start (GTK_BOX (htmlview->toolbar), widget, TRUE, TRUE, 0);
	g_signal_connect ((gpointer)widget, "activate", G_CALLBACK (on_htmlview_url_entry_activate), (gpointer)htmlview);
	htmlview->urlentry = widget;

	gtk_box_pack_start (GTK_BOX (htmlview->container), htmlview->toolbar, FALSE, FALSE, 0);
	gtk_box_pack_end (GTK_BOX (htmlview->container), htmlview->renderWidget, TRUE, TRUE, 0);

	gtk_widget_show_all (htmlview->container);
}

static void
liferea_htmlview_set_online (LifereaHtmlView *htmlview, gboolean online)
{
	if (RENDERER (htmlview)->setOffLine)
		(RENDERER (htmlview)->setOffLine) (!online);
}

static void
liferea_htmlview_online_status_changed (NetworkMonitor *nm, gboolean online, gpointer userdata)
{
	LifereaHtmlView *htmlview = LIFEREA_HTMLVIEW (userdata);

	liferea_htmlview_set_online (htmlview, online);
}

static void
liferea_htmlview_proxy_changed (NetworkMonitor *nm, gpointer userdata)
{
	LifereaHtmlView *htmlview = LIFEREA_HTMLVIEW (userdata);

	if (RENDERER (htmlview)->setProxy)
		(RENDERER (htmlview)->setProxy) (network_get_proxy_detect_mode (),
						 network_get_proxy_host (),
						 network_get_proxy_port (),
						 network_get_proxy_username (),
						 network_get_proxy_password ());
}

LifereaHtmlView *
liferea_htmlview_new (gboolean forceInternalBrowsing)
{
	LifereaHtmlView *htmlview;

	htmlview = LIFEREA_HTMLVIEW (g_object_new (LIFEREA_HTMLVIEW_TYPE, NULL));
	htmlview->forceInternalBrowsing = forceInternalBrowsing;

	liferea_htmlview_clear (htmlview);

	g_signal_connect (network_monitor_get (), "online-status-changed",
	                  G_CALLBACK (liferea_htmlview_online_status_changed),
	                  htmlview);
	g_signal_connect (network_monitor_get (), "proxy-changed",
	                  G_CALLBACK (liferea_htmlview_proxy_changed),
	                  htmlview);

	debug0 (DEBUG_NET, "Setting initial HTML widget proxy...");
	liferea_htmlview_proxy_changed (network_monitor_get (), htmlview);

	return htmlview;
}

/* Needed when adding widget to a parent and querying GTK theme */
GtkWidget *
liferea_htmlview_get_widget (LifereaHtmlView *htmlview)
{
	return htmlview->container;
}

void
liferea_htmlview_write (LifereaHtmlView *htmlview, const gchar *string, const gchar *base)
{
	const gchar	*baseURL = base;
	const gchar	*errMsg = "ERROR: Invalid encoded UTF8 buffer passed to HTML widget! This shouldn't happen.";

	if (!htmlview)
		return;

	htmlview->internal = TRUE;	/* enables special links */

	if (baseURL == NULL)
		baseURL = "file:///";

	if (debug_level & DEBUG_HTML) {
		gchar *filename = common_create_cache_filename (NULL, "output", "html");
		g_file_set_contents (filename, string, -1, NULL);
		g_free (filename);
	}

	if (!g_utf8_validate (string, -1, NULL)) {
		/* It is really a bug if we get invalid encoded UTF-8 here!!! */
		(RENDERER (htmlview)->write) (htmlview->renderWidget, errMsg, strlen (errMsg), baseURL, "text/plain");
	} else {
		(RENDERER (htmlview)->write) (htmlview->renderWidget, string, strlen (string), baseURL, "text/html");
	}

	/* We hide the toolbar as it should only be shown when loading external content */
	gtk_widget_hide (htmlview->toolbar);
}

void
liferea_htmlview_clear (LifereaHtmlView *htmlview)
{
	liferea_htmlview_write (htmlview, "<html><body></body></html>", NULL);
}

static gboolean
liferea_htmlview_is_special_url (const gchar *url)
{
	/* match against all special protocols, simple
	   convention: all have to start with "liferea-" */
	if (url == strstr (url, "liferea-"))
		return TRUE;

	return FALSE;
}

struct internalUriType {
	const gchar	*suffix;
	void		(*func)(itemPtr item);
};

static struct internalUriType internalUriTypes[] = {
	{ "refresh-comments",	comments_refresh },
	{ NULL,			NULL }
};

void
liferea_htmlview_on_url (LifereaHtmlView *htmlview, const gchar *url)
{
	if (!liferea_htmlview_is_special_url (url))
		g_signal_emit_by_name (htmlview, "statusbar-changed", url);
}

void
liferea_htmlview_title_changed (LifereaHtmlView *htmlview, const gchar *title)
{
	g_signal_emit_by_name (htmlview, "title-changed", title);
}

void
liferea_htmlview_location_changed (LifereaHtmlView *htmlview, const gchar *location)
{
	if (!g_str_has_prefix (location, "liferea")) {
		/* A URI different from the locally generated html base url is being loaded. */
		htmlview->internal = FALSE;
	}
	if (!htmlview->internal) {
		browser_history_add_location (htmlview->history, location);

		gtk_widget_set_sensitive (htmlview->forward, browser_history_can_go_forward (htmlview->history));
		gtk_widget_set_sensitive (htmlview->back,    browser_history_can_go_back (htmlview->history));

		gtk_entry_set_text (GTK_ENTRY (htmlview->urlentry), location);

		/* We show the toolbar as it should be visible when loading external content */
		gtk_widget_show_all (htmlview->toolbar);
	}

	g_signal_emit_by_name (htmlview, "location-changed", location);
}

gboolean
liferea_htmlview_handle_URL (LifereaHtmlView *htmlview, const gchar *url)
{
	struct internalUriType	*uriType;
	gboolean browse_inside_application;

	g_return_val_if_fail (htmlview, TRUE);
	g_return_val_if_fail (url, TRUE);

	conf_get_bool_value (BROWSE_INSIDE_APPLICATION, &browse_inside_application);

	debug3 (DEBUG_GUI, "handle URL: %s %s %s",
	        browse_inside_application?"true":"false",
	        htmlview->forceInternalBrowsing?"true":"false",
		htmlview->internal?"true":"false");

	/* first catch all links with special URLs... */
	if (liferea_htmlview_is_special_url (url)) {
		if (htmlview->internal) {

			/* it is a generic item list URI type */
			uriType = internalUriTypes;
			while (uriType->suffix) {
				if (!strncmp (url + strlen ("liferea-"), uriType->suffix, strlen (uriType->suffix))) {
					gchar *nodeid, *itemnr;
					nodeid = strstr (url, "://");
					if (nodeid) {
						nodeid += 3;
						itemnr = strchr (nodeid, '-');
						if (itemnr) {
							itemPtr item;

							*itemnr = 0;
							itemnr++;

							item = item_load (atol (itemnr));
							if (item) {
								(*uriType->func) (item);
								item_unload (item);
							} else {
								g_warning ("Fatal: no item with id (node=%s, item=%s) found!!!", nodeid, itemnr);
							}

							return TRUE;
						}
					}
				}
				uriType++;
			}
			g_warning ("Internal error: unhandled protocol in URL \"%s\"!", url);
		} else {
			g_warning ("Security: Prevented external HTML document to use internal link scheme (%s)!", url);
		}
		return TRUE;
	}

	if(htmlview->forceInternalBrowsing || browse_inside_application) {
		return FALSE;
	} else {
		(void)browser_launch_URL_external (url);
	}

	return TRUE;
}

void
liferea_htmlview_launch_URL_internal (LifereaHtmlView *htmlview, const gchar *url)
{

	gtk_widget_set_sensitive (htmlview->forward, browser_history_can_go_forward (htmlview->history));
	gtk_widget_set_sensitive (htmlview->back,    browser_history_can_go_back (htmlview->history));

	gtk_entry_set_text (GTK_ENTRY (htmlview->urlentry), url);

	(RENDERER (htmlview)->launch) (htmlview->renderWidget, url);
}

void
liferea_htmlview_set_zoom (LifereaHtmlView *htmlview, gfloat diff)
{
	(RENDERER (htmlview)->zoomLevelSet) (htmlview->renderWidget, diff);
}

gfloat
liferea_htmlview_get_zoom (LifereaHtmlView *htmlview)
{
	return (RENDERER (htmlview)->zoomLevelGet) (htmlview->renderWidget);
}

void
liferea_htmlview_scroll (LifereaHtmlView *htmlview)
{
	(RENDERER (htmlview)->scrollPagedown) (htmlview->renderWidget);
}

void
liferea_htmlview_do_zoom (LifereaHtmlView *htmlview, gint zoom)
{
	if (!zoom)
		liferea_htmlview_set_zoom (htmlview, 1.0);
	else if (zoom > 0)
		liferea_htmlview_set_zoom (htmlview, 1.2 * liferea_htmlview_get_zoom (htmlview));
	else
		liferea_htmlview_set_zoom (htmlview, 0.8 * liferea_htmlview_get_zoom (htmlview));

}
