/**
 * @file ico2xpm.c -- Convert icons to pixmaps
 * Copyright (C) 1998 Philippe Martin
 * Modified by Brion Vibber
 * 
 * Modified to be suitable for Liferea
 * Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "favicon.h"
#include "support.h"
#include "feed.h"
#include "common.h"
#include "update.h"
#include "update.h"
#include "debug.h"

#ifdef STDC_HEADERS
#include <stdlib.h>
#endif

/* Liferea specific wrapper functions */

void favicon_load(feedPtr fp) {
	gchar		*pngfilename, *xpmfilename;
	GdkPixbuf	*pixbuf;
	GError *error = NULL;
	
	/* try to load a saved favicon */
	pngfilename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons",fp->id, "png");
	xpmfilename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons",fp->id, "xpm");
	
	if(g_file_test(pngfilename, G_FILE_TEST_EXISTS)) {
		pixbuf = gdk_pixbuf_new_from_file (pngfilename, &error);
		if (pixbuf) {
			fp->icon = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
			g_object_unref(pixbuf);
		} else { /* Error */
			fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
				    pngfilename, error->message);
			g_error_free (error);
		}
	} else {
		if(g_file_test(xpmfilename, G_FILE_TEST_EXISTS)) {
			pixbuf = gdk_pixbuf_new_from_file (xpmfilename, &error);
			if (pixbuf) {
				fp->icon = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
				gdk_pixbuf_save(pixbuf, pngfilename, "png", NULL, NULL);
				g_object_unref(pixbuf);
			} else { /* Error */
				fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
					    xpmfilename, error->message);
				g_error_free (error);
			}
			unlink(xpmfilename);
		}
	}
	g_free(pngfilename);
	g_free(xpmfilename);
	
}

void favicon_remove(feedPtr fp) {
	gchar		*filename;
	
	/* try to load a saved favicon */
	filename = common_create_cache_filename( "cache" G_DIR_SEPARATOR_S "favicons", fp->id, "png");
	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		if(0 != unlink(filename))
			/* What can we do? The file probably doesn't exist. Or permissions are wrong. Oh well.... */;
	}
	g_free(filename);
}

static void favicon_download_request_cb(struct request *request) {
	feedPtr fp = (feedPtr)request->user_data;
	char *tmp;
	printf("favicon downloaded\n");
	if(NULL != request->data && request->size > 0) {
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
		GdkPixbuf *pixbuf;
		tmp = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", fp->id, "png");
		
		if (gdk_pixbuf_loader_write(loader, request->data, request->size, NULL)) {
			pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
			gdk_pixbuf_save(pixbuf, tmp, "png", NULL, NULL);
			favicon_load(fp);
		}
		g_free(tmp);
	}

	fp->faviconRequest = NULL;
	ui_feed_update(fp);
}

void favicon_download(feedPtr fp) {
	gchar			*baseurl;
	gchar			*tmp;
	struct request	*request;
	
	if (fp->faviconRequest != NULL)
		return; /* It is already being downloaded */

	/* try to download favicon */
	baseurl = g_strdup(fp->source);
	if(NULL != (tmp = strstr(baseurl, "://"))) {
		tmp += 3;
		if(NULL != (tmp = strchr(tmp, '/'))) {
			*tmp = 0;
			
			request = download_request_new(NULL);
			request->source = g_strdup_printf("%s/favicon.ico", baseurl);
			request->callback = &favicon_download_request_cb;
			request->user_data = fp;
			
			debug1(DEBUG_UPDATE, "trying to download favicon.ico for \"%s\"\n", request->source);

			download_queue(request);
		}
	}
	g_free(baseurl);
}

