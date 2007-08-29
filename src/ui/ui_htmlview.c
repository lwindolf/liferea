/**
 * @file ui_htmlview.c common interface for browser module implementations
 * and module loading functions
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gmodule.h>
#include "comments.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "feed.h"
#include "itemlist.h"
#include "net.h"
#include "plugin.h"
#include "social.h"
#include "render.h"
#include "ui/ui_enclosure.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_tabs.h"
#include "ui/ui_prefs.h"

/* function types for the imported symbols */
typedef htmlviewPluginPtr (*infoFunction)();
htmlviewPluginPtr htmlviewPlugin;

GSList *htmlviewPlugins = NULL;

extern GtkWidget *mainwindow;

static void liferea_htmlview_class_init	(LifereaHtmlViewClass *klass);
static void liferea_htmlview_init	(LifereaHtmlView *htmlview);

#define LIFEREA_HTMLVIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), LIFEREA_HTMLVIEW_TYPE, LifereaHtmlViewPrivate))

struct LifereaHtmlViewPrivate {
	GtkWidget	*renderWidget;
};

enum {
	STATUSBAR_CHANGED,
	LAST_SIGNAL
};

static guint liferea_htmlview_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

/* -------------------------------------------------------------------- */
/* module loading and initialisation					*/
/* -------------------------------------------------------------------- */

void
liferea_htmlview_plugin_init (void)
{
	GSList		*iter;
		
	/* Find best HTML renderer plugin */
	iter = htmlviewPlugins;
	while (iter) {
		htmlviewPluginPtr tmp = ((pluginPtr)iter->data)->symbols;
		if (!htmlviewPlugin || (htmlviewPlugin->priority < tmp->priority))
			htmlviewPlugin = tmp;
		iter = g_slist_next (iter);
	}
	
	if (htmlviewPlugin) {
		debug1 (DEBUG_PLUGINS, "using \"%s\" for HTML rendering...", htmlviewPlugin->name);
		htmlviewPlugin->plugin_init ();
		liferea_htmlview_update_proxy ();
	} else {
		g_error (_("Sorry, I was not able to load any installed browser plugin! Try the --debug-plugins option to get debug information!"));
	}
}

void
liferea_htmlview_plugin_deregister (void)
{
	(htmlviewPlugin->plugin_deinit) ();
}

gboolean
liferea_htmlview_plugin_register (pluginPtr plugin, GModule *handle)
{
	infoFunction		htmlview_plugin_get_info;

	if(g_module_symbol(handle, "htmlview_plugin_get_info", (void*)&htmlview_plugin_get_info)) {
		/* load feed list provider plugin info */
		if (NULL == (htmlviewPlugin = (*htmlview_plugin_get_info) ()))
			return FALSE; 
	}

	/* check feed list provider plugin version */
	if (HTMLVIEW_PLUGIN_API_VERSION != htmlviewPlugin->api_version) {
		debug3(DEBUG_PLUGINS, "html view API version mismatch: \"%s\" has version %d should be %d", htmlviewPlugin->name, htmlviewPlugin->api_version, HTMLVIEW_PLUGIN_API_VERSION);
		return FALSE;
	} 

	/* check if all mandatory symbols are provided */
	if (!(htmlviewPlugin->plugin_init &&
	      htmlviewPlugin->plugin_deinit)) {
		debug1 (DEBUG_PLUGINS, "mandatory symbols missing: \"%s\"", htmlviewPlugin->name);
		return FALSE;
	}

	/* assign the symbols so the caller will accept the plugin */
	plugin->symbols = htmlviewPlugin;

	htmlviewPlugins = g_slist_append (htmlviewPlugins, plugin);
	
	return TRUE;
}

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

	g_type_class_add_private (object_class, sizeof(LifereaHtmlViewPrivate));
}

static void
liferea_htmlview_init (LifereaHtmlView *htmlview)
{
	htmlview->priv = LIFEREA_HTMLVIEW_GET_PRIVATE (htmlview);
}

LifereaHtmlView *
liferea_htmlview_new (gboolean forceInternalBrowsing)
{
	LifereaHtmlView *htmlview;
	GtkWidget *renderWidget;
		
	htmlview = LIFEREA_HTMLVIEW (g_object_new (LIFEREA_HTMLVIEW_TYPE, NULL));
	htmlview->priv->renderWidget = htmlviewPlugin->create (htmlview, forceInternalBrowsing);	
	liferea_htmlview_clear (htmlview);
	
	return htmlview;
}

// evil method!
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
		(htmlviewPlugin->write) (htmlview->priv->renderWidget, buffer, strlen (buffer), baseURL, "application/xhtml+xml");
		g_free (buffer);
	} else {
		(htmlviewPlugin->write) (htmlview->priv->renderWidget, string, strlen (string), baseURL, "application/xhtml+xml");
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
	{ "refresh-comments",	comments_refresh },
	{ NULL,			NULL }
};

void
liferea_htmlview_on_url (LifereaHtmlView *htmlview, const gchar *url)
{
	if (!liferea_htmlview_is_special_url (url))
		g_signal_emit_by_name (htmlview, "statusbar_changed", url);
}

void
liferea_htmlview_launch_URL (LifereaHtmlView *htmlview, const gchar *url, gint launchType)
{
	struct internalUriType	*uriType;
	
	if (NULL == url) {
		/* FIXME: bad because this is not only used for item links! */
		ui_show_error_box (_("This item does not have a link assigned!"));
		return;
	}
	
	debug3 (DEBUG_GUI, "launch URL: %s  %s %d", getBooleanConfValue (BROWSE_INSIDE_APPLICATION)?"true":"false",
		  (htmlviewPlugin->launchInsidePossible) ()?"true":"false",
		  launchType);
		  
	/* first catch all links with special URLs... */
	if (liferea_htmlview_is_special_url (url)) {
		if (url == strstr (url, ENCLOSURE_PROTOCOL)) {
			ui_enclosure_new_popup (url);
			return;
		}
		
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
	
	if((launchType == UI_HTMLVIEW_LAUNCH_INTERNAL || getBooleanConfValue (BROWSE_INSIDE_APPLICATION)) &&
	   (htmlviewPlugin->launchInsidePossible) () &&
	   (launchType != UI_HTMLVIEW_LAUNCH_EXTERNAL)) {
		(htmlviewPlugin->launch) (htmlview->priv->renderWidget, url);
	} else {
		(void)liferea_htmlview_launch_in_external_browser (url);
	}
}

void
liferea_htmlview_set_zoom (LifereaHtmlView *htmlview, gfloat diff)
{
	(htmlviewPlugin->zoomLevelSet) (htmlview->priv->renderWidget, diff); 
}

gfloat
liferea_htmlview_get_zoom (LifereaHtmlView *htmlview)
{
	return (htmlviewPlugin->zoomLevelGet) (htmlview->priv->renderWidget);
}

static gboolean
liferea_htmlview_external_browser_execute (const gchar *cmd, const gchar *uri, gboolean remoteEscape, gboolean sync)
{
	GError		*error = NULL;
	gchar 		*tmpUri, *tmp, **argv, **iter;
	gint 		argc;
	gint		status = 0;
	gboolean 	done = FALSE;
  
	g_assert (cmd != NULL);
	g_assert (uri != NULL);

	/* If the command is using the X remote API we must
	   escaped all ',' in the URL */
	if (remoteEscape)
		tmpUri = common_strreplace (g_strdup (uri), ",", "%2C");
	else
		tmpUri = g_strdup (uri);

	/* If there is no %s in the command, then just append %s */
	if (strstr (cmd, "%s"))
		tmp = g_strdup (cmd);
	else
		tmp = g_strdup_printf ("%s %%s", cmd);
  
	/* Parse and substitute the %s in the command */
	g_shell_parse_argv (tmp, &argc, &argv, &error);
	g_free (tmp);
	if (error && (0 != error->code)) {
		ui_mainwindow_set_status_bar (_("Browser command failed: %s"), error->message);
		debug2 (DEBUG_GUI, "Browser command failed: %s : %s", tmp, error->message);
		g_error_free (error);
		return FALSE;
	}
  
	if (argv) {
		for (iter = argv; *iter != NULL; iter++)
			*iter = common_strreplace (*iter, "%s", tmpUri);
	}

	tmp = g_strjoinv (" ", argv);
	debug2 (DEBUG_GUI, "Running the browser-remote %s command '%s'", sync ? "sync" : "async", tmp);
	if (sync)
		g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &status, &error);
	else 
		g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
  
	if (error && (0 != error->code)) {
		debug2 (DEBUG_GUI, "Browser command failed: %s : %s", tmp, error->message);
		ui_mainwindow_set_status_bar (_("Browser command failed: %s"), error->message);
		g_error_free (error);
	} else if (status == 0) {
		ui_mainwindow_set_status_bar (_("Starting: \"%s\""), tmp);
		done = TRUE;
	}
  
	g_free (tmpUri);
	g_free (tmp);
	g_strfreev (argv);
  
	return done;
}

gboolean
liferea_htmlview_launch_in_external_browser (const gchar *uri)
{
	struct browser	*browser;
	gchar		*cmd = NULL;
	gboolean	done = FALSE;	
	
	g_assert (uri != NULL);
	
	browser = prefs_get_browser ();
	if (browser) {
		/* try to execute synchronously... */
		cmd = prefs_get_browser_command (browser, TRUE /* remote */, FALSE /* fallback */);
		if (cmd) {
			done = liferea_htmlview_external_browser_execute (cmd, uri, browser->escapeRemote, TRUE);
			g_free (cmd);
		}
	}
	
	if (done)
		return TRUE;
	
	/* if it failed try to execute asynchronously... */		
	cmd = prefs_get_browser_command (browser, FALSE /* remote */, TRUE /* fallback */);
	if (!cmd) {
	 	ui_mainwindow_set_status_bar ("fatal: cannot retrieve browser command!");
		g_warning ("fatal: cannot retrieve browser command!");
		return FALSE;
	}
	done = liferea_htmlview_external_browser_execute (cmd, uri, browser?browser->escapeRemote:FALSE, FALSE);
	g_free (cmd);
	return done;
}

gboolean
liferea_htmlview_scroll (void)
{
	LifereaHtmlView *htmlview;
	
	htmlview = ui_mainwindow_get_active_htmlview ();
	return (htmlviewPlugin->scrollPagedown) (htmlview->priv->renderWidget);
}

void
liferea_htmlview_update_proxy (void)
{
	if (htmlviewPlugin && htmlviewPlugin->setProxy)
		(htmlviewPlugin->setProxy) (network_get_proxy_host (),
		                            network_get_proxy_port (),
					    network_get_proxy_username (),
					    network_get_proxy_password ());
}

void
liferea_htmlview_set_online (gboolean online)
{
	if (htmlviewPlugin && htmlviewPlugin->setOffLine)
		(htmlviewPlugin->setOffLine) (!online);
}

/* -------------------------------------------------------------------- */
/* glade callbacks 							*/
/* -------------------------------------------------------------------- */

void
on_popup_launch_link_selected (gpointer url, guint callback_action, GtkWidget *widget)
{
	liferea_htmlview_launch_URL (ui_tabs_get_active_htmlview(), url, UI_HTMLVIEW_LAUNCH_EXTERNAL);
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
	node_request_automatic_add (url, NULL, NULL, NULL, FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);
	g_free (url);
}

void
on_popup_zoomin_selected (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	LifereaHtmlView	*htmlview;
	gfloat		zoom;
	
	htmlview = ui_tabs_get_active_htmlview ();
	zoom = liferea_htmlview_get_zoom (htmlview);
	zoom *= 1.2;
	
	liferea_htmlview_set_zoom (htmlview, zoom);
}

void
on_popup_zoomout_selected (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	LifereaHtmlView	*htmlview;
	gfloat		zoom;

	htmlview = ui_tabs_get_active_htmlview ();	
	zoom = liferea_htmlview_get_zoom (htmlview);
	zoom /= 1.2;
	
	liferea_htmlview_set_zoom (htmlview, zoom);
}
