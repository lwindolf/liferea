/**
 * @file ui_popup.h popup menus
 *
 * Copyright (C) 2003-2008 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _UI_POPUP_H
#define _UI_POPUP_H

#include <gtk/gtk.h>

#include "item.h"

/**
 * Updates dynamic popup menues. Needs to be called at least
 * once before using the popup creation methods below.
 */
void ui_popup_update_menues (void);

/**
 * Creates a popup menu with options for the item list and the
 * given selected item.
 * (Open Link, Copy Item, Copy Link...)
 *
 * @param item	the selected item
 *
 * @returns a new popup menu
 */
GtkMenu * ui_popup_make_item_menu (itemPtr item);

/**
 * Creates a popup menu with options for the HTML view context menu.
 * (Currently only zooming options)
 *
 * @returns a new popup menu
 */
GtkMenu * ui_popup_make_html_menu (void);

/**
 * Creates a popup menu with options for a HTML link context menu.
 * (Copy link, Open Link...)
 *
 * @param url	the link URL
 *
 * @returns a new popup menu
 */
GtkMenu * ui_popup_make_url_menu (const gchar * url);

/**
 * Creates a popup menu for the systray icon.
 * (Offline mode, Close, Minimize...)
 *
 * @returns a new popup menu
 */
GtkMenu * ui_popup_make_systray_menu (void);

/**
 * Creates a popup menu for the enclosure list view.
 * (Save As, Open With...)
 *
 * @param url	the enclosure URL
 *
 * @returns a new popup menu
 */
GtkMenu * ui_popup_make_enclosure_menu (const gchar *url);

/* GUI callbacks */

gboolean
on_mainfeedlist_button_press_event     (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

gboolean
on_itemlist_button_press_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

#endif
