/*
 * plugins_engine.h: Liferea Plugins using libpeas
 *
 * Copyright (C) 2012-2015 Lars Windolf <lars.windolf@gmx.de>
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

#include <libpeas/peas-engine.h>
#include <libpeas/peas-extension-set.h>

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
	PeasEngine parent;
	LifereaPluginsEnginePrivate *priv;
};

typedef struct _LifereaPluginsEngineClass LifereaPluginsEngineClass;

struct _LifereaPluginsEngineClass {
	PeasEngineClass parent_class;
};

GType liferea_plugins_engine_get_type (void) G_GNUC_CONST;

LifereaPluginsEngine *liferea_plugins_engine_get_default (void);

/**
 * liferea_plugins_engine_set_default_signals:
 *
 * Set up default "activate" and "deactivate" signals.
 *
 * @extensions:		the extensions set
 * @user_data:		some user data (or NULL)
 */
void liferea_plugins_engine_set_default_signals (PeasExtensionSet *extensions, gpointer user_data);

G_END_DECLS

#endif
