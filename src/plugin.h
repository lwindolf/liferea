/**
 * @file plugin.h Liferea plugin implementation
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

#ifndef _PLUGIN_H
#define _PLUGIN_H

#include <glib.h>
#include <gmodule.h>

/* Liferea functionality can be extended by plugins loaded
   on startup. Currently there are a few plugin types
   listed below.

   The plugin management interface allows initial loading
   of Liferea plugins as well as enabling/disabling of 
   single plugins. */
    
#define PLUGIN_API_VERSION 10

enum {
	PLUGIN_TYPE_HTML_RENDERER,
	PLUGIN_TYPE_PARSER,
	PLUGIN_TYPE_FEEDLIST_PROVIDER,
	PLUGIN_TYPE_NOTIFICATION,
	PLUGIN_TYPE_SCRIPT_SUPPORT,
	PLUGIN_TYPE_MAX
};

typedef struct plugin {
	guint		api_version;
	gchar 		*name;
	guint		type;			/* plugin type (e.g. renderer, parser or feed list provider) */
	//gchar		*description;		/* for plugin managment */
	void		*symbols;		/* plugin type specific symbol table */
} *pluginPtr;

#define DECLARE_PLUGIN(plugin) \
	G_MODULE_EXPORT pluginPtr plugin_get_info() { \
		return &plugin; \
	}

/**
 * Initialize plugin handling.
 */
void plugin_mgmt_init(void);

/**
 * Close/free all resources.
 */
void plugin_mgmt_deinit(void);

/**
 * Returns the list of available plugins.
 *
 * @returns a list of all feed list provider plugins
 */
GSList * plugin_mgmt_get_list(void);

#endif
