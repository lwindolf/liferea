/*
   tray icon handling
   
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>

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

#include <glib-2.0/glib.h>

/**
 * Add some number of messages to the new message count in the tooltip
 * @param count number of messages.
 */
void ui_tray_add_new(gint count);

/**
 * Set the tooltip to display zero new messages.
 */
void ui_tray_zero_new(void);

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
