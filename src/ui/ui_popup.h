/**
 * @file ui_popup.h popup menus
 *
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
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
#include "ui_mainwindow.h"
#include "item.h"

/* prepares the popup menues */
void ui_popup_update_menues(void);

/* function to generate popup menus for the item list depending
   on the list mode given in itemlist_mode */
GtkMenu *make_item_menu(itemPtr ip);

/* popup menu generating functions for the HTML view */
GtkMenu *make_html_menu(void);
GtkMenu *make_url_menu(char* url);

/** Create the popup menu for the systray icon */
GtkMenu *ui_popup_make_systray_menu(void);

/** popup menu generation for the enclosure popup menu */
GtkMenu *ui_popup_make_enclosure_menu(const gchar *enclosure);

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
