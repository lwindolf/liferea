/*
 * plugins_engine.h: Liferea Plugins using libpeas
 *
 * Copyright (C) 2012-2025 Lars Windolf <lars.windolf@gmx.de>
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
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _PLUGINS_ENGINE
#define _PLUGINS_ENGINE

#include <libpeas.h>

#include "ui/liferea_shell.h"

G_BEGIN_DECLS

#define LIFEREA_TYPE_PLUGINS_ENGINE (liferea_plugins_engine_get_type ())
G_DECLARE_FINAL_TYPE (LifereaPluginsEngine, liferea_plugins_engine, LIFEREA, PLUGINS_ENGINE, GObject)

GType liferea_plugins_engine_get_type (void) G_GNUC_CONST;

/**
 * liferea_plugins_engine_get: (skip)
 * 
 * Get the Liferea plugins engine instance.
 */
LifereaPluginsEngine *liferea_plugins_engine_get (void);

/**
 * liferea_plugins_engine_register_shell_plugins: (skip)
 * @shell:		the shell
 * 
 * Register all plugins that require the shell.
 */
void liferea_plugins_engine_register_shell_plugins (LifereaShell *shell);

/**
 * liferea_plugin_call: (skip)
 * @type:		the type of the plugin interface
 * @func:		the function to call
 * @user_data:		some user data (or NULL) 
 */
void liferea_plugin_call (GType type, GFunc func, gpointer user_data);

/**
 * liferea_plugin_is_active: (skip)
 * @type:		the type of the plugin interface
 * 
 * Returns: TRUE if at least one plugin of the type is active
 */
gboolean liferea_plugin_is_active (GType type);

G_END_DECLS

#endif
