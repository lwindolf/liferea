/*
 * plugins_engine.c: Liferea Plugins using libpeas
 * (derived from gtranslator code)
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2010 Steve Frécinaux
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <gio/gio.h>
#include <girepository.h>
#include <libpeas.h>

#include "auth_activatable.h"
#include "download_activatable.h"
#include "node_source_activatable.h"
#include "liferea_activatable.h"
#include "liferea_shell_activatable.h"
#include "plugins_engine.h"

struct _LifereaPluginsEnginePrivate
{
  GSettings	*plugin_settings;
  LifereaShell	*shell;			/*<< shell needs to be passed to all plugins */
  GHashTable	*extension_sets;	/*<< hash table of extension sets we might want to call */
};

G_DEFINE_TYPE_WITH_CODE (LifereaPluginsEngine, liferea_plugins_engine, PEAS_TYPE_ENGINE, G_ADD_PRIVATE (LifereaPluginsEngine))

static LifereaPluginsEngine *engine = NULL;

static void
liferea_plugins_engine_init (LifereaPluginsEngine *engine)
{
	gchar		*typelib_dir;
	const gchar	**names;
	gsize		length;
	GError		*error = NULL;

	g_autoptr(GVariant)	vlist;
	g_autoptr(GStrvBuilder)	b;

	engine->priv = liferea_plugins_engine_get_instance_private (engine);
	engine->priv->plugin_settings = g_settings_new ("net.sf.liferea.plugins");
	engine->priv->extension_sets = g_hash_table_new (g_direct_hash, g_direct_equal);

	b = g_strv_builder_new ();
	vlist = g_settings_get_value (engine->priv->plugin_settings, "active-plugins");
	names = g_variant_get_strv (vlist, &length);

	/* Disable incompatible plugins */
	const gchar *incompatible[] = {
		"webkit-settings",
		NULL
	};
	for (guint i = 0; i < length; i++) {
		if (!g_strv_contains (incompatible, names[i]))
			g_strv_builder_add (b, names[i]);
	}

	/* Safe modified settings */
	GStrv list = g_strv_builder_end (b);
	g_settings_set_strv (engine->priv->plugin_settings, "active-plugins", (const gchar *const *)list);
	g_strfreev (list);
	g_free (names);

	/* Only load libpeas after we cleaned the 'active-plugins' setting */
	peas_engine_enable_loader (PEAS_ENGINE (engine), "python3");
	peas_engine_enable_loader (PEAS_ENGINE (engine), "gjs");

	/* Require Lifereas's typelib. */
	typelib_dir = g_build_filename (PACKAGE_LIB_DIR,
					"girepository-1.0", NULL);

	if (!g_irepository_require_private (g_irepository_get_default (),
		typelib_dir, "Liferea", "3.0", 0, &error)) {
		g_warning ("Could not load Liferea repository: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	g_free (typelib_dir);

	/* This should be moved to libpeas */
	if (!g_irepository_require (g_irepository_get_default (),
				"Peas", "2.0", 0, &error)) {
		g_warning ("Could not load Peas repository: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	if (!g_irepository_require (g_irepository_get_default (),
				"PeasGtk", "1.0", 0, &error)) {
		g_warning ("Could not load PeasGtk repository: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	g_autofree gchar *data = g_build_filename (PACKAGE_DATA_DIR, "plugins", NULL);
	g_autofree gchar *lib = g_build_filename (PACKAGE_LIB_DIR, "plugins", NULL);
	peas_engine_add_search_path (PEAS_ENGINE (engine), data, data);
	peas_engine_add_search_path (PEAS_ENGINE (engine), lib, data);
	peas_engine_rescan_plugins (PEAS_ENGINE (engine));

	/* Load mandatory plugins */
	const gchar *mandatory[] = {
		"download-manager",
		"plugin-installer"
	};
	for (guint i = 0; i < G_N_ELEMENTS (mandatory); i++) {
		PeasPluginInfo *info = peas_engine_get_plugin_info (PEAS_ENGINE (engine), mandatory[i]);
		if (info)
			peas_engine_load_plugin (PEAS_ENGINE (engine), info);
		else
			g_warning ("The plugin-installer plugin was not found.");
	}
}

/* Provide default signal handlers */

static void
on_extension_added (PeasExtensionSet   *extensions,
                    PeasPluginInfo     *info,
		    LifereaActivatable *plugin,
		    gpointer           user_data)
{
	liferea_activatable_activate (plugin);
}

static void
on_extension_removed (PeasExtensionSet   *extensions,
                      PeasPluginInfo     *info,
                      LifereaActivatable *plugin,
		      gpointer           user_data)
{
	liferea_activatable_deactivate (plugin);
}

typedef struct {
	GFunc func;
	gpointer user_data;
} callCtxt;

static void
liferea_plugin_call_foreach (PeasExtensionSet	*set,
                             PeasPluginInfo	*info,
                             LifereaActivatable	*plugin,
                             gpointer		user_data)
{
	callCtxt *ctxt = (callCtxt *)user_data;
	((GFunc)ctxt->func)(plugin, ctxt->user_data);
}

void
liferea_plugin_call (GType type, GFunc func, gpointer user_data)
{
	PeasExtensionSet *set =g_hash_table_lookup (engine->priv->extension_sets, (gpointer)type);

	g_assert (set);

	callCtxt ctxt;
	ctxt.func = func;
	ctxt.user_data = user_data;
	peas_extension_set_foreach (set, (PeasExtensionSetForeachFunc)liferea_plugin_call_foreach, &ctxt);
}

gboolean
liferea_plugin_is_active (GType type)
{
	PeasExtensionSet *set = g_hash_table_lookup (engine->priv->extension_sets, GINT_TO_POINTER(type));

	return g_list_model_get_n_items (G_LIST_MODEL (set)) > 0;
}

static void
liferea_plugins_engine_dispose (GObject * object)
{
	LifereaPluginsEngine *engine = LIFEREA_PLUGINS_ENGINE (object);

	if (engine->priv->plugin_settings) {
		g_object_unref (engine->priv->plugin_settings);
		engine->priv->plugin_settings = NULL;
	}
	if (engine->priv->extension_sets) {
		g_hash_table_destroy (engine->priv->extension_sets);
		engine->priv->extension_sets = NULL;
	}

	G_OBJECT_CLASS (liferea_plugins_engine_parent_class)->dispose (object);
}

static void
liferea_plugins_engine_class_init (LifereaPluginsEngineClass * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->dispose = liferea_plugins_engine_dispose;
}

LifereaPluginsEngine *
liferea_plugins_engine_get (LifereaShell *shell)
{
	if (!engine) {
		engine = LIFEREA_PLUGINS_ENGINE (g_object_new (LIFEREA_TYPE_PLUGINS_ENGINE, NULL));
		engine->priv->shell = shell;
		g_object_add_weak_pointer (G_OBJECT (engine), (gpointer) &engine);

		/* Immediately register basic non-GUI plugin intefaces that might be requirement
		   for everything to come up. All other plugins are registered later on
		   using liferea_plugins_engine_register_shell_plugins() */
		GType types[] = {
			LIFEREA_AUTH_ACTIVATABLE_TYPE,
			LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE
		};

		for (guint i = 0; i < G_N_ELEMENTS (types); i++) {
			PeasExtensionSet *extensions = peas_extension_set_new (PEAS_ENGINE (engine), types[i], NULL);
			g_hash_table_insert (engine->priv->extension_sets, GINT_TO_POINTER(types[i]), extensions);

			peas_extension_set_foreach (extensions, (PeasExtensionSetForeachFunc)on_extension_added, NULL);

			g_signal_connect (extensions, "extension-added", G_CALLBACK (on_extension_added), NULL);
			g_signal_connect (extensions, "extension-removed", G_CALLBACK (on_extension_removed), NULL);
		}
	}

	return engine;
}

void
liferea_plugins_engine_register_shell_plugins (void) 
{
	GType types[] = {
		LIFEREA_TYPE_SHELL_ACTIVATABLE,
		LIFEREA_TYPE_DOWNLOAD_ACTIVATABLE
	};

	for (guint i = 0; i < G_N_ELEMENTS (types); i++) {
		/* Note: we expect all plugins to get property 'shell' as the default entrypoint */
		PeasExtensionSet *extensions = peas_extension_set_new (PEAS_ENGINE (engine), types[i], "shell", engine->priv->shell, NULL);
		g_hash_table_insert (engine->priv->extension_sets, GINT_TO_POINTER(types[i]), extensions);

		peas_extension_set_foreach (extensions, (PeasExtensionSetForeachFunc)on_extension_added, NULL);

		g_signal_connect (extensions, "extension-added", G_CALLBACK (on_extension_added), NULL);
		g_signal_connect (extensions, "extension-removed", G_CALLBACK (on_extension_removed), NULL);
	}
}
