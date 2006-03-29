/**
 * @file notif_plugin.c generic notification interface
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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
#include <gtk/gtk.h>
#include <string.h>
#include "debug.h"
#include "node.h"
#include "plugin.h"
#include "support.h"


typedef	notificationPluginPtr (*infoFunc)();

void notification_plugin_load(pluginPtr plugin, GModule *handle) {
	notificationPluginPtr	notificationPlugin;
	infoFunc		notification_plugin_get_info;

	if(g_module_symbol(handle, "notification_plugin_get_info", (void*)&notification_plugin_get_info)) {
		/* load notification provider plugin info */
		if(NULL == (notificationPlugin = (*notification_plugin_get_info)()))
			return;
	}

	/* check notification provider plugin version */
	if(NOTIFICATION_PLUGIN_API_VERSION != notificationPlugin->api_version) {
		debug3(DEBUG_PLUGINS, "notification API version mismatch: \"%s\" has version %d should be %d\n", notificationPlugin->name, notificationPlugin->api_version, NOTIFICATION_PLUGIN_API_VERSION);
		return;
	} 

	/* check if all mandatory symbols are provided */
	if(!(notificationPlugin->plugin_init &&
	     notificationPlugin->plugin_deinit)) {
		debug1(DEBUG_PLUGINS, "mandatory symbols missing: \"%s\"\n", notificationPlugin->name);
		return;
	}

	debug1(DEBUG_PLUGINS, "found notification plugin: %s", notificationPlugin->name);

	/* allow the plugin to initialize */
	(*notificationPlugin->plugin_init)();

	/* assign the symbols so the caller will accept the plugin */
	plugin->symbols = notificationPlugin;
}

