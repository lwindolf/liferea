/**
 * @file ui_enclosures.c enclosures user interface
 *
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
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

#include <gtk/gtk.h>
 
#define ENCLOSURE_PROTOCOL "liferea-enclosure://"

typedef struct encType {
	gchar		*mime;		/* either mime or extension is set */
	gchar		*extension;
	gchar		*cmd;		/* the command to launch the enclosure type */
	gboolean	permanent;	/* if TRUE definition is deleted after opening and 
					   not added to the permanent list of type configs */
} *encTypePtr;

/** loads the enclosure type configuration */
void ui_enclosure_init(void);

/** returns all configured enclosure types */
GSList * ui_enclosure_get_types(void);

/** opens a popup menu for the given link */
void ui_enclosure_new_popup(gchar *url);

/* popup menu callbacks */
void on_popup_open_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_save_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget);
