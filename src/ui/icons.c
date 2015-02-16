/**
 * @file icons.c  Using icons from theme and package pixmaps
 *
 * Copyright (C) 2010-2014 Lars Windolf <lars.windolf@gmx.de>
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

static GdkPixbuf *icons[MAX_ICONS];	/**< list of icon assignments */

static gchar *
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

static GdkPixbuf *
icon_get_from_theme (GtkIconTheme *icon_theme, const gchar *name, gint size)
{
	GError *error = NULL;
	GdkPixbuf *pixbuf;

	pixbuf = gtk_icon_theme_load_icon (icon_theme,
	                                   name, /* icon name */
	                                   size, /* size */
	                                   0,  /* flags */
	                                   &error);
	if (!pixbuf) {
		g_warning ("Couldn't load icon: %s", error->message);
		g_error_free (error);
	}
	return pixbuf;
}

void
icons_load (void)
{
	GtkIconTheme	*icon_theme;
	gint		i;
	
	/* first try to load icons from theme */
	static const gchar *iconThemeNames[] = {
		NULL,			/* ICON_UNREAD */
		"emblem-important",	/* ICON_FLAG */
		NULL,			/* ICON_AVAILABLE */
		NULL,			/* ICON_AVAILABLE_OFFLINE */
		"dialog-error",		/* ICON_UNAVAILABLE */
		NULL,			/* ICON_DEFAULT */
		"folder",		/* ICON_FOLDER */
		"folder-saved-search",	/* ICON_VFOLDER */
		NULL,			/* ICON_NEWSBIN */
		NULL,			/* ICON_EMPTY */
		NULL,			/* ICON_EMPTY_OFFLINE */
		"gtk-connect",		/* ICON_ONLINE */
		"gtk-disconnect",	/* ICON_OFFLINE */
		"mail-attachment",	/* ICON_ENCLOSURE */
		NULL
	};

	icon_theme = gtk_icon_theme_get_default ();
	for (i = 0; i < MAX_ICONS; i++)
		if (iconThemeNames[i])
			icons[i] = icon_get_from_theme (icon_theme, iconThemeNames[i], 16);

	/* and then load own default icons */
	static const gchar *iconNames[] = {
		"unread.png",		/* ICON_UNREAD */
		"flag.png",		/* ICON_FLAG */
		"available.png",	/* ICON_AVAILABLE */
		"available_offline.png",	/* ICON_AVAILABLE_OFFLINE */
		NULL,			/* ICON_UNAVAILABLE */
		"default.png",		/* ICON_DEFAULT */
		"directory.png",	/* ICON_FOLDER */
		"vfolder.png",		/* ICON_VFOLDER */
		"newsbin.png",		/* ICON_NEWSBIN */
		"empty.png",		/* ICON_EMPTY */
		"empty_offline.png",	/* ICON_EMPTY_OFFLINE */
		"online.png",		/* ICON_ONLINE */
		"offline.png",		/* ICON_OFFLINE */
		"attachment.png",	/* ICON_ENCLOSURE */
		NULL
	};

	for (i = 0; i < MAX_ICONS; i++)
		if (!icons[i])
			icons[i] = icon_create_from_file (iconNames[i]);
}

const GdkPixbuf *
icon_get (lifereaIcon icon)
{
	g_assert (NULL != *icons);		

	return icons[icon];
}


