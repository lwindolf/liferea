/*
   tray icon handling
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>
   Copyright (C) 2004 Christophe Barbe <christophe.barbe@ufies.org>

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

extern GdkPixbuf	*icons[];

extern GtkWidget	*mainwindow;

static GtkWidget	*eventbox = NULL;
static gint		newItems = 0;
static EggTrayIcon 	*tray_icon =  NULL;
static GtkTooltips	*tray_icon_tips = NULL;
static GtkWidget	*image = NULL;		/* the image in the notification area */

void setTrayToolTip(gchar *string) {
	GtkTooltipsData	*data = NULL;
	g_assert(tray_icon_tips);

	data = gtk_tooltips_data_get(GTK_WIDGET(tray_icon));

	if(NULL != data) {
		g_free(data->tip_text);
		g_free(data->tip_private);
	}
	
	gtk_tooltips_set_tip(GTK_TOOLTIPS(tray_icon_tips), 
					 GTK_WIDGET(eventbox),
					 string, string);
	g_free(string);
}

static void setTrayIcon(GdkPixbuf *icon) {
	g_assert(tray_icon);

	if(NULL != image)
		gtk_widget_destroy(image);

	image = gtk_image_new_from_pixbuf(icon);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	gtk_widget_show_all(GTK_WIDGET(tray_icon));
}

void doTrayIcon(gint count) { 
	if(count > 0) {
		setTrayIcon(icons[ICON_AVAILABLE]); 
		newItems += count;
		setTrayToolTip(g_strdup_printf(_("%d new items!"), newItems));
	}
}

void undoTrayIcon(void) {
	if(0 != newItems) {
		setTrayIcon(icons[ICON_EMPTY]);
		setTrayToolTip(g_strdup(_("No new items.")));
		newItems = 0;
	}
}

/* a click on the systray icon should show the program window
   if invisible or hide it if visible */
static void tray_icon_pressed(GtkWidget *button, GdkEventButton *event, EggTrayIcon *icon) {
	gint		x,y;
	undoTrayIcon();

	if (GTK_WIDGET_VISIBLE(mainwindow)) {
		if (gdk_window_get_state(GTK_WIDGET(mainwindow)->window) & GDK_WINDOW_STATE_ICONIFIED) {
			gtk_window_present(GTK_WINDOW(mainwindow));
		} else {
			/* save window position */
			gtk_window_get_position(GTK_WINDOW(mainwindow), &x, &y);
			setNumericConfValue(LAST_WINDOW_X, x);
			setNumericConfValue(LAST_WINDOW_Y, y);
			/* hide window */
			gtk_widget_hide(mainwindow);
		}
	} else {
		/* restore window position */
		if((0 != getNumericConfValue(LAST_WINDOW_X)) &&
			(0 != getNumericConfValue(LAST_WINDOW_Y)))
				gtk_window_move(GTK_WINDOW(mainwindow), getNumericConfValue(LAST_WINDOW_X),
								getNumericConfValue(LAST_WINDOW_Y));
		/* unhide window */
		gtk_window_present(GTK_WINDOW(mainwindow));
	}
	return;

}

static void installTrayIcon(void) {
	g_assert(!tray_icon);
	if(getBooleanConfValue(SHOW_TRAY_ICON)) {
		tray_icon = egg_tray_icon_new(PACKAGE);
		eventbox = gtk_event_box_new();
		
		g_signal_connect(eventbox, "button_press_event", G_CALLBACK(tray_icon_pressed), tray_icon);
		gtk_container_add(GTK_CONTAINER(tray_icon), eventbox);
		
		tray_icon_tips = gtk_tooltips_new();
		newItems = -1;
		undoTrayIcon();
	}
}

void updateTrayIcon(void) {
	if(getBooleanConfValue(SHOW_TRAY_ICON)) {
		if (tray_icon == NULL)
			installTrayIcon();
	} else {
		if (tray_icon != NULL) {
			gtk_widget_destroy(image);
			image = NULL;
			gtk_object_destroy (GTK_OBJECT (tray_icon));
			tray_icon = NULL;
		}
	}
}
