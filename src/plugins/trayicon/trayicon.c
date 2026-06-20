/*
 * trayicon.c: Liferea Plugins using libpeas2 + libstray
 *
 * Copyright (C) 2026 Lars Windolf <lars.windolf@gmx.de>
 * 
 * Derived from PeerTube-GTK
 *
 * Copyright (C) 2021-2025 MorsMortium, Erwinjitsu and Other Contributors    
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

#include <libpeas.h>
#include <gtk/gtk.h>

#define STRAY_IMPL 1

#include "stray.h"
#include "../src/liferea_application.h"
#include "../src/ui/liferea_shell.h"
#include "../src/plugins/liferea_activatable.h"
#include "../src/plugins/liferea_shell_activatable.h"

/*
 * Standard gettext macros
 */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  define Q_(String) g_strip_context ((String), gettext (String))
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define ngettext(Singular,Plural,Count) ((Count == 1) ? (Singular) : (Plural))
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define Q_(String) g_strip_context ((String), (String))
#  define N_(String) (String)
#endif

#define TRAYICON_TYPE_PLUGIN (trayicon_plugin_get_type ())

enum {
	PROP_0,
	PROP_SHELL,
	N_PROPS
};

typedef struct {
	GObject parent_instance;

        TrayIcon        *tray;
	GObject         *shell;
	guint            timeout_id;
} TrayiconPlugin;

typedef struct {
	GObjectClass parent_class;
} TrayiconPluginClass;

static void
trayicon_plugin_iface_init (LifereaActivatableInterface *iface);

static void
trayicon_plugin_shell_iface_init (LifereaShellActivatableInterface *iface);

G_DEFINE_FINAL_TYPE_WITH_CODE (TrayiconPlugin, trayicon_plugin, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (LIFEREA_TYPE_ACTIVATABLE, trayicon_plugin_iface_init)
                               G_IMPLEMENT_INTERFACE (LIFEREA_TYPE_SHELL_ACTIVATABLE, trayicon_plugin_shell_iface_init))

static void
trayicon_remove (TrayiconPlugin *plugin)
{
	if (plugin->timeout_id) {
		g_source_remove (plugin->timeout_id);
		plugin->timeout_id = 0;
	}
	if (plugin->tray) {
        	stray_destroy (plugin->tray);
		plugin->tray = NULL;
	}
}

static void
trayicon_plugin_finalize (GObject *object)
{
	TrayiconPlugin *plugin = (TrayiconPlugin *)object;

	trayicon_remove (plugin);
	g_clear_object (&plugin->shell);
	G_OBJECT_CLASS (trayicon_plugin_parent_class)->finalize (object);
}

static void
trayicon_plugin_init (TrayiconPlugin *plugin)
{
}

static void
trayicon_plugin_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
	TrayiconPlugin *plugin = (TrayiconPlugin *)object;

	switch (prop_id) {
		case PROP_SHELL:
			g_value_set_object (value, plugin->shell);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
trayicon_plugin_set_property (GObject      *object,
                              guint         prop_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
	TrayiconPlugin *plugin = (TrayiconPlugin *)object;

	switch (prop_id) {
		case PROP_SHELL:
			g_set_object (&plugin->shell, g_value_get_object (value));
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
trayicon_plugin_class_init (TrayiconPluginClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = trayicon_plugin_finalize;
	object_class->get_property = trayicon_plugin_get_property;
	object_class->set_property = trayicon_plugin_set_property;

	g_object_class_install_property (object_class,
	                                 PROP_SHELL,
	                                 g_param_spec_object ("shell",
	                                                       "Shell",
	                                                       "Liferea shell instance",
	                                                       LIFEREA_SHELL_TYPE,
	                                                       G_PARAM_READWRITE));
}

static void
on_toggle_visibility (G_GNUC_UNUSED int id, gpointer userdata)
{
	TrayiconPlugin *plugin = (TrayiconPlugin *)userdata;
	gboolean visible = FALSE;

	g_object_get (G_OBJECT (plugin->shell), "visibility", &visible, NULL);
	g_object_set (G_OBJECT (plugin->shell), "visibility", !visible, NULL);
}

static void
on_quit (G_GNUC_UNUSED int id, G_GNUC_UNUSED void *userdata)
{
        liferea_application_shutdown ();
}

static void
on_button_cb (G_GNUC_UNUSED TrayButton button, G_GNUC_UNUSED int x, G_GNUC_UNUSED int y, void *userdata)
{
	on_toggle_visibility (0, userdata);
}

/*static void
on_close_checkbox_changed (G_GNUC_UNUSED int id, G_GNUC_UNUSED void *userdata)
{
}*/

gboolean
TrayEvents(gpointer Data)
{
	// Process events the tray icon received
	stray_process_events((TrayIcon *) Data);

	return G_SOURCE_CONTINUE;
}

static void
trayicon_activate (LifereaActivatable *activatable)
{
	TrayiconPlugin *plugin = (TrayiconPlugin *)activatable;
	TrayMenu *menu;

        plugin->tray = stray_create ("Liferea", "emblem-web.svg", "Liferea");
	stray_set_status (plugin->tray, STRAY_STATUS_ACTIVE);

	// Create tray menu and add to icon
	menu = stray_menu_create ();
	stray_set_menu (plugin->tray, menu);

	// Try to register and exit on failure (systems with no tray icon support)
	if (stray_register (plugin->tray) == 0) {
		return;
	}

	stray_menu_add_item (menu, _("Show / Hide"), on_toggle_visibility, plugin);
	//stray_menu_add_check_item (menu, _("Minimize to tray on close"), on_close_checkbox_changed, plugin);
	//stray_menu_set_item_checked(menu, id, checked);
	stray_menu_add_item (menu, _("Quit"), on_quit, NULL);
	stray_set_button_callback (plugin->tray, on_button_cb, plugin);

	plugin->timeout_id = g_timeout_add (100, TrayEvents, plugin->tray);

	// Always force show window upon activation to avoid hidden window on startup
	g_object_set (plugin->shell, "visibility", TRUE, NULL);
}

static void
trayicon_deactivate (LifereaActivatable *activatable)
{
	TrayiconPlugin *plugin = (TrayiconPlugin *)activatable;

	trayicon_remove (plugin);
}

static void
trayicon_plugin_iface_init (LifereaActivatableInterface *iface)
{
	iface->activate = trayicon_activate;
	iface->deactivate = trayicon_deactivate;
}

static void
trayicon_plugin_shell_iface_init (LifereaShellActivatableInterface *iface)
{
}

G_MODULE_EXPORT void
_lp_trayicon_register_types (PeasObjectModule *module)
{
        peas_object_module_register_extension_type (module, LIFEREA_TYPE_SHELL_ACTIVATABLE, TRAYICON_TYPE_PLUGIN);
}