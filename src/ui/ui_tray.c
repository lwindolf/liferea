/**
 * @file ui_tray.c tray icon handling
 * 
 * Copyright (C) 2003-2009 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004 Christophe Barbe <christophe.barbe@ufies.org>
 * Copyright (C) 2009 Hubert Figuiere <hub@figuiere.net>
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
#include "feedlist.h"
#include "net_monitor.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"
#include "ui/ui_popup.h"
#include "ui/ui_tray.h"

// FIXME: determine this from Pango or Cairo somehow...
#define FONT_CHAR_WIDTH 6
#define FONT_CHAR_HEIGHT 8

static struct trayIcon_priv {
	int		trayCount;		/**< reference counter */
	GdkPixbuf	*currentIcon;		/**< currently displayed icon */
	
	GtkStatusIcon 	*status_icon;		/**< the tray icon widget */
} *trayIcon_priv = NULL;

static void ui_tray_install(void);

GtkStatusIcon*
ui_tray_get_status_icon (void)
{
	if (!trayIcon_priv)
		return NULL;

	return trayIcon_priv->status_icon;
}

static void
ui_tray_tooltip_set (const gchar *message)
{
	gtk_status_icon_set_tooltip_text(trayIcon_priv->status_icon, message);
}

static GdkPixbuf*
ui_tray_make_icon (void)
{
	cairo_t		*c;
	cairo_surface_t	*cs;
	gchar		*str;
	guint		newItems;
	gboolean	show_new_count_in_tray;
	gint		size, i, j;
	gint		w, h;
	int		stride, pstride;
	unsigned char	*data, *row;
	guchar		*pixels, *p;
	GdkPixbuf	*out;

	/* We expect currentIcon to be a 16x16 image and we will 
	   render a colored area with height 10 in the middle of 
	   the image with 8pt text inside */

	h = gdk_pixbuf_get_height(trayIcon_priv->currentIcon);
	w = gdk_pixbuf_get_width(trayIcon_priv->currentIcon);

	/* this is a hack as we don't know the size of status icon
	 * if it is not embedded.
	 */
	if(gtk_status_icon_is_embedded (trayIcon_priv->status_icon)) {
		size = gtk_status_icon_get_size (trayIcon_priv->status_icon);
	} else {
		size = MAX(h,w);
	}

	conf_get_bool_value (SHOW_NEW_COUNT_IN_TRAY, &show_new_count_in_tray);
	if(!show_new_count_in_tray)
		return trayIcon_priv->currentIcon;
	
	newItems = feedlist_get_new_item_count();

	cs = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, size, size);
	c = cairo_create (cs);
	cairo_set_antialias (c, CAIRO_ANTIALIAS_NONE);

	gdk_cairo_set_source_pixbuf (c, trayIcon_priv->currentIcon,
	                             (size - w) / 2, (size - h) / 2);
	cairo_paint (c);

	if(newItems > 0) {
		gint textWidth, textStart;
		/* GtkStatusIcon doesn't allow non-square icons yet, so the
		 * part that doesn't fit is just cropped. Don't put a large
		 * text there then, otherwise it won't appear entirely.
		 */
		if (newItems > 99)
			str = g_strdup_printf ("99+");
		else
			str = g_strdup_printf ("%d", newItems);
		textWidth = strlen(str) * FONT_CHAR_WIDTH;
		
		if(textWidth + 2 > size)
			textStart = 1;
		else
			textStart = size/2 - textWidth/2;

		cairo_rectangle(c, textStart - 1, size/2 - (FONT_CHAR_HEIGHT + 2)/2,
				textWidth + 1, FONT_CHAR_HEIGHT + 2);
		cairo_set_source_rgba(c, 1, 0.50, 0.10, 1.0);	// orange
		cairo_fill(c);

		cairo_set_source_rgba(c, 1, 1, 1, 1.0);
		cairo_move_to(c, textStart - 1, size/2 - (FONT_CHAR_HEIGHT + 2)/2 + FONT_CHAR_HEIGHT);
		cairo_select_font_face(c, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
		cairo_set_font_size(c, FONT_CHAR_HEIGHT + 2);
		cairo_show_text(c, str);

		g_free(str);
	}

	stride = cairo_image_surface_get_stride (cs);
	data = cairo_image_surface_get_data (cs);

	out = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, size, size);
	pstride = gdk_pixbuf_get_rowstride (out);
	pixels = gdk_pixbuf_get_pixels (out);
	for (i = 0; i < size; i++) {
		row = data + (i * stride);
		p = pixels + (i * pstride);
		for (j = 0; j < size; j++) {
			guint32 px = *(guint32*)row;
			p[0] = (px & 0xff0000) >> 16;
			p[1] = (px & 0xff00) >> 8;
			p[2] = (px & 0xff);
			p[3] = (px & 0xff000000) >> 24;
			p += 4;
			row += 4;
		}
	}

	cairo_destroy(c);
	cairo_surface_destroy (cs);

	return out;
}


static void
ui_tray_icon_set (gint newItems, GdkPixbuf *icon)
{
	gboolean	show_new_count_in_tray;

	g_assert (trayIcon_priv->status_icon);

	/* Having two code branches here to have real transparency
	   at least with new count disabled... */
	conf_get_bool_value (SHOW_NEW_COUNT_IN_TRAY, &show_new_count_in_tray);
	if (show_new_count_in_tray) {	

		trayIcon_priv->currentIcon = icon;

		gtk_status_icon_set_from_pixbuf (trayIcon_priv->status_icon, ui_tray_make_icon ());
	} else {
		/* Skip loading icon if already displayed. */
		if (icon == trayIcon_priv->currentIcon)
			return;
		trayIcon_priv->currentIcon = icon;
		gtk_status_icon_set_from_pixbuf(trayIcon_priv->status_icon, icon);
	}
}

void
ui_tray_update (void)
{
	gint	newItems, unreadItems;
	gchar	*msg, *tmp;
	
	if (!trayIcon_priv)
		return;

	newItems = feedlist_get_new_item_count ();
	unreadItems = feedlist_get_unread_item_count ();
		
	if (newItems != 0) {
		if (network_monitor_is_online ())
			ui_tray_icon_set (newItems, icons[ICON_AVAILABLE]);
		else
			ui_tray_icon_set (newItems, icons[ICON_AVAILABLE_OFFLINE]);
			
		msg = g_strdup_printf (ngettext ("%d new item", "%d new items", newItems), newItems);
	} else {
		if (network_monitor_is_online ())
			ui_tray_icon_set (newItems, icons[ICON_EMPTY]);
		else
			ui_tray_icon_set (newItems, icons[ICON_EMPTY_OFFLINE]);
			
		msg = g_strdup (_("No new items"));
	}

	if (unreadItems != 0)
		tmp = g_strdup_printf (ngettext("%s\n%d unread item", "%s\n%d unread items", unreadItems), msg, unreadItems);
	else
		tmp = g_strdup_printf (_("%s\nNo unread items"), msg);

	ui_tray_tooltip_set (tmp);
	g_free (tmp);
	g_free (msg);
}



static void
tray_icon_popup (GtkStatusIcon *status_icon,
                 guint          button,
                 guint          activate_time,
                 gpointer       user_data)
{
	ui_popup_systray_menu (&gtk_status_icon_position_menu, button, activate_time,
	                       status_icon);
}


/* a click on the systray icon should show the program window
   if invisible or hide it if visible */
static void
tray_icon_activate (GtkStatusIcon * status_icon, gpointer user_data)
{	
	liferea_shell_toggle_visibility ();
}


static void ui_tray_install(void) {

	g_assert(!trayIcon_priv);
	trayIcon_priv = g_new0(struct trayIcon_priv, 1);

	trayIcon_priv->status_icon = gtk_status_icon_new();
	
	g_signal_connect(trayIcon_priv->status_icon, "activate",
	                 G_CALLBACK(tray_icon_activate), NULL);  
	g_signal_connect(trayIcon_priv->status_icon, "popup-menu",
	                 G_CALLBACK(tray_icon_popup), NULL);
	
// No URL dropping support on the status icon.
// liferea_shell_setup_URL_receiver (trayIcon_priv->eventBox);
	
	ui_tray_update();
	trayIcon_priv->trayCount++;
}

static void ui_tray_remove(void) {

	g_assert(trayIcon_priv->status_icon);
	
	g_object_unref(G_OBJECT(trayIcon_priv->status_icon));
	g_free(trayIcon_priv);
	trayIcon_priv = NULL;

	if (0 == ui_tray_get_count ())
		liferea_shell_present ();
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

guint
ui_tray_get_count(void)
{
	return trayIcon_priv?trayIcon_priv->trayCount:0;
}

gboolean ui_tray_get_origin(gint *x, gint *y) {

	GdkRectangle rect;
	if(!trayIcon_priv)
		return FALSE;

	gtk_status_icon_get_geometry(trayIcon_priv->status_icon, NULL,
	                             &rect, NULL);
	*x = rect.x;
	*y = rect.y;

	return TRUE;
}

void ui_tray_size_request(GtkRequisition *requisition) {

	gint size;
	if(!trayIcon_priv)
		return;

	size = gtk_status_icon_get_size(trayIcon_priv->status_icon);
	requisition->width = requisition->height = size;
}

