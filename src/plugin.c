/**
 * @file plugin.c Liferea plugin implementation
 * 
 * Copyright (C) 2005-2006 Lars Lindner <lars.lindner@gmx.net>
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
#else
#  define LIBPREFIX "lib"
#endif

#include <gmodule.h>
#include <string.h>
#include <libxml/tree.h>

#include "debug.h"
#include "node.h"
#include "plugin.h"
#include "ui/ui_htmlview.h"
#include "fl_sources/node_source.h"
#include "notification/notif_plugin.h"

/* plugin managment */

/** list of all loaded plugins */
static GSList *plugins = NULL;

typedef	pluginPtr (*infoFunc)();

static pluginPtr plugin_mgmt_load(const gchar * filename) {
	pluginPtr	plugin = NULL;
	GModule		*handle = NULL;
	infoFunc	plugin_get_info;
	gboolean	success = FALSE;
	gchar		*path;

	path = g_strdup_printf(PACKAGE_LIB_DIR G_DIR_SEPARATOR_S "%s", filename);

#if GLIB_CHECK_VERSION(2,3,3)
	handle = g_module_open(path, G_MODULE_BIND_LOCAL);
#else
	handle = g_module_open(path, 0);
#endif

	g_free(path);

	if(!handle) {
		debug3(DEBUG_PLUGINS, "Cannot open %s%s (%s)!", PACKAGE_LIB_DIR G_DIR_SEPARATOR_S, filename, g_module_error());
		return NULL;
	}

	if(g_module_symbol(handle, "plugin_get_info", (void*)&plugin_get_info)) {
		/* load generic plugin info */
		if(NULL != (plugin = (*plugin_get_info)())) {
			/* check plugin version */
			if(PLUGIN_API_VERSION != plugin->api_version)
				debug5(DEBUG_PLUGINS, "API version mismatch: \"%s\" (%s, type=%d) has version %d should be %d", plugin->name, filename, plugin->type, plugin->api_version, PLUGIN_API_VERSION);

			/* try to load specific plugin type symbols */
			switch(plugin->type) {
				case PLUGIN_TYPE_NOTIFICATION:
					success = notification_plugin_register (plugin, handle);
					break;
				case PLUGIN_TYPE_HTML_RENDERER:
					success = liferea_htmlview_plugin_register (plugin, handle);
					break;
				default:
					if(plugin->type >= PLUGIN_TYPE_MAX) {
						debug3(DEBUG_PLUGINS, "Unknown or unsupported plugin type: %s (%s, type=%d)", plugin->name, filename, plugin->type);
					} else {
						success = TRUE;		/* no special initialization */
					}
					break;
			}
		}
	} else {
		debug1(DEBUG_PLUGINS, "File %s is no valid Liferea plugin!", filename);
	}
	
	if(!success) {
		g_module_close(handle);
		return NULL;
	}
		
	return plugin;
}

void plugin_mgmt_init(void) {
	guint		filenamelen;
	gchar		*filename;
	pluginPtr	plugin = NULL;
	GError		*error  = NULL;
	GDir		*dir;

	debug_enter("plugin_mgmt_get_init");

	if(!g_module_supported())
		g_error("Modules not supported! (%s)", g_module_error());

	debug1(DEBUG_PLUGINS, "Scanning for plugins (%s):", PACKAGE_LIB_DIR);
	dir = g_dir_open(PACKAGE_LIB_DIR, 0, &error);
	if(!error) {
		/* The expected library name syntax: 

		       <LIBPREFIX>li<type><name>.<library extension> 

		   Examples:  liblihtmlg.so
			      liblihtmlm.so
			      liblifldefault.so
			      libliflopml.so
			      ...  */	
		filenamelen = 5 + strlen(G_MODULE_SUFFIX);
		filename = (gchar *)g_dir_read_name(dir);
		while(filename) {
			if(DEBUG_VERBOSE & debug_level)
				debug1(DEBUG_PLUGINS, "testing %s...", filename);
			if((filenamelen < strlen(filename)) && (0 == strncmp(LIBPREFIX "li", filename, 5))) {	
				/* now lets filter the files with correct library suffix */
				if(!strncmp(G_MODULE_SUFFIX, filename + strlen(filename) - strlen(G_MODULE_SUFFIX), strlen(G_MODULE_SUFFIX))) {
					/* If we find one, try to load plugin info and if this
					   was successful try to invoke the specific plugin
					   type loader. If the second loading went well add
					   the plugin to the plugin list. */
					if(!(plugin = plugin_mgmt_load(filename))) {
						if(DEBUG_VERBOSE & debug_level)
							debug1(DEBUG_PLUGINS, "-> %s no valid plugin!", filename);
					} else {
						debug3(DEBUG_PLUGINS, "-> %s (%s, type=%d)", plugin->name, filename, plugin->type);
						plugins = g_slist_append(plugins, plugin);
					}
				} else {
					if(DEBUG_VERBOSE & debug_level)
						debug0(DEBUG_PLUGINS, "-> no library suffix");
				}
			} else {
				if(DEBUG_VERBOSE & debug_level)
					debug0(DEBUG_PLUGINS, "-> prefix does not match");
			}
			filename = (gchar *)g_dir_read_name(dir);
		}
		g_dir_close(dir);
	} else 	{
		g_warning("g_dir_open(%s) failed. Reason: %s\n", PACKAGE_LIB_DIR, error->message );
		g_error_free(error);
		error = NULL;
	}

	g_assert(NULL != plugins);
	
	/* do plugin type specific startup init */
	liferea_htmlview_plugin_init ();
	notification_plugin_init ();

	debug_exit("plugin_mgmt_init");
}

void plugin_mgmt_deinit(void) { }

GSList * plugin_mgmt_get_list(void) { return plugins; }

