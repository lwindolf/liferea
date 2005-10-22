/**
 * @file plugin.c Liferea plugin implementation
 * 
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
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
#include "support.h"
#include "debug.h"
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

typedef	pluginInfo* (*infoFunc)();

static pluginInfo * plugin_mgmt_load(gchar * filename) {
	pluginInfo	*pi = NULL;
	GModule		*handle = NULL;
	infoFunc	plugin_get_info;

#if GLIB_CHECK_VERSION(2,3,3)
	if((handle = g_module_open(filename, G_MODULE_BIND_LOCAL)) == NULL) {
#else
	if((handle = g_module_open(filename, 0)) == NULL) {
#endif
		debug1(DEBUG_PLUGINS, "Cannot open %s!\n", filename);
		return NULL;
	}

	if(g_module_symbol(handle, "plugin_get_info", (void*)&plugin_get_info)) {
		/* load generic plugin info */
		if(NULL == (pi = (*plugin_get_info)()))
			return NULL;

		/* try to load specific plugin type symbols */
		if(PLUGIN_API_VERSION != pi->api_version) {
			debug6(DEBUG_PLUGINS, "API version mismatch: %s (%s, type=%d, id=%d) has version %d should be %d\n", pi->name, filename, pi->type, pi->id, pi->api_version, PLUGIN_API_VERSION);
			return NULL;
		} 

		switch(pi->type) {
			case PLUGIN_TYPE_FEEDLIST_PROVIDER:
				fl_plugin_load(pi, handle);
				break;
			default:
				debug4(DEBUG_PLUGINS, "Unknown or unsupported plugin type: %s (%s, type=%d, id=%d)\n", pi->name, filename, pi->type, pi->id);
				return NULL;
				break;
		}
	} else {
		debug1(DEBUG_PLUGINS, "File %s is no valid Liferea plugin!\n", filename);
		g_module_close(handle);
	}
	
	return pi;
}

GSList * plugin_mgmt_get_list(void) {
	guint		filenamelen;
	gchar		*filename;
	pluginInfo	*pi = NULL;
	GError		*error  = NULL;
	GDir		*dir;

	if(NULL == plugins) {
		debug1(DEBUG_PLUGINS, _("Scanning for plugins (%s):\n"), PACKAGE_LIB_DIR);
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
				if((filenamelen < strlen(filename)) && (0 == strncmp("libli", filename, 5))) {	
				   	/* now lets filter the files with correct library suffix */
					if(0 == strncmp(G_MODULE_SUFFIX, filename - strlen(G_MODULE_SUFFIX), strlen(G_MODULE_SUFFIX))) {
						/* If we find one, try to load plugin info and if this
						   was successful try to invoke the specific plugin
						   type loader. If the second loading went well add
						   the plugin to the plugin list. */
						if(NULL == (pi = plugin_mgmt_load(filename))) {
							debug1(DEBUG_PLUGINS, "-> %s no valid plugin!\n", filename);
						} else {
							debug4(DEBUG_PLUGINS, "-> %s (%s, type=%d, id=%d)\n", pi->name, filename, pi->type, pi->id);
							plugins = g_slist_append(plugins, pi);
						}
					}
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
