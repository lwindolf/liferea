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
#include "ui/liferea_dialog.h"

struct _LifereaPluginsEngine
{
	GObject parent_instance;

	PeasEngine	*engine;
	GHashTable	*extension_sets;	/*<< hash table of extension sets we might want to call */
	PeasExtensionSet	*all;		/*<< all plugins (extension set of type LifereaActivatable) */
};

G_DEFINE_TYPE (LifereaPluginsEngine, liferea_plugins_engine, G_TYPE_OBJECT)

static LifereaPluginsEngine *plugins = NULL;

static void
liferea_plugins_engine_init (LifereaPluginsEngine *plugins)
{
	g_autofree gchar	*typelib_dir = NULL;
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
		"plugin-installer",
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
	typelib_dir = g_build_filename (PACKAGE_LIB_DIR, GI_REPOSITORY, NULL);
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

	g_settings_bind (plugin_settings,
			"active-plugins",
			plugins->engine, "loaded-plugins", G_SETTINGS_BIND_DEFAULT);

	/* Load mandatory plugins */
	const gchar *mandatory[] = {
		"download-manager"
	};
	for (guint i = 0; i < G_N_ELEMENTS (mandatory); i++) {
		PeasPluginInfo *info = peas_engine_get_plugin_info (PEAS_ENGINE (plugins->engine), mandatory[i]);
		if (info)
			peas_engine_load_plugin (PEAS_ENGINE (plugins->engine), info);
		else
			g_warning ("The plugin-installer plugin was not found.");
	}

	plugins->all = peas_extension_set_new (plugins->engine, LIFEREA_TYPE_ACTIVATABLE, NULL);
}

/* Provide default signal handlers */

static void
on_extension_added (PeasExtensionSet   *extensions,
                    PeasPluginInfo     *info,
		    LifereaActivatable *plugin,
		    gpointer           user_data)
{
	debug (DEBUG_GUI, "Plugin added %s", peas_plugin_info_get_name (info));
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

static void
on_plugin_checkbox_toggled(GtkToggleButton *button, PeasPluginInfo *info)
{
	gboolean is_active = gtk_toggle_button_get_active(button);
	if (is_active) {
		peas_engine_load_plugin(plugins->engine, info);
	} else {
		peas_engine_unload_plugin(plugins->engine, info);
	}
}

static void
on_row_selected (GtkListBox *list_box, GtkListBoxRow *selected_row, gpointer user_data)
{
	GtkWidget *configure_button = liferea_dialog_lookup (GTK_WIDGET (user_data), "configure");
	PeasPluginInfo *info = g_object_get_data (G_OBJECT (selected_row), "plugin-info");
	GObject *plugin = peas_extension_set_get_extension (plugins->all, info);

	gtk_widget_set_sensitive(configure_button, 
		plugin
		&& info
		&& LIFEREA_IS_ACTIVATABLE (plugin)
		&& (LIFEREA_ACTIVATABLE_GET_IFACE (plugin)->create_configure_widget != NULL)
		&& peas_plugin_info_is_loaded (info));
}

static void
on_configure_clicked (GtkButton *button, gpointer user_data)
{
	GtkListBox *list_box = GTK_LIST_BOX (liferea_dialog_lookup (GTK_WIDGET (user_data), "plugins_listbox"));
	GtkListBoxRow *selected_row = gtk_list_box_get_selected_row (list_box);
	PeasPluginInfo *info = g_object_get_data (G_OBJECT (selected_row), "plugin-info");
	GObject *plugin = peas_extension_set_get_extension (plugins->all, info);

	liferea_activatable_create_configure_widget (LIFEREA_ACTIVATABLE (plugin));
}

void
liferea_plugins_manage_dialog (GtkWindow *parent)
{
	GListModel *plugin_infos = G_LIST_MODEL(peas_engine_get_default ());
	GtkWidget *dialog = liferea_dialog_new ("plugins");
	GtkWidget *list_box = liferea_dialog_lookup (dialog, "plugins_listbox");
	GtkWidget *configure_button = liferea_dialog_lookup (dialog, "configure");

	for (guint i = 0; i < g_list_model_get_n_items(plugin_infos); i++) {
		PeasPluginInfo *info = PEAS_PLUGIN_INFO(g_list_model_get_item(plugin_infos, i));
		const gchar *plugin_name = peas_plugin_info_get_name(info);
		const gchar *plugin_desc = peas_plugin_info_get_description(info);
		gboolean is_active = peas_plugin_info_is_loaded(info);

		GtkWidget *row = gtk_list_box_row_new();
		GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
		GtkWidget *active_checkbox = gtk_check_button_new();
		gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(active_checkbox), is_active);
		GtkWidget *text_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
		GtkWidget *label_name = gtk_label_new(NULL);
		gtk_label_set_markup(GTK_LABEL(label_name), g_markup_printf_escaped("<b>%s</b>", plugin_name));
		gtk_label_set_xalign(GTK_LABEL(label_name), 0.0);
		GtkWidget *label_desc = gtk_label_new(plugin_desc);
		gtk_label_set_xalign(GTK_LABEL(label_desc), 0.0);

		gtk_box_append(GTK_BOX(text_box), label_name);
		gtk_box_append(GTK_BOX(text_box), label_desc);
		gtk_box_append(GTK_BOX(box), active_checkbox);
		gtk_box_append(GTK_BOX(box), text_box);
		gtk_list_box_append(GTK_LIST_BOX(list_box), row);
		g_signal_connect(active_checkbox, "toggled", G_CALLBACK(on_plugin_checkbox_toggled), info);

		g_object_set_data_full(G_OBJECT(row), "plugin-info", g_object_ref(info), g_object_unref);
	}

	g_signal_connect(list_box, "row-selected", G_CALLBACK(on_row_selected), dialog);
	g_signal_connect(configure_button, "clicked", G_CALLBACK(on_configure_clicked), dialog);

	gtk_widget_show(list_box);
	gtk_widget_show(dialog);
}

