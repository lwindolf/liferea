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

#include <libnotify/notify.h>

#include "conf.h"
#include "node.h"
#include "item.h"
#include "callbacks.h"
#include "support.h"
#include "plugin.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_node.h"
#include "notification/notif_plugin.h"

static void notif_libnotify_callback_open ( NotifyNotification *n, const gchar *action, gpointer user_data ) {
	g_assert(action != NULL);
	g_assert(strcmp(action, "open") == 0);

	fprintf(stderr, "PLUGIN:libnotify - callback\n");

	nodePtr node_p = user_data;
	ui_feedlist_select(node_p);

	notify_notification_close(n, NULL);
}

static void notif_libnotify_callback_mark_read ( NotifyNotification *n, const char *action, gpointer user_data ) {
	g_assert(action != NULL);
	g_assert(strcmp(action, "mark_read") == 0);

	nodePtr node_p = user_data;
	node_mark_all_read(node_p);

	notify_notification_close(n, NULL);
}

static gboolean notif_libnotify_init(void) {
	if (notify_init ("liferea")) {
		return TRUE;
}
	else {
		fprintf(stderr, "PLUGIN:notif_libnotify.c : notify_init failed" );
		return FALSE;
	}
}

static void notif_libnotify_deinit(void) {
	notify_uninit();
}

static void notif_libnotify_enable(void) { }

static void notif_libnotify_disable(void) { }

/*
	The feed has new items - so iterate threw the feed and create a notification
	containing all update news header lines
*/
static void notif_libnotify_node_has_new_items(nodePtr node_p) {

	GList *list_p;
	itemPtr item_p;

	gchar *labelText_p;
	gchar *labelText_now_p;
	gchar *labelText_prev_p;

	gchar *labelHeadline_p;
	gchar *labelURL_p;

	gint item_count = 0;

	labelText_now_p = g_strdup_printf ("");

	/* Gather the new feed's headlines */
	list_p = node_p->itemSet->items;
	while(list_p != NULL) {
		item_p = list_p->data;
		if( item_p->popupStatus == TRUE) {
			item_p->popupStatus = FALSE;
			item_count += 1;

			labelHeadline_p = g_strdup_printf (item_get_title(item_p));
			if (labelHeadline_p == NULL ) {
				labelHeadline_p = g_strdup_printf ("This news entry has no headline" );
			}

			labelURL_p = item_get_base_url(item_p);
			if (labelURL_p != NULL ) {
				labelText_p = g_strdup_printf ("%s <a href='%s'>Visit</a>\n", labelHeadline_p, labelURL_p );
			} else {
				labelText_p = g_strdup_printf ("%s\n", labelHeadline_p );
			}

			labelText_prev_p = labelText_now_p;
			labelText_now_p = g_strconcat( labelText_now_p, labelText_p, NULL);

			g_free(labelHeadline_p);
			g_free(labelText_p);
			g_free(labelText_prev_p);
		}
		list_p = g_list_next(list_p);
	}

	NotifyNotification *n;

	n = notify_notification_new (node_get_title(node_p), labelText_now_p, NULL, NULL);

	if ( node_p->icon != NULL ) {
		notify_notification_set_icon_from_pixbuf (n,node_p->icon);
	}

/*
	Give the user a second for every headline before notification disappears
*/
	notify_notification_set_timeout (n, item_count * 1000 );

	notify_notification_add_action(n, "open", "Open feed",
									(NotifyActionCallback)notif_libnotify_callback_open,
									node_p, NULL);
	notify_notification_add_action(n, "mark_read", "Mark all as read",
									(NotifyActionCallback)notif_libnotify_callback_mark_read,
									node_p, NULL);
//	notify_notification_set_category (n, "feed");


	if (!notify_notification_show (n, NULL)) {
		fprintf(stderr, "PLUGIN:libnotify - failed to send notification via libnotify\n");
	}

	g_free(labelText_now_p);
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
