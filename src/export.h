/*
   OPML feedlist import&export
   
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

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

#ifndef _EXPORT_H
#define _EXPORT_H

#include <gtk/gtk.h>

/**
 * Exports the current feedlist.
 * @param filename filename of export file
 * @returns 0 if no errors were detected
 */
int exportOPMLFeedList(gchar *filename);

/**
 * Reads an OPML file and inserts it into the feedlist.
 * @param filename path to file that will be read for importing
 * @param parent folder
 */
void importOPMLFeedList(gchar *filename, folderPtr parent);


/* GUI dialog callbacks */
void
on_import_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_export_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_exportfileselect_pressed            (GtkButton       *button,
                                        gpointer         user_data);

void
on_importfileselect_clicked            (GtkButton       *button,
                                        gpointer         user_data);

void
on_importfile_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_exportfile_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_importfileselect_pressed            (GtkButton       *button,
                                        gpointer         user_data);

void
on_exportfileselect_clicked            (GtkButton       *button,
                                        gpointer         user_data);

#endif
