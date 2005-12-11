/**
 * @file fl_plugin.c generic feedlist provider implementation
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
#include "fl_plugin.h"
#include "plugin.h"
#include "debug.h"

flPluginInfo * fl_plugins_get_root(void) {
	gboolean	found = FALSE;
	flPluginInfo	*fpi;
	pluginInfo	*pi;
	GSList		*iter;

	debug_enter("fl_plugins_get_root");

	/* scan for root flag and return plugin if found */
	iter = plugin_mgmt_get_list();
	while(NULL != iter) {
		pi = (pluginInfo *)iter->data;
		if(pi->type == PLUGIN_TYPE_FEEDLIST_PROVIDER) {
			fpi = pi->symbols;
			debug2(DEBUG_VERBOSE, "%s capabilities=%ld", fpi->name, fpi->capabilities);
			if(0 != (fpi->capabilities & FL_PLUGIN_CAPABILITY_IS_ROOT)) {
				found = TRUE;
				break;
			}
			iter = g_slist_next(iter);
		}
	}
	
	if(FALSE == found) 
		g_error("No root capable feed list provider plugin found!");

	debug_exit("fl_plugins_get_root");

	return fpi;
}

typedef	flPluginInfo* (*infoFunc)();

void fl_plugin_load(pluginInfo *pi, GModule *handle) {
	flPluginInfo	*fpi;
	infoFunc	fl_plugin_get_info;

	if(g_module_symbol(handle, "fl_plugin_get_info", (void*)&fl_plugin_get_info)) {
		/* load feed list provider plugin info */
		if(NULL == (fpi = (*fl_plugin_get_info)()))
			return;
	}

	/* check feed list provider plugin version */
	if(FL_PLUGIN_API_VERSION != fpi->api_version) {
		debug3(DEBUG_PLUGINS, "feed list API version mismatch: \"%s\" has version %d should be %d\n", fpi->name, fpi->api_version, FL_PLUGIN_API_VERSION);
		return;
	} 

	/* check if all mandatory symbols are provided */
	if(!((NULL != fpi->plugin_init) &&
	     (NULL != fpi->plugin_deinit) &&
	     (NULL != fpi->node_render)))
		return;

	debug1(DEBUG_PLUGINS, "found feed list plugin: %s", fpi->name);

	/* allow the plugin to initialize */
	(*fpi->plugin_init)();

	/* assign the symbols so the caller will accept the plugin */
	pi->symbols = fpi;
}

void fl_plugin_import(nodePtr np, xmlNodePtr cur) {
	GSList		*iter;
	flPluginInfo	*fpi;
	pluginInfo	*pi;
	xmlChar		*cacheId = NULL, *typeStr = NULL;

	debug_enter("fl_plugin_import");

	if(NULL == (typeStr = xmlGetProp(cur, BAD_CAST"type"))) 
		return;

	debug2(DEBUG_CACHE, "creating feed list plugin instance (type=%s,id=%s)\n", typeStr, np->id);

	/* scan for matching plugin and create new instance */
	iter = plugin_mgmt_get_list();
	while(NULL != iter) {
		pi = (pluginInfo *)iter->data;
		if(pi->type == PLUGIN_TYPE_FEEDLIST_PROVIDER) {
			fpi = pi->symbols;
			if(0 != strcmp(fpi->id, typeStr)) {
				fpi->handler_new(np);
				return;
			}
			iter = g_slist_next(iter);
		}
	}

	debug_exit("flplugin_import");
}

void fl_plugin_export(nodePtr np, xmlNodePtr cur) {

	debug_enter("fl_plugin_export");

	xmlNewProp(cur, BAD_CAST"type", BAD_CAST(FL_PLUGIN(np)->id));

	debug_exit("fl_plugin_export");
}

