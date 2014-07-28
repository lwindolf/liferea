/**
 * @file icons.h  Using icons from theme and package pixmaps
 *
 * Copyright (C) 2010-2013 Lars Windolf <lars.windolf@gmx.de>
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
  
#ifndef _ICONS_H
#define _ICONS_H

#include <gtk/gtk.h>

/* list of all icons used in Liferea */
typedef enum {
	ICON_UNREAD,
	ICON_FLAG,
	ICON_AVAILABLE,
	ICON_AVAILABLE_OFFLINE,
	ICON_UNAVAILABLE,
	ICON_DEFAULT,
	ICON_FOLDER,
	ICON_VFOLDER,
	ICON_NEWSBIN,
	ICON_EMPTY,
	ICON_EMPTY_OFFLINE,
	ICON_ONLINE,
	ICON_OFFLINE,
	ICON_ENCLOSURE,
	MAX_ICONS
} lifereaIcon;

/**
 * Load all icons from theme and Liferea pixmaps.
 *
 * Must be called once before icon_get() may be used!
 * Must be called only after GTK theme was initialized!
 */
void icons_load (void);

/**
 * icon_get:
 * Returns a GdkPixbuf for the requested item.
 *
 * @param icon	the icon
 *
 * Returns: (transfer none): GdkPixbuf
 */
const GdkPixbuf * icon_get (lifereaIcon icon);

/**
 * icon_create_from_file:
 * Takes a file name relative to "pixmaps" directory and tries to load the 
 * image into a GdkPixbuf. Can be used to load icons not in lifereaIcon
 * on demand.
 *
 * @param file name	the name of the file
 *
 * Returns: (transfer none): a new pixbuf or NULL
 */
GdkPixbuf * icon_create_from_file (const gchar *filename);

#endif
