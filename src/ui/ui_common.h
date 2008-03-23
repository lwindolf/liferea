/**
 * @file ui_common.h  UI helper functions
 *
 * Copyright (C) 2008 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _UI_COMMON_H
#define _UI_COMMON_H

#include <gtk/gtk.h>

/**
 * Helper function to set up a combo box option menu.
 * To be used to initialize dialogs.
 *
 * @param widget	the option menu widget
 * @param options	list of option literals
 * @param callback	"changed" callback for the widget (or NULL)
 * @param defaultValue	the default menu value
 */
void ui_common_setup_combo_menu (GtkWidget *widget, gchar **options, GCallback callback, gint defaultValue);

#endif
