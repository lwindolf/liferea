/*
   tray icon handling
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#define NO_NEW_ITEMS	0
#define NEW_ITEMS	1

extern GdkPixbuf	*emptyIcon;
extern GdkPixbuf	*availableIcon;

extern GtkWidget	*mainwindow;

static GtkWidget	*eventbox = NULL;
static gint		newItems = 0;
static EggTrayIcon 	*tray_icon =  NULL;
static GtkTooltips	*tray_icon_tips = NULL;
static GtkWidget	*image = NULL;		/* the image in the notification area */

void setTrayToolTip(gchar *string) {
	GtkTooltipsData	*data = NULL;
	
	if(NULL != tray_icon_tips) {
		data = gtk_tooltips_data_get(GTK_WIDGET(tray_icon));
		if(NULL != data) {
			g_free(data->tip_text);
			g_free(data->tip_private);
		}		
		
	   	gtk_tooltips_set_tip(GTK_TOOLTIPS(tray_icon_tips), 
				     GTK_WIDGET(eventbox),
				     string, g_strdup(string));
	}
}

static void setTrayIcon(GdkPixbuf *icon) {

	if(NULL != tray_icon) {
		if(NULL != image)
			gtk_widget_destroy(image);

		image = gtk_image_new_from_pixbuf(icon);
		gtk_container_add(GTK_CONTAINER(eventbox), image);
		gtk_widget_show_all(GTK_WIDGET(tray_icon));
	}
}

void doTrayIcon(gint count) { 

	setTrayIcon(availableIcon); 
	newItems += count;
	setTrayToolTip(g_strdup_printf(_("%d new items!"), newItems));
}

void undoTrayIcon(void) {

	if(0 != newItems) {
		setTrayIcon(emptyIcon);
		setTrayToolTip(g_strdup(_("No new items.")));
		newItems = 0;
	}
}

static void tray_icon_pressed(GtkWidget *button, GdkEventButton *event, EggTrayIcon *icon) {

	undoTrayIcon();
	
	/* a click on the systray icon should show the program window
	   if invisible or hide it if visible */
	if(GTK_WIDGET_VISIBLE(GTK_WIDGET(mainwindow)))
		gtk_widget_hide(GTK_WIDGET(mainwindow));
	else
		gtk_window_present(GTK_WINDOW(mainwindow));
}

static gboolean mainwindow_state_changed(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	GdkEventWindowState	*state;
	
	/* to hide the window when iconified ... */
	state = ((GdkEventWindowState *)event);
	if(GDK_WINDOW_STATE_ICONIFIED == state->new_window_state)
		gtk_widget_hide(GTK_WIDGET(mainwindow));
	
	return TRUE;
}


void setupTrayIcon(void) {

	if(NULL == tray_icon) {
		if(getBooleanConfValue(SHOW_TRAY_ICON)) {
			tray_icon = egg_tray_icon_new(PACKAGE);
			eventbox = gtk_event_box_new();

			/* disabled because this causes troubles when switching desktops!
			g_signal_connect(mainwindow, "window-state-event", G_CALLBACK(mainwindow_state_changed), mainwindow);
			*/
			g_signal_connect(eventbox, "button_press_event", G_CALLBACK(tray_icon_pressed), tray_icon);
			gtk_container_add(GTK_CONTAINER(tray_icon), eventbox);

			tray_icon_tips = gtk_tooltips_new();
			newItems = -1;
			undoTrayIcon();
		}
	}
}
