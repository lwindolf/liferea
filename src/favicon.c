/**
 * @file favicon.c  Saving, loading and discovering favicons
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2015-2022 Lars Windolf <lars.windolf@gmx.de>
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "debug.h"
#include "favicon.h"
#include "html.h"
#include "metadata.h"

GdkPixbuf *
favicon_load_from_cache (const gchar *id, guint size)
{
	struct stat	statinfo;
	gchar		*filename;
	GdkPixbuf	*pixbuf, *result = NULL;
	GError 		*error = NULL;

	filename = common_create_cache_filename ("favicons", id, "png");

	if (0 == stat ((const char*)filename, &statinfo)) {
		pixbuf = gdk_pixbuf_new_from_file (filename, &error);
		if (pixbuf && !error) {
			result = gdk_pixbuf_scale_simple (pixbuf, size, size, GDK_INTERP_BILINEAR);
			g_object_unref (pixbuf);
		} else { /* Error */
			fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
			        filename, error->message);
			g_error_free (error);
		}
	}
	g_free (filename);

	return result;
}

void
favicon_remove_from_cache (const gchar *id)
{
	gchar		*filename;

	/* try to load a saved favicon */
	filename = common_create_cache_filename ("favicons", id, "png");
	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		if (0 != unlink (filename))
			g_warning ("Removal of %s failed", filename);
	}
	g_free (filename);
}

/* prevent saving overly huge favicons loaded from net */
static void
favicon_pixbuf_size_prepared_cb (GdkPixbufLoader *loader, gint width, gint height, gpointer user_data)
{
	gint max_size = 256;

	debug2 (DEBUG_UPDATE, "   - favicon size is %d:%d", width, height);
	if (width > max_size || height > max_size) {
		width = width < max_size ? width : max_size;
		height = height < max_size ? height : max_size;
		gdk_pixbuf_loader_set_size(loader, width, height);
	}
}

gboolean
favicon_save_from_data (const struct updateResult * const result, const gchar *id)
{
	GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
	GdkPixbuf	*pixbuf;
	GError		*err = NULL;
	gboolean	success = FALSE;

	g_signal_connect (loader, "size-prepared",
	                  G_CALLBACK (favicon_pixbuf_size_prepared_cb), NULL);

	if (gdk_pixbuf_loader_write (loader, (guchar *)result->data, (gsize)result->size, &err)) {
		if (gdk_pixbuf_loader_close (loader, &err)) {
			pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
			if (pixbuf) {
				gchar *tmp = common_create_cache_filename ("favicons", id, "png");
				debug2 (DEBUG_UPDATE, "saving favicon %s to file %s", id, tmp);
				if (!gdk_pixbuf_save (pixbuf, tmp, "png", &err, NULL)) {
					g_warning ("Could not save favicon (id=%s) to file %s!", id, tmp);
				} else {
					success = TRUE;
				}
				g_free (tmp);
			} else {
				debug0 (DEBUG_UPDATE, "gdk_pixbuf_loader_get_pixbuf() failed!");
			}
		} else {
			debug0 (DEBUG_UPDATE, "gdk_pixbuf_loader_close() failed!");
		}
	} else {
		debug0 (DEBUG_UPDATE, "gdk_pixbuf_loader_write() failed!");
		gdk_pixbuf_loader_close (loader, NULL);
	}

	if (err) {
		debug1 (DEBUG_UPDATE, "%s", err->message);
		g_error_free (err);
	}

	g_object_unref (loader);
	return success;
}

static gint
count_slashes (const gchar *str)
{
	const gchar	*tmp = str;
	gint		slashes = 0;

	slashes = 0;
	while (*tmp) { if (*tmp == '/') slashes++;tmp++; }

	return slashes;
}

/*
 * This code tries to download from a series of URLs. If there are no
 * favicons, this will make five downloads, three of which will be 404
 * errors. Hopefully this will not cause any webservers pain because
 * this code should be run only once a month per feed.
 *
 * 1. --> downloading favicon from the feed (e.g. <icon> tag in atom feeds)
 * 2. --> downloading HTML of the feed url and looking for a favicon reference
 * 3. --> downloading HTML of root of webserver and looking for a favicon reference
 * 4. --> downloading favicon from the root HTML
 * 5. --> downloading favicon from directory of RSS feed
 * 6. --> downloading favicon from root of webserver of the RSS feed
 */
GSList *
favicon_get_urls (subscriptionPtr subscription, const gchar *html_url)
{
	GSList		*urls = NULL;
	gchar		*tmp, *tmp2;
	const gchar	*source_url = subscription->source;

	/* case 1: the feed parser passed us an icon URL in the subscription metadata */
	if (metadata_list_get (subscription->metadata, "icon")) {
		tmp = g_strstrip (g_strdup (metadata_list_get (subscription->metadata, "icon")));
		urls = g_slist_append (urls, tmp);
		debug1 (DEBUG_UPDATE, "(1) adding favicon search URL: %s", tmp);
	}

	/* case 2: */
	if (html_url && g_strstr_len (html_url, -1, "://")) {
		tmp = g_strstrip (g_strdup (html_url));
		urls = g_slist_append (urls, tmp);
		debug1 (DEBUG_UPDATE, "(2) adding favicon search URL: %s", tmp);
	}

	/* case 3: */
	g_assert (source_url);
	if (*source_url != '|') {
		tmp = tmp2 = g_strstrip (g_strdup (source_url));

		if (strlen(tmp) && tmp[strlen (tmp) - 1] == '/')
			tmp[strlen (tmp) - 1] = 0;	/* Strip trailing slash */

		tmp = strrchr (tmp, '/');
		if (tmp) {
			*tmp = 0;
			urls = g_slist_append (urls, g_strdup (tmp2));
			debug1 (DEBUG_UPDATE, "(3) adding favicon search URL: %s", tmp2);
		}
		g_free (tmp2);
	}

	/* case 4: */
	if (html_url) {
		if (2 < count_slashes(html_url)) {
			tmp = tmp2 = g_strstrip (g_strdup (html_url));
			tmp = strstr (tmp, "://");
			if (tmp) {
				tmp = strchr (tmp + 3, '/');
				if (tmp) {
					*tmp = 0;
					tmp = tmp2;
					tmp2 = g_strdup_printf ("%s/favicon.ico", tmp);
					urls = g_slist_append (urls, tmp2);
					debug1 (DEBUG_UPDATE, "(4) adding favicon source URL: %s", tmp2);
				}
			}
			g_free (tmp);
		}
	}

	if (*source_url != '|' && 2 < count_slashes(source_url)) {
		/* case 5: */
		tmp = tmp2 = g_strstrip (g_strdup (source_url));
		tmp = strrchr(tmp, '/');
		if (tmp) {
			*tmp = 0;
			tmp = tmp2;
			tmp2 = g_strdup_printf ("%s/favicon.ico", tmp);
			urls = g_slist_append (urls, tmp2);
			debug1(DEBUG_UPDATE, "(5) adding favicon source URL: %s", tmp2);
		}
		g_free (tmp);

		/* case 6: */
		tmp = tmp2 = g_strstrip (g_strdup (source_url));
		tmp = strstr(tmp, "://");
		if (tmp) {
			tmp = strchr (tmp + 3, '/');	/* to skip to first subpath */
			if (tmp) {
				*tmp = 0;
				tmp = tmp2;
				tmp2 = g_strdup_printf ("%s/favicon.ico", tmp);
				urls = g_slist_append (urls, tmp2);
				debug1 (DEBUG_UPDATE, "(6) adding favicon source URL: %s", tmp2);
			}
		}
		g_free (tmp);
	}
	return urls;
}
