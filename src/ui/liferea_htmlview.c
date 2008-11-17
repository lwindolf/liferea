/**
 * @file liferea_htmlview.c  Liferea embedded HTML rendering
 *
 * Copyright (C) 2003-2008 Lars Lindner <lars.lindner@gmail.com>
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
#include <glib.h>
#include "comments.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "feed.h"
#include "itemlist.h"
#include "net.h"
#include "social.h"
#include "render.h"
#include "ui/liferea_shell.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_tabs.h"
#include "ui/ui_prefs.h"

static htmlviewImplPtr htmlviewImpl = NULL;

extern htmlviewImplPtr htmlview_get_impl();

static void liferea_htmlview_class_init	(LifereaHtmlViewClass *klass);
static void liferea_htmlview_init	(LifereaHtmlView *htmlview);

#define LIFEREA_HTMLVIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), LIFEREA_HTMLVIEW_TYPE, LifereaHtmlViewPrivate))

struct LifereaHtmlViewPrivate {
	GtkWidget	*renderWidget;
};

enum {
	STATUSBAR_CHANGED,
	TITLE_CHANGED,
	LOCATION_CHANGED,
	OPEN_TAB,
	CLOSE_TAB,
	LAST_SIGNAL
};

static guint liferea_htmlview_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

/* -------------------------------------------------------------------- */
/* Liferea HTML rendering object					*/
/* -------------------------------------------------------------------- */

GType
liferea_htmlview_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (LifereaHtmlViewClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) liferea_htmlview_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (LifereaHtmlView),
			0, /* n_preallocs */
			(GInstanceInitFunc) liferea_htmlview_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "LifereaHtmlView",
					       &our_info, 0);
	}

	return type;
}

static void
liferea_htmlview_finalize (GObject *object)
{
	LifereaHtmlView *ls = LIFEREA_HTMLVIEW (object);
	
	g_object_unref (ls->priv->renderWidget);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
liferea_htmlview_class_init (LifereaHtmlViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = liferea_htmlview_finalize;
	
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

	liferea_htmlview_signals[OPEN_TAB] = 
		g_signal_new ("open-tab", 
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0, 
		NULL,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING);

	liferea_htmlview_signals[CLOSE_TAB] = 
		g_signal_new ("close-tab", 
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0, 
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0);
		
	g_type_class_add_private (object_class, sizeof (LifereaHtmlViewPrivate));
}

static void
liferea_htmlview_init (LifereaHtmlView *htmlview)
{
	htmlview->priv = LIFEREA_HTMLVIEW_GET_PRIVATE (htmlview);
	
	if (!htmlviewImpl)
		htmlviewImpl = htmlview_get_impl ();
}

LifereaHtmlView *
liferea_htmlview_new (gboolean forceInternalBrowsing)
{
	LifereaHtmlView *htmlview;
		
	htmlview = LIFEREA_HTMLVIEW (g_object_new (LIFEREA_HTMLVIEW_TYPE, NULL));
	htmlview->priv->renderWidget = htmlviewImpl->create (htmlview, forceInternalBrowsing);
	liferea_htmlview_clear (htmlview);
	
	return htmlview;
}

// FIXME: evil method!
GtkWidget *
liferea_htmlview_get_widget (LifereaHtmlView *htmlview)
{
	return htmlview->priv->renderWidget;
}

void
liferea_htmlview_write (LifereaHtmlView *htmlview, const gchar *string, const gchar *base)
{ 
	const gchar	*baseURL = base;
	
	if (baseURL == NULL)
		baseURL = "file:///";

	if (debug_level & DEBUG_HTML) {
		gchar *filename = common_create_cache_filename (NULL, "output", "xhtml");
		g_file_set_contents (filename, string, -1, NULL);
		g_free (filename);
	}
	
	if (!g_utf8_validate (string, -1, NULL)) {
		gchar *buffer = g_strdup (string);
		
		/* Its really a bug if we get invalid encoded UTF-8 here!!! */
		g_warning ("Invalid encoded UTF8 buffer passed to HTML widget!");
		
		/* to prevent crashes inside the browser */
		buffer = common_utf8_fix (buffer);
		(htmlviewImpl->write) (htmlview->priv->renderWidget, buffer, strlen (buffer), baseURL, "application/xhtml+xml");
		g_free (buffer);
	} else {
		(htmlviewImpl->write) (htmlview->priv->renderWidget, string, strlen (string), baseURL, "application/xhtml+xml");
	}
}

void
liferea_htmlview_clear (LifereaHtmlView *htmlview)
{
	GString	*buffer;

	buffer = g_string_new (NULL);
	htmlview_start_output (buffer, NULL, FALSE, FALSE);
	htmlview_finish_output (buffer); 
	liferea_htmlview_write (htmlview, buffer->str, NULL);
	g_string_free (buffer, TRUE);
}

gboolean
liferea_htmlview_is_special_url (const gchar *url)
{
	/* match against all special protocols, simple
	   convention: all have to start with "liferea-" */
	if (url == strstr (url, "liferea-"))
		return TRUE;
	
	return FALSE;
}

struct internalUriType {
	gchar	*suffix;
	void	(*func)(itemPtr item);
};

static struct internalUriType internalUriTypes[] = {
	/* { "tag",		FIXME }, */
	{ "flag",		itemlist_toggle_flag },
	{ "bookmark",		ui_itemlist_add_item_bookmark },
	{ "link-search",	ui_itemlist_search_item_link },
	{ "refresh-comments",	comments_refresh },
	{ NULL,			NULL }
};

void
liferea_htmlview_on_url (LifereaHtmlView *htmlview, const gchar *url)
{
	if (!liferea_htmlview_is_special_url (url))
		g_signal_emit_by_name (htmlview, "statusbar-changed", g_strdup (url));
}

void
liferea_htmlview_title_changed (LifereaHtmlView *htmlview, const gchar *title)
{
	g_signal_emit_by_name (htmlview, "title-changed", g_strdup (title));
}

void
liferea_htmlview_location_changed (LifereaHtmlView *htmlview, const gchar *location)
{
	g_signal_emit_by_name (htmlview, "location-changed", g_strdup (location));
}

void
liferea_htmlview_open (LifereaHtmlView *htmlview, const gchar *url)
{
	g_signal_emit_by_name (htmlview, "open-tab", g_strdup (url));
}

void
liferea_htmlview_close (LifereaHtmlView *htmlview)
{
	g_signal_emit_by_name (htmlview, "close-tab");
}

void
liferea_htmlview_launch_URL (LifereaHtmlView *htmlview, const gchar *url, gint launchType)
{
	struct internalUriType	*uriType;
	
	if (!htmlview)
		htmlview = browser_tabs_get_active_htmlview ();
	
	if (!url) {
		/* FIXME: bad because this is not only used for item links! */
		ui_show_error_box (_("This item does not have a link assigned!"));
		return;
	}
	
	debug3 (DEBUG_GUI, "launch URL: %s  %s %d", conf_get_bool_value (BROWSE_INSIDE_APPLICATION)?"true":"false",
		  (htmlviewImpl->launchInsidePossible) ()?"true":"false",
		  launchType);

	// FIXME: check if htmlview is an internal (item viewer...) first before handling special links
	/* first catch all links with special URLs... */
	if (liferea_htmlview_is_special_url (url)) {
	
		/* it is a generic item list URI type */		
		uriType = internalUriTypes;
		while (uriType->suffix) {
			if (!strncmp(url + strlen("liferea-"), uriType->suffix, strlen(uriType->suffix))) {
				gchar *nodeid, *itemnr;
				nodeid = strstr (url, "://");
				if (nodeid) {
					nodeid += 3;
					itemnr = nodeid;
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

						return;
					}
				}
			}
			uriType++;
		}
		g_warning ("Internal error: unhandled protocol in URL \"%s\"!", url);
		return;
	}
	
	if((launchType == UI_HTMLVIEW_LAUNCH_INTERNAL || conf_get_bool_value (BROWSE_INSIDE_APPLICATION)) &&
	   (htmlviewImpl->launchInsidePossible) () &&
	   (launchType != UI_HTMLVIEW_LAUNCH_EXTERNAL)) {
		(htmlviewImpl->launch) (htmlview->priv->renderWidget, url);
	} else {
		(void)browser_launch_URL_external (url);
	}
}

void
liferea_htmlview_set_zoom (LifereaHtmlView *htmlview, gfloat diff)
{
	(htmlviewImpl->zoomLevelSet) (htmlview->priv->renderWidget, diff); 
}

gfloat
liferea_htmlview_get_zoom (LifereaHtmlView *htmlview)
{
	return (htmlviewImpl->zoomLevelGet) (htmlview->priv->renderWidget);
}

gboolean
liferea_htmlview_scroll (LifereaHtmlView *htmlview)
{
	return (htmlviewImpl->scrollPagedown) (htmlview->priv->renderWidget);
}

void
liferea_htmlview_update_proxy (void)
{
	if (htmlviewImpl)
		(htmlviewImpl->setProxy) (network_get_proxy_host (),
		                          network_get_proxy_port (),
					  network_get_proxy_username (),
					  network_get_proxy_password ());
}

void
liferea_htmlview_set_online (gboolean online)
{
	if (htmlviewImpl)
		(htmlviewImpl->setOffLine) (!online);
}

/* -------------------------------------------------------------------- */
/* glade callbacks 							*/
/* -------------------------------------------------------------------- */

void
on_popup_launch_link_selected (gpointer url, guint callback_action, GtkWidget *widget)
{
	liferea_htmlview_launch_URL (NULL, url, UI_HTMLVIEW_LAUNCH_EXTERNAL);
}

void
on_popup_copy_url_selected (gpointer url, guint callback_action, GtkWidget *widget)
{
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get (GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text (clipboard, url, -1);
 
	clipboard = gtk_clipboard_get (GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text (clipboard, url, -1);
	
	g_free (url);
}

void
on_popup_subscribe_url_selected (gpointer url, guint callback_action, GtkWidget *widget)
{
	feedlist_add_subscription (url, NULL, NULL, NULL, FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);
	g_free (url);
}

void
on_popup_zoomin_selected (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	LifereaHtmlView	*htmlview;
	gfloat		zoom;
	
	htmlview = browser_tabs_get_active_htmlview ();
	zoom = liferea_htmlview_get_zoom (htmlview);
	zoom *= 1.2;
	
	liferea_htmlview_set_zoom (htmlview, zoom);
}

void
on_popup_zoomout_selected (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	LifereaHtmlView	*htmlview;
	gfloat		zoom;

	htmlview = browser_tabs_get_active_htmlview ();	
	zoom = liferea_htmlview_get_zoom (htmlview);
	zoom /= 1.2;
	
	liferea_htmlview_set_zoom (htmlview, zoom);
}
