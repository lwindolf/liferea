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

#include <libxml/uri.h>
#include "ui_popup.h"
#include "ui_enclosure.h"

/* opens a popup menu for the given link */
void ui_enclosure_new_popup(gchar *url) {
	gchar	*enclosure;
	
	if(NULL != (enclosure = xmlURIUnescapeString(url + strlen(ENCLOSURE_PROTOCOL "load?"), 0, NULL))) {
		gtk_menu_popup(ui_popup_make_enclosure_menu(enclosure), NULL, NULL, NULL, NULL, 0, gtk_get_current_event_time());
		g_free(enclosure);
	}
}

void on_popup_open_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget) {
g_print("open: %s\n", (gchar *)callback_data);
}
void on_popup_save_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget) {
g_print("save: %s\n", (gchar *)callback_data);
}
