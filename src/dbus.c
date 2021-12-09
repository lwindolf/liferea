/**
 * @file dbus.c DBUS interface to control Liferea
 * 
 * Copyright (C) 2007 mooonz <mooonz@users.sourceforge.net>
 * Copyright (C) 2010 Emilio Pozuelo Monfort <pochu27@gmail.com>
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

#include "dbus.h"
#include "debug.h"
#include "feedlist.h"
#include "net_monitor.h"
#include "subscription.h"
#include "ui/liferea_shell.h"

static GDBusNodeInfo *introspection_data = NULL;

static const gchar introspection_xml[] =
"<node name='/org/gnome/feed/Reader'>"
"  <interface name='org.gnome.feed.Reader'>"
"    <method name='Ping'>"
"      <arg name='result' type='b' direction='out' />"
"    </method>"
"    <method name='SetOnline'>"
"      <arg name='online' type='b' />"
"      <arg name='result' type='b' direction='out' />"
"    </method>"
"    <method name='Subscribe'>"
"      <arg name='url' type='s' />"
"      <arg name='result' type='b' direction='out' />"
"    </method>"
"    <method name='GetUnreadItems'>"
"      <arg name='result' type='i' direction='out' />"
"    </method>"
"    <method name='GetNewItems'>"
"      <arg name='result' type='i' direction='out' />"
"    </method>"
"    <method name='Refresh'>"
"      <arg name='result' type='b' direction='out' />"
"    </method>"
"  </interface>"
"</node>";

G_DEFINE_TYPE(LifereaDBus, liferea_dbus, G_TYPE_OBJECT)

static gboolean
liferea_dbus_ping (LifereaDBus *self, GError **err)
{
	return TRUE;
}

static gboolean
liferea_dbus_set_online (LifereaDBus *self, gboolean online, GError **err)
{
	network_monitor_set_online (online);
	return TRUE;
}

static gboolean
liferea_dbus_subscribe (LifereaDBus *self, const gchar *url, GError **err)
{
	liferea_shell_show_window ();
	feedlist_add_subscription (url, NULL, NULL, 0);
	return TRUE;
}

static guint
liferea_dbus_get_unread_items (LifereaDBus *self, GError **err)
{
	return feedlist_get_unread_item_count ();
}

static guint
liferea_dbus_get_new_items (LifereaDBus *self, GError **err)
{
	return feedlist_get_new_item_count ();
}

static gboolean
liferea_dbus_refresh (LifereaDBus *self, GError **err)
{
	node_update_subscription (feedlist_get_root (), GUINT_TO_POINTER (0));
	return TRUE;
}

static void
handle_method_call (GDBusConnection       *connection,
		    const gchar           *sender,
		    const gchar           *object_path,
		    const gchar           *interface_name,
		    const gchar           *method_name,
		    GVariant              *parameters,
		    GDBusMethodInvocation *invocation,
		    gpointer               user_data)
{
	LifereaDBus *self = user_data;
	gboolean res;

	if (g_str_equal (method_name, "Ping")) {
		res = liferea_dbus_ping (self, NULL);
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(b)", res));
	} else if (g_str_equal (method_name, "SetOnline") &&
	    g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(b)"))) {
		gboolean set_online;
		g_variant_get (parameters, "(b)", &set_online);
		res = liferea_dbus_set_online (self, set_online, NULL);
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(b)", res));
	} else if (g_str_equal (method_name, "Subscribe") &&
	    g_variant_is_of_type (parameters, G_VARIANT_TYPE ("(s)"))) {
		const gchar *url;
		g_variant_get (parameters, "(s)", &url);
		res = liferea_dbus_subscribe (self, url, NULL);
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(b)", res));
	} else if (g_str_equal (method_name, "GetUnreadItems")) {
		guint num = liferea_dbus_get_unread_items (self, NULL);
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(i)", num));
	} else if (g_str_equal (method_name, "GetNewItems")) {
		guint num = liferea_dbus_get_new_items (self, NULL);
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(i)", num));
	} else if (g_str_equal (method_name, "Refresh")) {
		res = liferea_dbus_refresh (self, NULL);
		g_dbus_method_invocation_return_value (invocation,
			g_variant_new ("(b)", res));
	} else {
		g_warning ("Unknown method name or unknown parameters: %s",
			   method_name);
	}
}

static const GDBusInterfaceVTable interface_vtable =
{
  handle_method_call,
  NULL,
  NULL
};

static void
on_bus_acquired (GDBusConnection *connection,
		 const gchar     *name,
		 gpointer         user_data)
{
	guint id;

	debug_enter ("on_bus_acquired");

	/* parse introspection data */
	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml,
							   NULL);

	id = g_dbus_connection_register_object (connection,
						LF_DBUS_PATH,
						introspection_data->interfaces[0],
						&interface_vtable,
						NULL,  /* user_data */
						NULL,  /* user_data_free_func */
						NULL); /* GError** */

	g_assert (id > 0);

	debug_exit ("on_bus_acquired");
}

static void
on_name_acquired (GDBusConnection *connection,
		  const gchar     *name,
		  gpointer         user_data)
{
	debug1 (DEBUG_GUI, "Acquired the name %s on the session bus\n", name);
}

static void
on_name_lost (GDBusConnection *connection,
	      const gchar     *name,
	      gpointer         user_data)
{
	debug1 (DEBUG_GUI, "Lost the name %s on the session bus\n", name);
}

static void liferea_dbus_init(LifereaDBus *obj) { }

static void
liferea_dbus_dispose (GObject *obj)
{
	LifereaDBus *self = LIFEREA_DBUS (obj);

	g_bus_unown_name (self->owner_id);

	G_OBJECT_CLASS (liferea_dbus_parent_class)->dispose (obj);
}

static void
liferea_dbus_class_init (LifereaDBusClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS (klass);

	gobject_class->dispose = liferea_dbus_dispose;
}

LifereaDBus*
liferea_dbus_new (void)
{
	LifereaDBus *obj = NULL;

	debug_enter ("liferea_dbus_new");

	obj = (LifereaDBus*)g_object_new(LIFEREA_DBUS_TYPE, NULL);

	obj->owner_id = g_bus_own_name (G_BUS_TYPE_SESSION,
					LF_DBUS_SERVICE,
					G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT,
					on_bus_acquired,
					on_name_acquired,
					on_name_lost,
					NULL,
					NULL);

	debug_exit ("liferea_dbus_new");

	return obj;
}
