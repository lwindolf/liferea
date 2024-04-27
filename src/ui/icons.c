/*
 * @file icons.c  Using icons from theme and package pixmaps
 *
 * Copyright (C) 2010-2024 Lars Windolf <lars.windolf@gmx.de>
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

#include "icons.h"

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "common.h"

static GIcon *icons[MAX_ICONS];	/*<< list of icon assignments */

gchar *
icon_find_pixmap_file (const gchar *filename)
{
	gchar *pathname = g_build_filename (PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps", filename, NULL);
	if (g_file_test (pathname, G_FILE_TEST_EXISTS))
		return pathname;
	g_free (pathname);
	return NULL;
}

GdkPixbuf *
icon_create_from_file (const gchar *filename)
{
	gchar *pathname = NULL;
	GdkPixbuf *pixbuf;
	GError *error = NULL;

	if (!filename || !filename[0])
		return NULL;

	pathname = icon_find_pixmap_file (filename);

	if (!pathname) {
		g_warning (_("Couldn't find pixmap file: %s"), filename);
		return NULL;
	}

	pixbuf = gdk_pixbuf_new_from_file (pathname, &error);
	if (!pixbuf) {
		fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
		       pathname, error->message);
		g_error_free (error);
	}
	g_free (pathname);
	return pixbuf;
}

void
icons_load (void)
{
	GtkIconTheme	*icon_theme;
	gint		i;
	gchar 		*path;

	path = g_build_filename (PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps", NULL);
	icon_theme = gtk_icon_theme_get_default ();

	gtk_icon_theme_append_search_path (icon_theme, path);

	static const gchar *iconNames[] = {
		"unread",		/* ICON_UNREAD */
		"emblem-important",	/* ICON_FLAG */
		"dialog-error",		/* ICON_UNAVAILABLE */
		"default",		/* ICON_DEFAULT */
		"folder",		/* ICON_FOLDER */
		"folder-saved-search",	/* ICON_VFOLDER */
		"newsbin",		/* ICON_NEWSBIN */
		NULL
	};

	for (i = 0; i < MAX_ICONS; i++)
		icons[i] = g_themed_icon_new (iconNames[i]);
}

const GIcon *
icon_get (lifereaIcon icon)
{
	g_assert (NULL != *icons);		

	return icons[icon];
}
