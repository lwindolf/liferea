/**
 * @file mozsupport.h Liferea GtkMozEmbed support
 *
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifndef _MOZSUPPORT_H
#define _MOZSUPPORT_H

#include <glib.h>
#include <gtk/gtk.h>
#ifdef __cplusplus
extern "C"
{
#endif

/* Zoom */
void mozsupport_set_zoom (GtkWidget *embed, float f);
gfloat mozsupport_get_zoom (GtkWidget *embed);

/* Events */
gint mozsupport_get_mouse_event_button(gpointer event);
gint mozsupport_key_press_cb(GtkWidget *widget, gpointer ev);
void mozsupport_scroll_to_top(GtkWidget *widget);
gboolean mozsupport_scroll_pagedown(GtkWidget *widget);

/* preference setting */
gboolean mozsupport_save_prefs (void);
gboolean mozsupport_preference_set (const char *preference_name,
				 const char *new_value);
gboolean mozsupport_preference_set_boolean (const char *preference_name,
					 gboolean  new_boolean_value);
gboolean mozsupport_preference_set_int (const char *preference_name,
				     int new_int_value);
void mozsupport_set_offline_mode (gboolean offline);
#ifdef __cplusplus
}
#endif 

#endif
