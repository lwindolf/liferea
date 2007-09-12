/**
 * @file notif_plugin.c generic notification interface
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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
#include "common.h"
#include "debug.h"
#include "node.h"
#include "plugin.h"
#include "notification/notif_plugin.h"

static GSList *notificationPlugins = NULL;

typedef	notificationPluginPtr (*infoFunc)();

gboolean
notification_plugin_register (pluginPtr plugin, GModule *handle)
{
	notificationPluginPtr	notificationPlugin = NULL;
	infoFunc		notification_plugin_get_info;
	GSList 			*iter;

	if (g_module_symbol (handle, "notification_plugin_get_info", (void*)&notification_plugin_get_info)) {
		/* load notification provider plugin info */
		if (NULL == (notificationPlugin = (*notification_plugin_get_info) ()))
			return FALSE;
	}

	/* check notification provider plugin version */
	if (NOTIFICATION_PLUGIN_API_VERSION != notificationPlugin->api_version) {
		debug3 (DEBUG_PLUGINS, "notification API version mismatch: \"%s\" has version %d should be %d", plugin->name, notificationPlugin->api_version, NOTIFICATION_PLUGIN_API_VERSION);
		return FALSE;
	} 

	/* check if all mandatory symbols are provided */
	if (!(notificationPlugin->plugin_init &&
	      notificationPlugin->plugin_deinit)) {
		debug1 (DEBUG_PLUGINS, "mandatory symbols missing: \"%s\"", plugin->name);
		return FALSE;
	}

	/* add plugin to notification plugin instance list */
	notificationPlugins = g_slist_append (notificationPlugins, plugin);

	/* assign the symbols so the caller will accept the plugin */
	plugin->symbols = notificationPlugin;
	
	return TRUE;
}

static void
notification_plugin_init_for_type (notificationType type)
{
	GSList			*iter;
	notificationPluginPtr	selectedPlugin = NULL;
	
	/* Check for already loaded plugin of same type and with higher priority */
	iter = notificationPlugins;
	while (iter) {
		notificationPluginPtr tmp = ((pluginPtr)iter->data)->symbols;

		if (tmp->type == type) {
			if (!selectedPlugin || (tmp->priority > selectedPlugin->priority))
				selectedPlugin = tmp;
		}
		iter = g_slist_next (iter);
	}
	
	/* Allow the plugin to initialize */
	if (selectedPlugin) {
		if ((*selectedPlugin->plugin_init) ()) {
			debug2 (DEBUG_PLUGINS, "using \"%s\" for notification type %d", selectedPlugin->name, selectedPlugin->type);
		} else {
			debug1 (DEBUG_PLUGINS, "notification plugin \"%s\" did not load succesfully", selectedPlugin->name);
		}
	}
}

void
notification_plugin_init (void)
{
	notification_plugin_init_for_type (NOTIFICATION_TYPE_POPUP);
	notification_plugin_init_for_type (NOTIFICATION_TYPE_TRAY);
}

void notification_enable(gboolean enabled) {
	notificationPluginPtr	plugin;
	GSList 			*iter;

	iter = notificationPlugins;
	while(iter) {
		plugin = ((pluginPtr)iter->data)->symbols;
		(*plugin->notification_enable)();
		iter = g_slist_next(iter);
	}
}

void notification_node_has_new_items(nodePtr node) { 
	notificationPluginPtr	plugin;
	GSList 			*iter;
	
	iter = notificationPlugins;
	while(iter) {
		plugin = ((pluginPtr)iter->data)->symbols;
		(*plugin->node_has_new_items)(node);
		iter = g_slist_next(iter);
	}
}

void notification_node_removed(nodePtr node) { 
	notificationPluginPtr	plugin;
	GSList 			*iter;
	
	iter = notificationPlugins;
	while(iter) {
		plugin = ((pluginPtr)iter->data)->symbols;
		(*plugin->node_removed)(node);
		iter = g_slist_next(iter);
	}
}
