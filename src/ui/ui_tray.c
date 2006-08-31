/**
 * @file ui_tray.c tray icon handling
 * 
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Christophe Barbe <christophe.barbe@ufies.org>
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

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "conf.h"
#include "callbacks.h"
#include "eggtrayicon.h"
#include "support.h"
#include "feedlist.h"
#include "ui_tray.h"
#include "ui_popup.h"
#include "ui_mainwindow.h"
#include "update.h"

#define NO_NEW_ITEMS	0
#define NEW_ITEMS	1

extern GdkPixbuf	*icons[];

extern GtkWidget	*mainwindow;

static GtkWidget	*eventbox = NULL;
static EggTrayIcon 	*tray_icon =  NULL;
static GtkTooltips	*tray_icon_tips = NULL;
static GtkWidget	*image = NULL;		/* the image in the notification area */
static int trayCount = 0;
static GdkPixbuf *currentIcon = NULL;

static void installTrayIcon(void);

void ui_tray_tooltip_set(gchar *message) {
	GtkTooltipsData	*data = NULL;
	
	g_assert(tray_icon_tips);

	data = gtk_tooltips_data_get(GTK_WIDGET(tray_icon));

	if(NULL != data) {
		/* FIXME: Is this necessary? Gtk 2.2.4 frees these strings
		   inside of set_tip, if needed. */
		g_free(data->tip_text);
		g_free(data->tip_private);
		data->tip_text = NULL;
		data->tip_private = NULL;
	}
	
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tray_icon_tips), 
					 GTK_WIDGET(eventbox),
					 message, message);
}

static void ui_tray_icon_set(GdkPixbuf *icon) {

	g_assert(tray_icon);
	
	/* Skip loading icon if already displayed. */
	if (icon == currentIcon)
		return;
	currentIcon = icon;
	
	if(NULL != image)
		gtk_widget_destroy(image);

	image = gtk_image_new_from_pixbuf(icon);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	gtk_widget_show_all(GTK_WIDGET(tray_icon));
}

void ui_tray_update(void) {
	gint	newItems, unreadItems;
	gchar	*msg, *tmp;
	
	if(!tray_icon)
		return;

	newItems = feedlist_get_new_item_count();
	unreadItems = feedlist_get_unread_item_count();
		
	if(newItems != 0) {
		if(update_is_online()) ui_tray_icon_set(icons[ICON_AVAILABLE]);
		else ui_tray_icon_set(icons[ICON_AVAILABLE_OFFLINE]);
		msg = g_strdup_printf(ngettext("%d new item", "%d new items", newItems), newItems);
	} else {
		if(update_is_online()) ui_tray_icon_set(icons[ICON_EMPTY]);
		else ui_tray_icon_set(icons[ICON_EMPTY_OFFLINE]);
		msg = g_strdup(_("No new items"));
	}

	if(unreadItems != 0) {
		tmp = g_strdup_printf(ngettext("%s\n%d unread item", "%s\n%d unread items", unreadItems), msg, unreadItems);
	} else {
		tmp = g_strdup_printf(_("%s\nNo unread items"), msg);
	}

	ui_tray_tooltip_set(tmp);
	g_free(tmp);
	g_free(msg);
}

/* a click on the systray icon should show the program window
   if invisible or hide it if visible */
static void tray_icon_pressed(GtkWidget *button, GdkEventButton *event, EggTrayIcon *icon) {
	
	switch(event->button) {
	case 1:
		ui_mainwindow_toggle_visibility(NULL, NULL);
		break;
	case 3:
		gtk_menu_popup(ui_popup_make_systray_menu(), NULL, NULL, NULL, NULL, event->button, event->time);
		break;
	}
						  
	return;
}

static gboolean ui_tray_create_cb() {

	installTrayIcon();
	
	return FALSE; /* for when we're called by the glib idle handler */
}


static void ui_tray_embedded_cb(GtkWidget *widget, void *data) {

	ui_mainwindow_tray_add();
}


static void ui_tray_destroyed_cb(GtkWidget *widget, void *data) {

	g_object_unref(G_OBJECT(tray_icon));

	image = NULL;
	tray_icon = NULL;
	trayCount--;
	ui_mainwindow_tray_remove();
	
	/* And make it re-appear when the notification area reappears */
	g_idle_add(ui_tray_create_cb, NULL);
	
}

static void installTrayIcon(void) {

	g_assert(NULL == tray_icon);

	tray_icon = egg_tray_icon_new(PACKAGE);
	eventbox = gtk_event_box_new();
	
	g_signal_connect(eventbox, "button_press_event", G_CALLBACK(tray_icon_pressed), tray_icon);
	g_signal_connect(G_OBJECT(tray_icon), "embedded", G_CALLBACK(ui_tray_embedded_cb), NULL);
	g_signal_connect(G_OBJECT(tray_icon), "destroy", G_CALLBACK(ui_tray_destroyed_cb), NULL);
	
	ui_dnd_setup_URL_receiver(eventbox);
	
	gtk_container_add(GTK_CONTAINER(tray_icon), eventbox);
	g_object_ref(G_OBJECT(tray_icon));
	
	tray_icon_tips = gtk_tooltips_new();
	ui_tray_update();
	trayCount++;
}

static void removeTrayIcon() {

	g_assert(tray_icon != NULL);
	
	g_signal_handlers_disconnect_by_func(G_OBJECT(tray_icon), G_CALLBACK(ui_tray_destroyed_cb), NULL);
	gtk_widget_destroy(image);
	image = NULL;
	g_object_unref(G_OBJECT(tray_icon));
	gtk_object_destroy(GTK_OBJECT (tray_icon));
	tray_icon = NULL;
	currentIcon = NULL;
	trayCount--;
	ui_mainwindow_tray_remove();
}

void ui_tray_enable(gboolean enabled) {

	if(enabled) {
		if(tray_icon == NULL)
			installTrayIcon();
	} else {
		if(tray_icon != NULL)
			removeTrayIcon();
	}
}

int ui_tray_get_count() {
	return trayCount;
}

gboolean ui_tray_get_origin(gint *x, gint *y) {
	if ( tray_icon == NULL ) {
		return FALSE;
	}
	gdk_window_get_origin (GTK_WIDGET (tray_icon)->window, x, y);	
	return TRUE;
}

void ui_tray_size_request (GtkRequisition *requisition) {
	gtk_widget_size_request ( GTK_WIDGET (tray_icon), requisition);
}
