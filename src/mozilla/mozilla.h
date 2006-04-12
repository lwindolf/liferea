/**
 * @file mozilla.h Liferea GtkMozEmbed support
 *
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _MOZILLA_H
#define _MOZILLA_H

#include <glib.h>
#include <gtk/gtk.h>
#ifdef __cplusplus
extern "C"
{
#endif

/* Zoom */
void mozilla_set_zoom (GtkWidget *embed, float f);
gfloat mozilla_get_zoom (GtkWidget *embed);

/* Events */
gint mozilla_get_mouse_event_button(gpointer event);
gint mozilla_key_press_cb(GtkWidget *widget, gpointer ev);
void mozilla_scroll_to_top(GtkWidget *widget);
gboolean mozilla_scroll_pagedown(GtkWidget *widget);

/* preference setting */
gboolean mozilla_save_prefs (void);
gboolean mozilla_preference_set (const char *preference_name,
				 const char *new_value);
gboolean mozilla_preference_set_boolean (const char *preference_name,
					 gboolean  new_boolean_value);
gboolean mozilla_preference_set_int (const char *preference_name,
				     int new_int_value);
gboolean mozilla_set_offline_mode (gboolean offline);
#ifdef __cplusplus
}
#endif 

#endif
