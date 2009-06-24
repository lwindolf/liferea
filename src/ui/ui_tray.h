/**
 * @file ui_tray.h  tray icon handling
 *
 * Copyright (C) 2004-2008 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _UI_TRAY_H
#define _UI_TRAY_H

#include <glib.h>

/**
 * Enforce tray icon state update.
 */
void ui_tray_update (void);

/**
 * Enable or disable the systray icon
 *
 * @param enabled	TRUE to show icon, or FALSE to hide it
 */
void ui_tray_enable (gboolean enabled);

/**
 * Get the status icon
 *
 * @returns the status icon, or NULL
 */
GtkStatusIcon* ui_tray_get_status_icon (void);

/**
 * Determine number of active tray icons.
 *
 * @returns the current number of enabled systray icons
 */
guint ui_tray_get_count (void);

/**
 * Determine position of tray icon (for libnotify)
 *
 * @param x	horizontal position
 * @param y	vertical position
 *
 * @returns FALSE on error
 */
gboolean ui_tray_get_origin (gint *x, gint *y);

/**
 * Determine size of tray icon (for libnotify)
 *
 * @param requisition	the requisition
 */
void ui_tray_size_request (GtkRequisition *requisition);

#endif
