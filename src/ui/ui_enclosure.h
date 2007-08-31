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

typedef struct enclosureDownloadTool {
	const char	*format;	/**< format string to construct download command */
	gboolean	niceFilename;	/**< TRUE if format has second %s for output file name */
} *enclosureDownloadToolPtr; 

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

/** 
 * Opens a popup menu for the given link 
 *
 * @param url		valid HTTP URL
 */
void ui_enclosure_new_popup(const gchar *url);

/** 
 * Downloads a given enclosure URL into a file
 *  
 * @param type		NULL or pointer to type structure
 * @param url		valid HTTP URL
 * @param filename	valid filename
 */
void ui_enclosure_save(encTypePtr type, const gchar *url, const gchar *filename);

/* popup menu callbacks */
void on_popup_open_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget);
void on_popup_save_enclosure(gpointer callback_data, guint callback_action, GtkWidget *widget);

void ui_enclosure_change_type(gpointer type);
void ui_enclosure_remove_type(gpointer type);
#endif /* _UI_ENCLOSURE_H */
