/**
 * @file ui_tray.c tray icon handling
 * 
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <math.h>
#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "common.h"
#include "conf.h"
#include "eggtrayicon.h"
#include "feedlist.h"
#include "update.h"
#include "ui/ui_dnd.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_popup.h"
#include "ui/ui_tray.h"

// FIXME: determine this from Pango or Cairo somehow...
#define	FONT_CHAR_WIDTH	6
#define FONT_CHAR_HEIGHT 8

#define TRAY_ICON_WIDTH	16
#define TRAY_ICON_HEIGHT 16

extern GdkPixbuf	*icons[];

static struct trayIcon_priv {
	int		trayCount;		/**< reference counter */
	GdkPixbuf	*currentIcon;		/**< currently displayed icon */
	
	EggTrayIcon 	*widget;		/**< the tray icon widget */
	GtkWidget	*image;			/**< render widget in the notification area */
	GtkWidget	*alignment;
	GtkWidget	*eventBox;
	GtkTooltips	*toolTips;
} *trayIcon_priv = NULL;

static void ui_tray_install(void);

void ui_tray_tooltip_set(gchar *message) {
	GtkTooltipsData	*data = NULL;
	
	g_assert(trayIcon_priv->toolTips);

	data = gtk_tooltips_data_get(GTK_WIDGET(trayIcon_priv->widget));

	if(NULL != data) {
		/* FIXME: Is this necessary? Gtk 2.2.4 frees these strings
		   inside of set_tip, if needed. */
		g_free(data->tip_text);
		g_free(data->tip_private);
		data->tip_text = NULL;
		data->tip_private = NULL;
	}
	
	gtk_tooltips_set_tip(GTK_TOOLTIPS(trayIcon_priv->toolTips), 
	                     GTK_WIDGET(trayIcon_priv->eventBox),
			     message, message);
}

static void ui_tray_expose_cb() {
	cairo_t		*c;
	gchar		*str;
	guint		newItems;
	GtkRequisition	requisition;
	
	gtk_widget_size_request(GTK_WIDGET(trayIcon_priv->widget), &requisition);
	
	/* We expect currentIcon to be a 16x16 image and we will 
	   render a colored area with height 10 in the middle of 
	   the image with 8pt text inside */

	gdk_draw_pixbuf(GDK_DRAWABLE(trayIcon_priv->image->window), 
	                NULL, trayIcon_priv->currentIcon, 0, 0, 
			(requisition.width > TRAY_ICON_WIDTH)?(requisition.width - TRAY_ICON_WIDTH)/2 - 1:0, 
			(requisition.height > TRAY_ICON_HEIGHT)?(requisition.height - TRAY_ICON_HEIGHT)/2 - 1:0, 
	                gdk_pixbuf_get_height(trayIcon_priv->currentIcon),
			gdk_pixbuf_get_width(trayIcon_priv->currentIcon), 
			GDK_RGB_DITHER_NONE, 0, 0);
	
	if(!conf_get_bool_value(SHOW_NEW_COUNT_IN_TRAY))
		return;
	
	newItems = feedlist_get_new_item_count();
	if(newItems > 0) {
		guint textWidth, textStart;
		str = g_strdup_printf("%d", newItems);
		textWidth = strlen(str) * FONT_CHAR_WIDTH;
		
		if(textWidth + 2 > TRAY_ICON_WIDTH)
			textStart = 1;
		else
			textStart = TRAY_ICON_WIDTH/2 - textWidth/2;

		c = gdk_cairo_create(trayIcon_priv->image->window);
		cairo_rectangle(c, textStart - 1, 3, textWidth + 1, FONT_CHAR_HEIGHT + 2);
		cairo_set_source_rgb(c, 1, 0.50, 0.10);	// orange
		cairo_fill(c);

		cairo_set_source_rgb(c, 1, 1, 1);		
		cairo_move_to(c, textStart - 1, 3 + FONT_CHAR_HEIGHT);
		cairo_select_font_face(c, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(c, FONT_CHAR_HEIGHT + 2);
		cairo_show_text(c, str);
		cairo_destroy(c);

		g_free(str);
	}
}

static void
ui_tray_icon_set (gint newItems, GdkPixbuf *icon)
{
	guint 	width;

	g_assert (trayIcon_priv->widget);

	/* Having two code branches here to have real transparency
	   at least with new count disabled... */
	if (conf_get_bool_value (SHOW_NEW_COUNT_IN_TRAY)) {	
		width = ((guint) log10 (newItems) + 1) * FONT_CHAR_WIDTH;
		width += 2; /* number color border */
		width += 2; /* tray icon padding */;
		if (width < 16)
			width = 16;

		trayIcon_priv->currentIcon = icon;

		if (trayIcon_priv->image)
			gtk_widget_destroy (trayIcon_priv->image);

		if (trayIcon_priv->alignment)
			gtk_widget_destroy (trayIcon_priv->alignment);

		trayIcon_priv->alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		trayIcon_priv->image = gtk_drawing_area_new ();
		gtk_widget_set_size_request (trayIcon_priv->image, width, 16);
		g_signal_connect (G_OBJECT (trayIcon_priv->image), "expose_event",  
                        	  G_CALLBACK (ui_tray_expose_cb), NULL);

		gtk_container_add (GTK_CONTAINER (trayIcon_priv->eventBox), trayIcon_priv->alignment);
		gtk_container_add (GTK_CONTAINER (trayIcon_priv->alignment), trayIcon_priv->image);
		gtk_widget_show_all (GTK_WIDGET(trayIcon_priv->widget));
	} else {
		/* Skip loading icon if already displayed. */
		if (icon == trayIcon_priv->currentIcon)
			return;
		trayIcon_priv->currentIcon = icon;

		if (trayIcon_priv->image)
			gtk_widget_destroy (trayIcon_priv->image);

		if (trayIcon_priv->alignment) {
			gtk_widget_destroy (trayIcon_priv->alignment);
			trayIcon_priv->alignment = NULL;
		}

		trayIcon_priv->alignment = gtk_alignment_new (0.5, 0.5, 0.0, 0.0);
		trayIcon_priv->image = gtk_image_new_from_pixbuf (icon);
		gtk_container_add (GTK_CONTAINER (trayIcon_priv->eventBox), trayIcon_priv->alignment);
		gtk_container_add (GTK_CONTAINER (trayIcon_priv->alignment), trayIcon_priv->image);
		gtk_widget_show_all (GTK_WIDGET (trayIcon_priv->widget));
   	}
}

void ui_tray_update(void) {
	gint	newItems, unreadItems;
	gchar	*msg, *tmp;
	
	if(!trayIcon_priv)
		return;

	newItems = feedlist_get_new_item_count();
	unreadItems = feedlist_get_unread_item_count();
		
	if(newItems != 0) {
		if(update_is_online())
			ui_tray_icon_set(newItems, icons[ICON_AVAILABLE]);
		else
			ui_tray_icon_set(newItems, icons[ICON_AVAILABLE_OFFLINE]);
			
		msg = g_strdup_printf(ngettext("%d new item", "%d new items", newItems), newItems);
	} else {
		if(update_is_online())
			ui_tray_icon_set(newItems, icons[ICON_EMPTY]);
		else
			ui_tray_icon_set(newItems, icons[ICON_EMPTY_OFFLINE]);
			
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

	ui_tray_install();
	
	return FALSE; /* for when we're called by the glib idle handler */
}


static void ui_tray_embedded_cb(GtkWidget *widget, void *data) {

	ui_mainwindow_tray_add();
}


static void ui_tray_destroyed_cb(GtkWidget *widget, void *data) {

	g_object_unref(G_OBJECT(trayIcon_priv->widget));
	g_free(trayIcon_priv);
	trayIcon_priv = NULL;
	
	ui_mainwindow_tray_remove();
	
	/* And make it re-appear when the notification area reappears */
	g_idle_add(ui_tray_create_cb, NULL);
	
}

static void ui_tray_install(void) {

	g_assert(!trayIcon_priv);
	trayIcon_priv = g_new0(struct trayIcon_priv, 1);

	trayIcon_priv->widget = egg_tray_icon_new(PACKAGE);
	trayIcon_priv->eventBox = gtk_event_box_new();
	
	g_signal_connect(trayIcon_priv->eventBox, "button_press_event",
	                 G_CALLBACK(tray_icon_pressed), trayIcon_priv->widget);
	g_signal_connect(G_OBJECT(trayIcon_priv->widget), "embedded",
	                 G_CALLBACK(ui_tray_embedded_cb), NULL);
	g_signal_connect(G_OBJECT(trayIcon_priv->widget), "destroy",
	                 G_CALLBACK(ui_tray_destroyed_cb), NULL);
	
	ui_dnd_setup_URL_receiver(trayIcon_priv->eventBox);
	
	gtk_container_add(GTK_CONTAINER(trayIcon_priv->widget), trayIcon_priv->eventBox);
	g_object_ref(G_OBJECT(trayIcon_priv->widget));
	
	trayIcon_priv->toolTips = gtk_tooltips_new();
	ui_tray_update();
	trayIcon_priv->trayCount++;
}

static void ui_tray_remove(void) {

	g_assert(trayIcon_priv->widget);
	
	g_signal_handlers_disconnect_by_func(G_OBJECT(trayIcon_priv->widget),
	                                     G_CALLBACK(ui_tray_destroyed_cb), NULL);
	gtk_widget_destroy(trayIcon_priv->image);
	g_object_unref(G_OBJECT(trayIcon_priv->widget));
	gtk_object_destroy(GTK_OBJECT(trayIcon_priv->widget));
	g_free(trayIcon_priv);
	trayIcon_priv = NULL;

	ui_mainwindow_tray_remove();
}

void ui_tray_enable(gboolean enabled) {

	if(enabled) {
		if(!trayIcon_priv)
			ui_tray_install();
	} else {
		if(trayIcon_priv)
			ui_tray_remove();
	}
}

int ui_tray_get_count() {
	return trayIcon_priv?trayIcon_priv->trayCount:0;
}

gboolean ui_tray_get_origin(gint *x, gint *y) {

	if(!trayIcon_priv)
		return FALSE;

	gdk_window_get_origin(GTK_WIDGET(trayIcon_priv->widget)->window, x, y);	
	return TRUE;
}

void ui_tray_size_request(GtkRequisition *requisition) {

	if(!trayIcon_priv)
		return;

	gtk_widget_size_request(GTK_WIDGET(trayIcon_priv->widget), requisition);
}
