/**
 * @file ui_tray.h tray icon handling
 *
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
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

#include <glib.h>

/**
 * Enforce tray icon state update.
 */
void ui_tray_update(void);

/**
 * Set the tooltip message in the systray
 * @param message new message
 */
void ui_tray_tooltip_set(gchar *message);

/**
 * Enable or disable the systray icon
 * @param enabled set to TRUE to show icon, or false to hide it
 */
void ui_tray_enable(gboolean enabled);

/**
 * @returns the current number of enabled systray icons
 */
int ui_tray_get_count();

/**
 * Determine position of tray icon ( libnotify )
 *
 * @param x horizontal position
 * @param y vertical position
 */
gboolean ui_tray_get_origin(gint *x, gint *y);

/**
 * Determine size of tray icon ( libnotify )
 *
 * @param requisition requisition
 */
void ui_tray_size_request (GtkRequisition *requisition);
