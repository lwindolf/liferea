/** 
 * @file notif_libnotify.c notifications via libnotfy
 *
 * Copyright (c) 2004, Karl Soderstrom <ks@xanadunet.net>
 * Copyright (c) 2005, Nathan Conrad <t98502@users.sourceforge.net>
 * Copyright (c) 2006, Norman Jonas <liferea.sf.net@devport.codepilot.net>
 *	      
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
 
#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <glib.h>
#include "conf.h"
#include "node.h"
#include "item.h"
#include "callbacks.h"
#include "support.h"
#include "plugin.h"
#include "ui/ui_feedlist.h"
#include "notification/notif_plugin.h"

#include <libnotify/notify.h>

/* List of all the current notifications */
static GSList *notifications_p = NULL;

/* Function prototypes */

static void notif_libnotify_callback_open ( NotifyNotification *n, const char *action ) {
	g_assert(action != NULL);
	g_assert(strcmp(action, "open") == 0);

//	...

	notify_notification_close(n, NULL);
}

static void notif_libnotify_callback_mark_read ( NotifyNotification *n, const char *action ) {
	g_assert(action != NULL);
	g_assert(strcmp(action, "mark_read") == 0);

//	...

	notify_notification_close(n, NULL);
}

static gboolean notif_libnotify_init(void) {
	if (notify_init ("liferea"))
		return TRUE;
	else
		return FALSE;
		
}

static void notif_libnotify_deinit(void) { }

static void notif_libnotify_enable(void) { }

static void notif_libnotify_disable(void) { }

/*
	The feed has new items - so iterate threw the feed and create a notification
	containing all update news header lines
*/
static void notif_libnotify_node_has_new_items(nodePtr node) {
//	GSList *list_p = NULL;

	NotifyNotification *n;

	n = notify_notification_new ("Feed \"Liferea\" has been updated", 
                                     "<b>Libnotify support</b> Today libnotify support has been added [...]<br>"
		                             "<a href='http://liferea.sf.net'>Visit</a>\nNext headline test",
                                     "stock_news", NULL);
	notify_notification_set_timeout (n, NOTIFY_EXPIRES_DEFAULT);

//	notify_notification_set_category (n, "feed");


/*	braucht eventuell dbus
	DBusConnection *conn;

	conn = dbus_bus_get(DBUS_BUS_SESSION, NULL);
#	loop = g_main_loop_new(NULL, FALSE);
	dbus_connection_setup_with_g_main(conn, NULL);

	notify_notification_add_action(n, "open", "Open feed",
								   (NotifyActionCallback)notif_libnotify_callback_open,
								   NULL, NULL);

	notify_notification_add_action(n, "mark_read", "Mark as read",
								   (NotifyActionCallback)notif_libnotify_callback_mark_read,
								   NULL, NULL);
*/

	if (!notify_notification_show (n, NULL)) {
		fprintf(stderr, "failed to send notification via libnotify\n");
	}

	g_object_unref(G_OBJECT(n));

/*	NotifyNotification *n = notify_notification_new ( "Feed has new / updated topcis", notification, GTK_STOCK_DIALOG_INFO, NULL );
	notify_notification_add_action (n, "open", N_("Open feed"),
		notif_libnotify_callback, NULL, NULL);
	notify_notification_set_timeout (n, 10000);
	notify_notification_show(n, NULL);
*/
}
	
static void notif_libnotify_node_removed(nodePtr node) {
}

/* notification plugin definition */

static struct notificationPlugin npi = {
	NOTIFICATION_PLUGIN_API_VERSION,
	notif_libnotify_init,
	notif_libnotify_deinit,
	notif_libnotify_enable,
	notif_libnotify_disable,
	notif_libnotify_node_has_new_items,
	notif_libnotify_node_removed
};

static struct plugin pi = {
	PLUGIN_API_VERSION,
	"libnotify notification",
	PLUGIN_TYPE_NOTIFICATION,
	//"Implementation of a notification using libnotify.",
	&npi
};

DECLARE_PLUGIN(pi);
DECLARE_NOTIFICATION_PLUGIN(npi);
