/**
 * @file plugin.h Liferea plugin implementation
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

#ifndef _PLUGIN_H
#define _PLUGIN_H

#include <glib.h>
#include <gmodule.h>

#define PLUGIN_ID_MOZILLA_RENDERER	1
#define PLUGIN_ID_GTKHTML2_RENDERER	2

#define PLUGIN_ID_DEFAULT_FEEDLIST	101
#define PLUGIN_ID_OPML_FEEDLIST		102
#define PLUGIN_ID_BLOGLINES_FEEDLIST	103

#define PLUGIN_ID_TRAY_NOTIFICATION	201
#define PLUGIN_ID_POPUP_NOTIFICATION	202

#define PLUGIN_API_VERSION 10

enum {
	PLUGIN_TYPE_HTML_RENDERER,
	PLUGIN_TYPE_PARSER,
	PLUGIN_TYPE_FEEDLIST_PROVIDER,
	PLUGIN_TYPE_NOTIFICATION,
	PLUGIN_TYPE_MAX
};

typedef struct pluginInfo_ pluginInfo;

struct pluginInfo_ {
	unsigned int	api_version;
	gchar 		*name;
	guint		type;			/* plugin type (e.g. renderer, parser or feed list provider) */
	guint		id;			/* unique plugin id */
	//gchar		*description;		/* for plugin managment */
	void		*symbols;		/* plugin type specific symbol table */
};

#define DECLARE_PLUGIN(pi) \
	G_MODULE_EXPORT pluginInfo* plugin_get_info() { \
		return &pi; \
	}

/**
 * Initialize plugin handling.
 */
void plugin_mgmt_init(void);

/**
 * Close/free all resources.
 */
void plugin_mgmt_init(void);

/**
 * Loads all available plugins. Activates them if necessary.
 */
void plugin_mgmt_load(void);

/**
 * Enables the plugin with the given id,
 *
 * @param id	plugin id
 */
void plugin_enable(guint id);

/**
 * Disables the plugin with the given id,
 *
 * @param id	plugin id
 */
void plugin_disable(guint id);

/**
 * Returns TRUE if the plugin with the given id is active, FALSE if not.
 *
 * @param id	plugin id
 *
 * @return TRUE if plugin is enabled
 */
gboolean plugin_get_active(guint id);

#endif
