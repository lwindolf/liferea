/**
 * @file ui_htmlview.c common interface for browser module implementations
 * and module loading functions
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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
#include "common.h"
#include "conf.h"
#include "callbacks.h"
#include "debug.h"
#include "plugin.h"
#include "social.h"
#include "support.h"
#include "render.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_tabs.h"
#include "ui/ui_prefs.h"
#include "ui/ui_enclosure.h"

/* function types for the imported symbols */
typedef htmlviewPluginPtr (*infoFunction)();
htmlviewPluginPtr htmlviewPlugin;

GSList *htmlviewPlugins = NULL;

extern GtkWidget *mainwindow;

extern char	*proxyname;
extern char	*proxyusername;
extern char	*proxypassword;
extern int	proxyport;

/* -------------------------------------------------------------------- */
/* module loading and initialisation					*/
/* -------------------------------------------------------------------- */

void ui_htmlview_init(void) {
	GSList		*iter;
		
	/* Find best HTML renderer plugin */
	iter = htmlviewPlugins;
	while(iter) {
		htmlviewPluginPtr tmp = ((pluginPtr)iter->data)->symbols;
		if(!htmlviewPlugin || (htmlviewPlugin->priority < tmp->priority))
			htmlviewPlugin = tmp;
		iter = g_slist_next(iter);
	}
	
	if(htmlviewPlugin) {
		debug1(DEBUG_PLUGINS, "using \"%s\" for HTML rendering...", htmlviewPlugin->name);
		htmlviewPlugin->plugin_init();
		ui_htmlview_set_proxy(proxyname, proxyport, proxyusername, proxypassword);
	} else {
		g_error(_("Sorry, I was not able to load any installed browser plugin! Try the --debug-plugins option to get debug information!"));
	}
}

void ui_htmlview_deinit() {
	(htmlviewPlugin->plugin_deinit)();
}

gboolean ui_htmlview_plugin_load(pluginPtr plugin, GModule *handle) {
	infoFunction		htmlview_plugin_get_info;

	if(g_module_symbol(handle, "htmlview_plugin_get_info", (void*)&htmlview_plugin_get_info)) {
		/* load feed list provider plugin info */
		if(NULL == (htmlviewPlugin = (*htmlview_plugin_get_info)()))
			return FALSE;
	}

	/* check feed list provider plugin version */
	if(HTMLVIEW_PLUGIN_API_VERSION != htmlviewPlugin->api_version) {
		debug3(DEBUG_PLUGINS, "html view API version mismatch: \"%s\" has version %d should be %d\n", htmlviewPlugin->name, htmlviewPlugin->api_version, HTMLVIEW_PLUGIN_API_VERSION);
		return FALSE;
	} 

	/* check if all mandatory symbols are provided */
	if(!(htmlviewPlugin->plugin_init &&
	     htmlviewPlugin->plugin_deinit)) {
		debug1(DEBUG_PLUGINS, "mandatory symbols missing: \"%s\"\n", htmlviewPlugin->name);
		return FALSE;
	}

	/* assign the symbols so the caller will accept the plugin */
	plugin->symbols = htmlviewPlugin;

	htmlviewPlugins = g_slist_append(htmlviewPlugins, plugin);
	
	return TRUE;
}

/* -------------------------------------------------------------------- */
/* browser module interface functions					*/
/* -------------------------------------------------------------------- */

GtkWidget *ui_htmlview_new(gboolean forceInternalBrowsing) {
	GtkWidget *htmlview = htmlviewPlugin->create(forceInternalBrowsing);
	
	ui_htmlview_clear(htmlview);
	
	return htmlview;
}

void ui_htmlview_write(GtkWidget *htmlview, const gchar *string, const gchar *base) { 
	const gchar	*baseURL = base;
	
	if(baseURL == NULL)
		baseURL = "file:///";

	if(debug_level & DEBUG_HTML) {
		gchar *filename = common_create_cache_filename(NULL, "output", "xhtml");
		g_file_set_contents(filename, string, -1, NULL);
		g_free(filename);
	}
	
	if(!g_utf8_validate(string, -1, NULL)) {
		gchar *buffer = g_strdup(string);
		
		/* Its really a bug if we get invalid encoded UTF-8 here!!! */
		g_warning("Invalid encoded UTF8 buffer passed to HTML widget!");
		
		/* to prevent crashes inside the browser */
		buffer = common_utf8_fix(buffer);
		(htmlviewPlugin->write)(htmlview, buffer, strlen(buffer), baseURL, "application/xhtml+xml");
		g_free(buffer);
	} else {
		(htmlviewPlugin->write)(htmlview, string, strlen(string), baseURL, "application/xhtml+xml");
	}
}

void ui_htmlview_clear(GtkWidget *widget) {
	GString	*buffer;

	buffer = g_string_new(NULL);
	htmlview_start_output(buffer, NULL, FALSE, FALSE);
	htmlview_finish_output(buffer); 
	ui_htmlview_write(widget, buffer->str, NULL);
	g_string_free(buffer, TRUE);
}

gboolean ui_htmlview_is_special_url(const gchar *url) {

	/* match against all special protocols, simple
	   convention: all have to start with "liferea-" */
	if(url == strstr(url, "liferea-"))
		return TRUE;
	
	return FALSE;
}

struct internalUriType {
	gchar	*suffix;
	void	(*func)(itemPtr item);
};

static struct internalUriType internalUriTypes[] = {
	/* { "tag",	FIXME }, */
	{ "flag",	itemlist_toggle_flag },
	{ "bookmark",	ui_itemlist_add_item_bookmark },
	{ NULL,		NULL }
};

void ui_htmlview_on_url(const gchar *url) {

	if(!ui_htmlview_is_special_url(url))
		ui_mainwindow_set_status_bar("%s", url);
}

void ui_htmlview_launch_URL(GtkWidget *htmlview, const gchar *url, gint launchType) {
	struct internalUriType	*uriType;
	
	if(NULL == url) {
		/* FIXME: bad because this is not only used for item links! */
		ui_show_error_box(_("This item does not have a link assigned!"));
		return;
	}
	
	debug3(DEBUG_GUI, "launch URL: %s  %s %d\n", getBooleanConfValue(BROWSE_INSIDE_APPLICATION)?"true":"false",
		  (htmlviewPlugin->launchInsidePossible)()?"true":"false",
		  launchType);
		  
	/* first catch all links with special URLs... */
	if(url == strstr(url, "liferea-")) {
		if(url == strstr(url, ENCLOSURE_PROTOCOL)) {
			ui_enclosure_new_popup(url);
			return;
		}
		
		/* it is a generic item list URI type */		
		uriType = internalUriTypes;
		while(uriType->suffix) {
			if(!strncmp(url + strlen("liferea-"), uriType->suffix, strlen(uriType->suffix))) {
				gchar *nodeid, *itemid;
				nodeid = strstr(url, "://");
				if(nodeid) {
					nodeid += 3;
					itemid = nodeid;
					itemid = strchr(nodeid, '-');
					if(itemid) {
						nodePtr node;
						itemPtr item;
						
						*itemid = 0;
						itemid++;
						
						node = node_from_id(nodeid);
						if(!node) {
							g_warning("Fatal: no node with id (%s) found!\n", nodeid);
							return;
						}
						
						node_load(node);
						
						item = itemset_lookup_item(node->itemSet, node, atol(itemid));
						if(item)
							(*uriType->func)(item);
						else
							g_warning("Fatal: no item with id (node=%s, item=%s) found!!!", nodeid, itemid);
							
						node_unload(node);
						return;
					}
				}
			}
			uriType++;
		}
		g_warning("Internal error: unhandled protocol in URL \"%s\"!", url);
		return;
	}
	
	if((launchType == UI_HTMLVIEW_LAUNCH_INTERNAL || getBooleanConfValue(BROWSE_INSIDE_APPLICATION)) &&
	   (htmlviewPlugin->launchInsidePossible)() &&
	   (launchType != UI_HTMLVIEW_LAUNCH_EXTERNAL)) {
		(htmlviewPlugin->launch)(htmlview, url);
	} else {
		(void)ui_htmlview_launch_in_external_browser(url);
	}
}

void ui_htmlview_set_zoom(GtkWidget *htmlview, gfloat diff) {

	(htmlviewPlugin->zoomLevelSet)(htmlview, diff); 
}

gfloat ui_htmlview_get_zoom(GtkWidget *htmlview) {

	return (htmlviewPlugin->zoomLevelGet)(htmlview);
}

static gboolean ui_htmlview_external_browser_execute(const gchar *cmd, const gchar *uri, gboolean remoteEscape, gboolean sync) {
	GError		*error = NULL;
	gchar 		*tmpUri, *tmp, **argv, **iter;
	gint 		argc, status;
	gboolean 	done = FALSE;
  
	g_assert(cmd != NULL);
	g_assert(uri != NULL);

	/* If the command is using the X remote API we must
	   escaped all ',' in the URL */
	if(remoteEscape)
		tmpUri = common_strreplace(g_strdup(uri), ",", "%2C");
	else
		tmpUri = g_strdup(uri);

	/* If there is no %s in the command, then just append %s */
	if(strstr(cmd, "%s"))
		tmp = g_strdup(cmd);
	else
		tmp = g_strdup_printf("%s %%s", cmd);
  
	/* Parse and substitute the %s in the command */
	g_shell_parse_argv(tmp, &argc, &argv, &error);
	g_free(tmp);
	if(error && (0 != error->code)) {
		ui_mainwindow_set_status_bar(_("Browser command failed: %s"), error->message);
		debug2(DEBUG_GUI, "Browser command failed: %s : %s", tmp, error->message);
		g_error_free(error);
		return FALSE;
	}
  
	if(argv) {
		for(iter = argv; *iter != NULL; iter++)
			*iter = common_strreplace(*iter, "%s", tmpUri);
	}

	tmp = g_strjoinv(" ", argv);
	debug2(DEBUG_GUI, "Running the browser-remote %s command '%s'", sync ? "sync" : "async", tmp);
	if(sync)
		g_spawn_sync(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &status, &error);
	else
		g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
  
	if(error && (0 != error->code)) {
		debug2(DEBUG_GUI, "Browser command failed: %s : %s", tmp, error->message);
		ui_mainwindow_set_status_bar(_("Browser command failed: %s"), error->message);
		g_error_free(error);
	} else if(status == 0) {
		ui_mainwindow_set_status_bar(_("Starting: \"%s\""), tmp);
		done = TRUE;
	}
  
	g_free(tmpUri);
	g_free(tmp);
	g_strfreev(argv);
  
	return done;
}

gboolean ui_htmlview_launch_in_external_browser(const gchar *uri) {
	struct browser	*browser;
	gchar		*cmd = NULL;
	gboolean	done = FALSE;	
	
	g_assert(uri != NULL);
	
	browser = prefs_get_browser();
	if(browser) {
		/* try to execute synchronously... */
		cmd = prefs_get_browser_command(browser, TRUE /* remote */, FALSE /* fallback */);
		if(cmd) {
			done = ui_htmlview_external_browser_execute(cmd, uri, browser->escapeRemote, TRUE);
			g_free(cmd);
		}
	}
	
	if(done)
		return TRUE;
	
	/* if it failed try to execute asynchronously... */		
	cmd = prefs_get_browser_command(browser, FALSE /* remote */, TRUE /* fallback */);
	if(!cmd) {
		ui_mainwindow_set_status_bar("fatal: cannot retrieve browser command!");
		g_warning("fatal: cannot retrieve browser command!");
		return FALSE;
	}
	done = ui_htmlview_external_browser_execute(cmd, uri, browser?browser->escapeRemote:FALSE, FALSE);
	g_free(cmd);
	return done;
}

gboolean ui_htmlview_scroll(void) {

	return (htmlviewPlugin->scrollPagedown)(ui_mainwindow_get_active_htmlview());
}

void ui_htmlview_set_proxy(gchar *hostname, int port, gchar *username, gchar *password) {

	if(htmlviewPlugin && htmlviewPlugin->setProxy)
		(htmlviewPlugin->setProxy)(hostname, port, username, password);
}

void ui_htmlview_online_status_changed(gboolean online) {

	if(htmlviewPlugin && htmlviewPlugin->setOffLine)
		(htmlviewPlugin->setOffLine)(!online);
}

/* -------------------------------------------------------------------- */
/* htmlview callbacks 							*/
/* -------------------------------------------------------------------- */

void on_popup_launch_link_selected(gpointer url, guint callback_action, GtkWidget *widget) {

	ui_htmlview_launch_URL(ui_tabs_get_active_htmlview(), url, UI_HTMLVIEW_LAUNCH_EXTERNAL);
}

void on_popup_copy_url_selected(gpointer url, guint callback_action, GtkWidget *widget) {
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, url, -1);
 
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, url, -1);
	
	g_free(url);
}

void on_popup_subscribe_url_selected(gpointer url, guint callback_action, GtkWidget *widget) {

	node_request_automatic_add(url, NULL, NULL, NULL, FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);
	g_free(url);
}

void on_popup_zoomin_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	GtkWidget	*htmlview;
	gfloat		zoom;
	
	htmlview = ui_tabs_get_active_htmlview();
	zoom = ui_htmlview_get_zoom(htmlview);
	zoom *= 1.2;
	
	ui_htmlview_set_zoom(htmlview, zoom);
}

void on_popup_zoomout_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	GtkWidget	*htmlview;
	gfloat		zoom;

	htmlview = ui_tabs_get_active_htmlview();	
	zoom = ui_htmlview_get_zoom(htmlview);
	zoom /= 1.2;
	
	ui_htmlview_set_zoom(htmlview, zoom);
}
