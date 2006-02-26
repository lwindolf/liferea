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

#include <gmodule.h>
#include <string.h>
#include <libxml/tree.h>
#include "support.h"
#include "debug.h"
#include "node.h"
#include "plugin.h"
#include "fl_providers/fl_plugin.h"

/* plugin managment */

/** list of all loaded plugins */
static GSList *plugins = NULL;

void plugin_mgmt_init(void) {

	if(!g_module_supported())
		g_error(_("Modules not supported! (%s)"), g_module_error());
	
}

void plugin_mgmt_deinit(void) {
	// FIXME
}

typedef	pluginPtr (*infoFunc)();

static pluginPtr plugin_mgmt_load(const gchar * filename) {
	pluginPtr	plugin = NULL;
	GModule		*handle = NULL;
	infoFunc	plugin_get_info;
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
		if(!(plugin = (*plugin_get_info)()))
			return NULL;

		/* check plugin version */
		if(PLUGIN_API_VERSION != plugin->api_version) {
			debug5(DEBUG_PLUGINS, "API version mismatch: \"%s\" (%s, type=%d) has version %d should be %d", plugin->name, filename, plugin->type, plugin->api_version, PLUGIN_API_VERSION);
			return NULL;
		} 

		/* try to load specific plugin type symbols */
		switch(plugin->type) {
			case PLUGIN_TYPE_FEEDLIST_PROVIDER:
				fl_plugin_load(plugin, handle);
				break;
			default:
				debug3(DEBUG_PLUGINS, "Unknown or unsupported plugin type: %s (%s, type=%d)", plugin->name, filename, plugin->type);
				return NULL;
				break;
		}
	} else {
		debug1(DEBUG_PLUGINS, "File %s is no valid Liferea plugin!", filename);
		g_module_close(handle);
	}
	
	return plugin;
}

GSList * plugin_mgmt_get_list(void) {
	guint		filenamelen;
	gchar		*filename;
	pluginPtr	plugin = NULL;
	GError		*error  = NULL;
	GDir		*dir;

	debug_enter("plugin_mgmt_get_list");

	if(NULL == plugins) {
		debug1(DEBUG_PLUGINS, _("Scanning for plugins (%s):"), PACKAGE_LIB_DIR);
		dir = g_dir_open(PACKAGE_LIB_DIR, 0, &error);
		if(!error) {
			/* The expected library name syntax: 

			       libli<type><name>.<library extension> 
			       
			   Examples:  liblihtmlg.so
			              liblihtmlm.so
				      liblifldefault.so
				      libliflopml.so
				      ...  */	
			filenamelen = 5 + strlen(G_MODULE_SUFFIX);
			filename = (gchar *)g_dir_read_name(dir);
			while(NULL != filename) {
				debug1(DEBUG_VERBOSE, "testing %s...", filename);
				if((filenamelen < strlen(filename)) && (0 == strncmp("libli", filename, 5))) {	
				   	/* now lets filter the files with correct library suffix */
					if(0 == strncmp(G_MODULE_SUFFIX, filename + strlen(filename) - strlen(G_MODULE_SUFFIX), strlen(G_MODULE_SUFFIX))) {
						/* If we find one, try to load plugin info and if this
						   was successful try to invoke the specific plugin
						   type loader. If the second loading went well add
						   the plugin to the plugin list. */
						if(!(plugin = plugin_mgmt_load(filename))) {
							debug1(DEBUG_VERBOSE, "-> %s no valid plugin!", filename);
						} else {
							debug3(DEBUG_PLUGINS, "-> %s (%s, type=%d)", plugin->name, filename, plugin->type);
							plugins = g_slist_append(plugins, plugin);
						}
					} else {
						debug0(DEBUG_VERBOSE, "-> no library suffix");
					}
				} else {
					debug0(DEBUG_VERBOSE, "-> prefix does not match");
				}
				filename = (gchar *)g_dir_read_name(dir);
			}
			g_dir_close(dir);
		} else 	{
			g_warning("g_dir_open(%s) failed. Reason: %s\n", PACKAGE_LIB_DIR, error->message );
			g_error_free(error);
			error = NULL;
		}
	}

	g_assert(NULL != plugins);

	debug_exit("plugin_mgmt_get_list");

	return plugins;
}

/* common plugin methods */

void plugin_enable(guint id) {

	// FIXME: set gconf key to true
}

void plugin_disable(guint id) {

	// FIXME: set gconf key to false
}

gboolean plugin_get_active(guint id) {

	// FIXME: return enabled state from gconf
	return TRUE;
}


