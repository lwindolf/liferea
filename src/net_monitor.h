/*
 * @file net_monitor.h  network status monitor
 *
 * Copyright (C) 2009-2022 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _NETWORK_MONITOR_H
#define _NETWORK_MONITOR_H

#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct NetworkMonitor		NetworkMonitor;
typedef struct NetworkMonitorClass	NetworkMonitorClass;
typedef struct NetworkMonitorPrivate	NetworkMonitorPrivate;

struct NetworkMonitor {
	GObject parent;
	
	/*< private >*/
	NetworkMonitorPrivate	*priv;
};

struct NetworkMonitorClass {
	GObjectClass parent;
};

GType network_monitor_get_type (void);

#define NETWORK_MONITOR_TYPE              (network_monitor_get_type ())
#define NETWORK_MONITOR(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), NETWORK_MONITOR_TYPE, NetworkMonitor))
#define NETWORK_MONITOR_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), NETWORK_MONITOR_TYPE, NetworkMonitorClass))
#define IS_NETWORK_MONITOR(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), NETWORK_MONITOR_TYPE))
#define IS_NETWORK_MONITOR_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), NETWORK_MONITOR_TYPE))
#define NETWORK_MONITOR_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), NETWORK_MONITOR_TYPE, NetworkMonitorClass))

/**
 * network_monitor_get: (skip)
 * 
 * Returns the network monitor object. Creates it if
 * necessary first.
 *
 * @returns the network monitor
 */
NetworkMonitor* network_monitor_get (void);

/**
 * network_monitor_set_online:
 * 
 * Sets the online status according to mode.
 *
 * @param mode	TRUE for online, FALSE for offline
 */ 
void network_monitor_set_online (gboolean mode);

/**
 * network_monitor_is_online:
 * 
 * Queries the online status.
 *
 * @return TRUE if online
 */
gboolean network_monitor_is_online (void);

/**
 * network_monitor_proxy_changed: (skip)
 * 
 * Called by networking when proxy was changed.
 */
void network_monitor_proxy_changed (void);

#endif
