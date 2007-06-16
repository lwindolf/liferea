/**
 * @file dbus.c DBUS interface to control Liferea
 * 
 * Copyright (C) 2007 mooonz <mooonz@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif 

#ifdef USE_DBUS	

#include <dbus/dbus-glib.h>
#include "dbus.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "feedlist.h"

G_DEFINE_TYPE(LifereaDBus, liferea_dbus, G_TYPE_OBJECT)

gboolean liferea_dbus_ping(LifereaDBus *self, gboolean *ret, GError **err) {
	*ret = TRUE;
	return TRUE;
}

gboolean liferea_dbus_set_online(LifereaDBus *self, gboolean online, gboolean *ret, GError **err) {
	update_set_online(online);
	*ret = TRUE;
	return TRUE;
}

gboolean liferea_dbus_subscribe(LifereaDBus *self, gchar *url, gboolean *ret, GError **err) {
	node_request_automatic_add(url, NULL, NULL, NULL, FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);
	*ret = TRUE;
	return TRUE;
}

gboolean liferea_dbus_get_unread_items(LifereaDBus *self, guint *ret, GError **err) {
	*ret = feedlist_get_unread_item_count();
	return TRUE;
}

gboolean liferea_dbus_get_new_items(LifereaDBus *self, guint *ret, GError **err) {
	*ret = feedlist_get_new_item_count();
	return TRUE;
}

#include "dbus_wrap.c"

static void liferea_dbus_init(LifereaDBus *obj) {
}

static void liferea_dbus_class_init(LifereaDBusClass *klass) {
	dbus_g_object_type_install_info (LIFEREA_DBUS_TYPE, &dbus_glib_liferea_dbus_object_info);
}

LifereaDBus* liferea_dbus_new() {
	LifereaDBus *obj = NULL;
	DBusGConnection *bus;
	DBusGProxy *bus_proxy;
	GError *error = NULL;
	guint request_name_result;
	
	bus = dbus_g_bus_get (DBUS_BUS_SESSION, &error);
	if (!bus)
		return NULL;

	bus_proxy = dbus_g_proxy_new_for_name (bus, "org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus");

	if (!dbus_g_proxy_call (bus_proxy, "RequestName", &error,
				G_TYPE_STRING, LF_DBUS_SERVICE, G_TYPE_UINT, 0, G_TYPE_INVALID,
				G_TYPE_UINT, &request_name_result, G_TYPE_INVALID))
		return NULL;

	obj = (LifereaDBus*)g_object_new(LIFEREA_DBUS_TYPE, NULL);
	dbus_g_connection_register_g_object (bus, LF_DBUS_PATH, G_OBJECT (obj));
	
	return obj;
}

#endif /* USE_DBUS */
