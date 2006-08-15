/**
 * @file ui_update.h GUI update monitor
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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
 
#ifndef _UI_UPDATE_H
#define _UI_UPDATE_H

#include <gtk/gtk.h>

void on_close_update_monitor_clicked(GtkButton *button, gpointer user_data);

void on_menu_show_update_monitor(GtkWidget *widget, gpointer user_data);

void on_cancel_all_requests_clicked(GtkButton *button, gpointer user_data);
 
#endif
