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

#include "callbacks.h"
#include "eggtrayicon.h"
#include "support.h"

#define NO_NEW_MESSAGES		"no new messages"

#define NO_NEW_ITEMS	0
#define NEW_ITEMS	1

extern GdkPixbuf	*emptyIcon;
extern GdkPixbuf	*availableIcon;

extern GtkWidget	*mainwindow;

static GtkWidget	*eventbox;
static gboolean		newItems = FALSE;
static EggTrayIcon 	*tray_icon;
static GtkTooltips	*tray_icon_tips;
static GtkWidget	*image = NULL;		/* the image in the notification area */

void setTrayToolTip(gchar *string) {

   	gtk_tooltips_set_tip(GTK_TOOLTIPS(tray_icon_tips), 
			     GTK_WIDGET(eventbox),
			     string, string);
}

static void setTrayIcon(GdkPixbuf *icon) {

	if(NULL != image)
		gtk_widget_destroy(image);

	image = gtk_image_new_from_pixbuf(icon);
	gtk_container_add(GTK_CONTAINER(eventbox), image);
	gtk_widget_show_all(GTK_WIDGET(tray_icon));
}

void doTrayIcon(void) { 

	if(FALSE == newItems) {
		setTrayIcon(availableIcon); 
		newItems = TRUE;
	}
}

void undoTrayIcon(void) {

	if(TRUE == newItems) {
		setTrayIcon(emptyIcon); 
		newItems = FALSE;
	}
}

static void tray_icon_pressed(GtkWidget *button, GdkEventButton *event, EggTrayIcon *icon) {

	undoTrayIcon();
	gtk_window_present(GTK_WINDOW(mainwindow));
}

void setupTrayIcon(void) {

  	tray_icon = egg_tray_icon_new(PACKAGE);
	eventbox = gtk_event_box_new();
	
  	g_signal_connect(eventbox, "button_press_event",
			 G_CALLBACK (tray_icon_pressed), tray_icon);
  	gtk_container_add(GTK_CONTAINER(tray_icon), eventbox);
	
	setTrayIcon(emptyIcon);
	
   	tray_icon_tips = gtk_tooltips_new();
	setTrayToolTip(_(NO_NEW_MESSAGES));	
}
