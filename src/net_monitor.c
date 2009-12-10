/**
 * @file network_monitor.c  network status monitor
 *
 * Copyright (C) 2009 Lars Lindner <lars.lindner@gmail.com>
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

#ifdef USE_NM
#include <dbus/dbus.h>
#include <libnm_glib.h>
#endif

#include "debug.h"
#include "net.h"

static void network_monitor_class_init	(NetworkMonitorClass *klass);
static void network_monitor_init	(NetworkMonitor *nm);

#define NETWORK_MONITOR_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), NETWORK_MONITOR_TYPE, NetworkMonitorPrivate))

struct NetworkMonitorPrivate {
	gboolean		online;
	
#ifdef USE_NM
	/* State for NM support */
	libnm_glib_ctx		*nm_ctx;
	guint			nm_id;
#endif
};

enum {
	ONLINE_STATUS_CHANGED,
	PROXY_CHANGED,
	LAST_SIGNAL
};

static guint network_monitor_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;
static NetworkMonitor *network_monitor = NULL;

GType
network_monitor_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (NetworkMonitorClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) network_monitor_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (NetworkMonitor),
			0, /* n_preallocs */
			(GInstanceInitFunc) network_monitor_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "NetworkMonitor",
					       &our_info, 0);
	}

	return type;
}

static void
network_monitor_finalize (GObject *object)
{
#ifdef USE_NM	
	debug0 (DEBUG_NET, "network manager: unregistering network state change callback");
	
	if (nm_id != 0 && nm_ctx != NULL) {
		libnm_glib_unregister_callback (nm_ctx, nm_id);
		libnm_glib_shutdown (nm_ctx);
		nm_ctx = NULL;
		nm_id = 0;
	}
#endif	
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

	g_type_class_add_private (object_class, sizeof (NetworkMonitorPrivate));
}


#ifdef USE_NM
static void
nm_state_changed (libnm_glib_ctx *ctx, gpointer user_data)
{
	libnm_glib_state	state;
	gboolean online;

	g_return_if_fail (ctx != NULL);

	state = libnm_glib_get_network_state (ctx);
	online = network_monitor_is_online ();

	if (priv->online && state == LIBNM_NO_NETWORK_CONNECTION) {
		debug0 (DEBUG_NET, "network manager: no network connection -> going offline");
		network_monitor_set_online (FALSE);
	} else if (!online && state == LIBNM_ACTIVE_NETWORK_CONNECTION) {
		debug0 (DEBUG_NET, "network manager: active connection -> going online");
		network_monitor_set_online (TRUE);
	}
}

static gboolean
nm_initialize (void)
{
	debug0 (DEBUG_NET, "network manager: registering network state change callback");
	
	if (!nm_ctx) {
		nm_ctx = libnm_glib_init ();
		if (!nm_ctx) {
			g_warning ("Could not initialize libnm.");
			return FALSE;
		}	
	}

	nm_id = libnm_glib_register_callback (nm_ctx, nm_state_changed, NULL, NULL);
	
	return TRUE;
}

#endif

void
network_monitor_set_online (gboolean mode)
{
	if (network_monitor->priv->online != mode) {
		network_monitor->priv->online = mode;
		debug1 (DEBUG_NET, "Changing online mode to %s", mode?"online":"offline");
		g_signal_emit_by_name (network_monitor, "online-status-changed", mode);
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
		
	g_signal_emit_by_name (network_monitor, "proxy-changed", NULL);
}

static void
network_monitor_init (NetworkMonitor *nm)
{
	nm->priv = NETWORK_MONITOR_GET_PRIVATE (nm);
	nm->priv->online = TRUE;

	/* For now accessing the network monitor also sets up the network! */
	network_init ();
	
	/* But it needs to be performed after the DBUS setup or else the 
	   following won't work... */
#ifdef USE_NM
	{
		DBusConnection *connection;

		connection = dbus_bus_get (DBUS_BUS_SYSTEM, NULL);

		if (connection) {
			dbus_connection_set_exit_on_disconnect (connection, FALSE);

			if (dbus_bus_name_has_owner (connection, "org.freedesktop.NetworkManager", NULL)) {
				nm_initialize ();
				/* network manager will set online state right after initialization... */
			}

			dbus_connection_unref(connection);
		}
	}
#endif
}

NetworkMonitor *
network_monitor_get (void)
{
	if (!network_monitor)
		network_monitor = NETWORK_MONITOR (g_object_new (NETWORK_MONITOR_TYPE, NULL));	
		
	return network_monitor;
}
