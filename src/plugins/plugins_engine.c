/*
 * plugins_engine.c: Liferea Plugins using libpeas2
 *
 * Copyright (C) 2002-2005 Paolo Maggi
 * Copyright (C) 2010 Steve Frécinaux
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

#include <config.h>

#include <string.h>

#include <glib/gi18n.h>
#include <glib.h>
#include <gio/gio.h>
#include <girepository.h>
#include <libpeas.h>

#include "auth_activatable.h"
#include "debug.h"
#include "download_activatable.h"
#include "node_source_activatable.h"
#include "liferea_activatable.h"
#include "liferea_shell_activatable.h"
#include "plugins_engine.h"

struct _LifereaPluginsEngine
{
	GObject parent_instance;

	PeasEngine	*engine;
	GHashTable	*extension_sets;	/*<< hash table of extension sets we might want to call */
};

G_DEFINE_TYPE (LifereaPluginsEngine, liferea_plugins_engine, G_TYPE_OBJECT)

static LifereaPluginsEngine *plugins = NULL;

static void
liferea_plugins_engine_init (LifereaPluginsEngine *plugins)
{
	g_autofree gchar	*typelib_dir;
	const gchar		**names;
	gsize			length;
	GError			*error = NULL;

	g_autoptr(GSettings)	plugin_settings = g_settings_new ("net.sf.liferea.plugins");
	g_autoptr(GVariant)	vlist;
	g_autoptr(GStrvBuilder)	b;

	debug (DEBUG_GUI, "Initializing plugins engine");

	plugins->extension_sets = g_hash_table_new (g_direct_hash, g_direct_equal);
	plugins->engine = peas_engine_get_default ();
	g_object_add_weak_pointer (G_OBJECT (plugins), (gpointer) &plugins->engine);

	b = g_strv_builder_new ();
	vlist = g_settings_get_value (plugin_settings, "active-plugins");
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
	g_settings_set_strv (plugin_settings, "active-plugins", (const gchar *const *)list);
	g_strfreev (list);
	g_free (names);

	/* Only load libpeas after we cleaned the 'active-plugins' setting */
	peas_engine_enable_loader (PEAS_ENGINE (plugins->engine), "python");
	peas_engine_enable_loader (PEAS_ENGINE (plugins->engine), "gjs");

	/* Require Lifereas's typelib. */
	typelib_dir = g_build_filename (PACKAGE_LIB_DIR, "girepository-1.0", NULL);
	if (!g_irepository_require_private (g_irepository_get_default (),
		typelib_dir, "Liferea", "3.0", 0, &error)) {
		g_warning ("Could not load Liferea repository: %s", error->message);
		g_error_free (error);
		error = NULL;
	}

	g_autofree gchar *userdata = g_build_filename (g_get_user_data_dir (), "liferea", "plugins", NULL);
	g_autofree gchar *data = g_build_filename (PACKAGE_DATA_DIR, "plugins", NULL);
	g_autofree gchar *lib = g_build_filename (PACKAGE_LIB_DIR, "plugins", NULL);

        peas_engine_add_search_path (PEAS_ENGINE (plugins->engine), userdata, userdata);
	peas_engine_add_search_path (PEAS_ENGINE (plugins->engine), lib, data);
	peas_engine_rescan_plugins (PEAS_ENGINE (plugins->engine));

	/* Load mandatory plugins */
	const gchar *mandatory[] = {
		"download-manager",
		"plugin-installer"
	};
	for (guint i = 0; i < G_N_ELEMENTS (mandatory); i++) {
		PeasPluginInfo *info = peas_engine_get_plugin_info (PEAS_ENGINE (plugins->engine), mandatory[i]);
		if (info)
			peas_engine_load_plugin (PEAS_ENGINE (plugins->engine), info);
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
	PeasExtensionSet *set = g_hash_table_lookup (plugins->extension_sets, (gpointer)type);

	g_assert (set);

	callCtxt ctxt;
	ctxt.func = func;
	ctxt.user_data = user_data;
	peas_extension_set_foreach (set, (PeasExtensionSetForeachFunc)liferea_plugin_call_foreach, &ctxt);
}

gboolean
liferea_plugin_is_active (GType type)
{
	PeasExtensionSet *set = g_hash_table_lookup (plugins->extension_sets, GINT_TO_POINTER(type));

	return g_list_model_get_n_items (G_LIST_MODEL (set)) > 0;
}

static void
liferea_plugins_engine_dispose (GObject * object)
{
	LifereaPluginsEngine *plugins = LIFEREA_PLUGINS_ENGINE (object);

	if (plugins->extension_sets) {
		g_hash_table_destroy (plugins->extension_sets);
		plugins->extension_sets = NULL;
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
liferea_plugins_engine_get (void)
{
	if (!plugins) {
		plugins = LIFEREA_PLUGINS_ENGINE (g_object_new (LIFEREA_TYPE_PLUGINS_ENGINE, NULL));

		/* Immediately register basic non-GUI plugin intefaces that might be requirement
		   for everything to come up. All other plugins are registered later on
		   using liferea_plugins_engine_register_shell_plugins() */
		GType types[] = {
			LIFEREA_AUTH_ACTIVATABLE_TYPE,
			LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE
		};

		debug (DEBUG_GUI, "Registering basic plugins");

		for (guint i = 0; i < G_N_ELEMENTS (types); i++) {
			PeasExtensionSet *extensions = peas_extension_set_new (PEAS_ENGINE (plugins->engine), types[i], NULL);
			g_hash_table_insert (plugins->extension_sets, GINT_TO_POINTER(types[i]), extensions);

			peas_extension_set_foreach (extensions, (PeasExtensionSetForeachFunc)on_extension_added, NULL);

			g_signal_connect (extensions, "extension-added", G_CALLBACK (on_extension_added), NULL);
			g_signal_connect (extensions, "extension-removed", G_CALLBACK (on_extension_removed), NULL);
		}
	}

	return plugins;
}

void
liferea_plugins_engine_register_shell_plugins (LifereaShell *shell) 
{
	GType types[] = {
		LIFEREA_TYPE_SHELL_ACTIVATABLE,
		LIFEREA_TYPE_DOWNLOAD_ACTIVATABLE
	};

	g_assert (plugins);

	debug (DEBUG_GUI, "Registering shell plugins");

	for (guint i = 0; i < G_N_ELEMENTS (types); i++) {
		/* Note: we expect all plugins to get property 'shell' as the default entrypoint */
		PeasExtensionSet *extensions = peas_extension_set_new (PEAS_ENGINE (plugins->engine), types[i], "shell", shell, NULL);
		g_hash_table_insert (plugins->extension_sets, GINT_TO_POINTER(types[i]), extensions);

		peas_extension_set_foreach (extensions, (PeasExtensionSetForeachFunc)on_extension_added, NULL);

		g_signal_connect (extensions, "extension-added", G_CALLBACK (on_extension_added), NULL);
		g_signal_connect (extensions, "extension-removed", G_CALLBACK (on_extension_removed), NULL);
	}
}
