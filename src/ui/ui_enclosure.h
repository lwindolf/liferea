/**
 * @file ui_enclosure.h enclosures user interface
 *
 * Copyright (C) 2005-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifndef _UI_ENCLOSURE_H
#define _UI_ENCLOSURE_H

#include <gtk/gtk.h>

#include "enclosure.h"		// FIXME: should not be necessary

/** 
 * Opens a popup menu for the given link 
 *
 * @param url		valid HTTP URL
 */
void ui_enclosure_new_popup(const gchar *url);

/* popup menu callbacks */
void on_popup_open_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_save_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget);

// FIXME: these do not belong here!
void ui_enclosure_change_type (encTypePtr type);
void ui_enclosure_remove_type (encTypePtr type);

#endif /* _UI_ENCLOSURE_H */
