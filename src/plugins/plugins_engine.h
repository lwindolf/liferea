/*
 * plugins_engine.h: Liferea Plugins using libpeas
 *
 * Copyright (C) 2012-2024 Lars Windolf <lars.windolf@gmx.de>
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

#define LIFEREA_TYPE_PLUGINS_ENGINE              (liferea_plugins_engine_get_type ())
#define LIFEREA_PLUGINS_ENGINE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), LIFEREA_TYPE_PLUGINS_ENGINE, LifereaPluginsEngine))
#define LIFEREA_PLUGINS_ENGINE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), LIFEREA_TYPE_PLUGINS_ENGINE, LifereaPluginsEngineClass))
#define LIFEREA_IS_PLUGINS_ENGINE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), LIFEREA_TYPE_PLUGINS_ENGINE))
#define LIFEREA_IS_PLUGINS_ENGINE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_TYPE_PLUGINS_ENGINE))
#define LIFEREA_PLUGINS_ENGINE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), LIFEREA_TYPE_PLUGINS_ENGINE, LifereaPluginsEngineClass))
typedef struct _LifereaPluginsEngine LifereaPluginsEngine;
typedef struct _LifereaPluginsEnginePrivate LifereaPluginsEnginePrivate;

struct _LifereaPluginsEngine {
	PeasEngine *parent;
	LifereaPluginsEnginePrivate *priv;
};

typedef struct _LifereaPluginsEngineClass LifereaPluginsEngineClass;

struct _LifereaPluginsEngineClass {
	PeasEngineClass parent_class;
};

GType liferea_plugins_engine_get_type (void) G_GNUC_CONST;

/**
 * liferea_plugins_engine_get: (skip)
 * @shell:		the shell
 * 
 * Get the Liferea plugins engine instance.
 */
LifereaPluginsEngine *liferea_plugins_engine_get (LifereaShell *shell);

/**
 * liferea_plugins_engine_register_shell_plugins: (skip)
 * 
 * Register all plugins that require the shell.
 */
void liferea_plugins_engine_register_shell_plugins (void);

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
