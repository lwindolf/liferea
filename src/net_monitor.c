/*
 * @file net_monitor.c  network status monitor
 *
 * Copyright (C) 2009-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "net_monitor.h"

#include <gio/gio.h>

#include "conf.h"
#include "debug.h"
#include "net.h"

struct NetworkMonitorPrivate {
	GDBusConnection *conn;
	guint		subscription_id;

	gboolean	online;
};

G_DEFINE_TYPE_WITH_CODE (NetworkMonitor, network_monitor, G_TYPE_OBJECT, G_ADD_PRIVATE (NetworkMonitor))

enum {
	ONLINE_STATUS_CHANGED,
	PROXY_CHANGED,
	LAST_SIGNAL
};

static guint network_monitor_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;
static NetworkMonitor *network_monitor = NULL;


static void
network_monitor_finalize (GObject *object)
{
	NetworkMonitor *self = NETWORK_MONITOR (object);

	debug0 (DEBUG_NET, "network manager: unregistering network state change callback");

	if (self->priv->conn && self->priv->subscription_id) {
		g_dbus_connection_signal_unsubscribe (self->priv->conn,
						      self->priv->subscription_id);
	}

	network_deinit ();

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
network_monitor_class_init (NetworkMonitorClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = network_monitor_finalize;

	network_monitor_signals [ONLINE_STATUS_CHANGED] =
		g_signal_new ("online-status-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__BOOLEAN,
		G_TYPE_NONE,
		1,
		G_TYPE_BOOLEAN);

	network_monitor_signals [PROXY_CHANGED] =
		g_signal_new ("proxy-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0);
}

static gboolean is_nm_connected (guint state)
{
	gboolean intranet;

	conf_get_bool_value (INTRANET_CONNECTIVITY, &intranet);

	/* We support both intranet without internet connectivity
	   and normal internet use cases (see Github #1121). */
	if ((state == 60 && intranet) || /* NM_STATE_CONNECTED_SITE (intranet use case)*/
		 state == 70)                /* NM_STATE_CONNECTED_GLOBAL (default) */
		return TRUE;
	return FALSE;
}

static void
on_network_state_changed_cb (GDBusConnection *connection,
			     const gchar *sender_name,
			     const gchar *object_path,
			     const gchar *interface_name,
			     const gchar *signal_name,
			     GVariant *parameters,
			     gpointer user_data)
{
	gboolean online = network_monitor_is_online ();
	guint state;

	g_variant_get (parameters, "(u)", &state);

	if (online && !is_nm_connected (state)) {
		debug0 (DEBUG_NET, "network manager: no network connection -> going offline");
		network_monitor_set_online (FALSE);
	} else if (!online && is_nm_connected (state)) {
		debug0 (DEBUG_NET, "network manager: active connection -> going online");
		network_monitor_set_online (TRUE);
	}
}

void
network_monitor_set_online (gboolean mode)
{
	if (network_monitor->priv->online != mode) {
		network_monitor->priv->online = mode;
		debug1 (DEBUG_NET, "Changing online mode to %s", mode?"online":"offline");
		g_signal_emit (network_monitor, network_monitor_signals[ONLINE_STATUS_CHANGED], 0, mode);
	}
}

gboolean
network_monitor_is_online (void)
{
	if (!network_monitor)
		return FALSE;

	return network_monitor->priv->online;
}

void
network_monitor_proxy_changed (void)
{
	if (!network_monitor)
		return;

	g_signal_emit (network_monitor, network_monitor_signals[PROXY_CHANGED], 0, NULL);
}

static void
on_bus_get_cb (GObject *source_object, GAsyncResult *result, gpointer user_data)
{
	NetworkMonitor *self = NETWORK_MONITOR (user_data);
	GError *error = NULL;

	self->priv->conn = g_bus_get_finish (result, &error);
	if (!self->priv->conn) {
		debug1 (DEBUG_NET, "Could not connect to system bus: %s", error->message);
		g_error_free (error);
		return;
	}

	g_dbus_connection_set_exit_on_close (self->priv->conn, FALSE);

	debug0 (DEBUG_NET, "network manager: connecting to StateChanged signal");
	self->priv->subscription_id = g_dbus_connection_signal_subscribe (self->priv->conn,
									  "org.freedesktop.NetworkManager",
									  "org.freedesktop.NetworkManager",
									  "StateChanged",
									  NULL,
									  NULL,
									  G_DBUS_SIGNAL_FLAGS_NONE,
									  on_network_state_changed_cb,
									  self,
									  NULL);

	debug1 (DEBUG_NET, "network manager: connected to StateChanged signal: %s",
		self->priv->subscription_id ? "yes" : "no");
}

static void
network_monitor_init (NetworkMonitor *nm)
{
	nm->priv = network_monitor_get_instance_private (nm);
	nm->priv->online = TRUE;

	/* For now accessing the network monitor also sets up the network! */
	network_init ();

	g_bus_get (G_BUS_TYPE_SYSTEM, NULL, on_bus_get_cb, nm);
}

NetworkMonitor *
network_monitor_get (void)
{
	if (G_UNLIKELY (!network_monitor))
		network_monitor = NETWORK_MONITOR (g_object_new (NETWORK_MONITOR_TYPE, NULL));

	return network_monitor;
}
