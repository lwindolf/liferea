/**
 * @file favicon.c Liferea favicon handling
 * 
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "favicon.h"
#include "support.h"
#include "feed.h"
#include "common.h"
#include "update.h"
#include "debug.h"
#include "html.h"
#include "ui_feedlist.h"

void favicon_load(feedPtr fp) {
	struct stat	statinfo;
	GTimeVal	now;
	gchar		*pngfilename, *xpmfilename;
	GdkPixbuf	*pixbuf;
	GError 		*error = NULL;
	
	/* try to load a saved favicon */
	pngfilename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", feed_get_id(fp), "png");
	xpmfilename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", feed_get_id(fp), "xpm");
	
	if(0 == stat((const char*)pngfilename, &statinfo)) {
		pixbuf = gdk_pixbuf_new_from_file (pngfilename, &error);
		if (pixbuf != NULL) {
			if (fp->icon != NULL)
				g_object_unref(fp->icon);
			fp->icon = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
			g_object_unref(pixbuf);
		} else { /* Error */
			fprintf (stderr, "Failed to load pixbuf file: %s: %s\n",
				    pngfilename, error->message);
			g_error_free (error);
		}
		
		/* check creation date and update favicon if older than one month */
		g_get_current_time(&now);
		if(now.tv_sec > statinfo.st_mtime + 60*60*24*31) {
			debug1(DEBUG_UPDATE, "updating favicon %s\n", pngfilename);
			favicon_download(fp);
		}
	} else {
		/* FIXME: remove this migration code when time comes */
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
	filename = common_create_cache_filename( "cache" G_DIR_SEPARATOR_S "favicons", feed_get_id(fp), "png");
	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		if(0 != unlink(filename))
			/* What can we do? The file probably doesn't exist. Or permissions are wrong. Oh well.... */;
	}
	g_free(filename);
}

/*
 * This code tries to download a series of files. If there are no
 * favicons, this will make four downloads, two of which will be 404
 * errors. Hopefully this will not cause any webservers pain because
 * this code should be run only once a month per feed.
 *
 * Flag states: (stored in request->flags)
 *
 * 0 <-- downloading HTML of the feed url
 * 1 <-- downloading favicon from the feed url HTML
 * 2 <-- downloading HTML of root of webserver
 * 3 <-- downloading favicon from the root HTML
 * 4 <-- downloading favicon from directory of RSS feed
 * 5 <-- downloading favicon from root of webserver
 */

static void favicon_download_request_favicon_cb(struct request *request);
static void favicon_download_html(feedPtr fp, int phase);

static void favicon_download_5(feedPtr fp) {
	gchar *baseurl, *tmp;
	struct request *request;
	
	baseurl = g_strdup(feed_get_source(fp));
	if(NULL != (tmp = strstr(baseurl, "://"))) {
		tmp += 3;
		if(NULL != (tmp = strchr(tmp, '/'))) {
			*tmp = 0;
			request = download_request_new(NULL);
			request->source = g_strdup_printf("%s/favicon.ico", baseurl);
			
			request->callback = &favicon_download_request_favicon_cb;
			request->user_data = fp;
			request->flags = 5;
			fp->faviconRequest = request;
			
			debug1(DEBUG_UPDATE, "trying to download server root favicon.ico for \"%s\"\n", request->source);
			
			download_queue(request);
		}
	}
	g_free(baseurl);
}

static void favicon_download_4(feedPtr fp) {
	gchar *baseurl, *tmp;
	struct request *request;
	
	baseurl = g_strdup(feed_get_source(fp));
	if(NULL != (tmp = strstr(baseurl, "://"))) {
		tmp += 3;
		if(NULL != (tmp = strrchr(tmp, '/'))) {
			*tmp = 0;
			
			request = download_request_new(NULL);
			request->source = g_strdup_printf("%s/favicon.ico", baseurl);
			request->callback = &favicon_download_request_favicon_cb;
			request->user_data = fp;
			request->flags = 4;
			fp->faviconRequest = request;
			
			debug1(DEBUG_UPDATE, "trying to download favicon.ico for \"%s\"\n", request->source);
			
			download_queue(request);
		}
	}
	g_free(baseurl);
}

static void favicon_download_request_favicon_cb(struct request *request) {
	feedPtr	fp = (feedPtr)request->user_data;
	gchar	*tmp;
	gboolean success = FALSE;
	
	debug2(DEBUG_UPDATE, "icon download processing (%s, %d bytes)", request->source, request->size);
	fp->faviconRequest = NULL;
	
	if(NULL != request->data && request->size > 0) {
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
		GdkPixbuf *pixbuf;
		tmp = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", feed_get_id(fp), "png");
		
		if(gdk_pixbuf_loader_write(loader, request->data, request->size, NULL)) {
			pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
			debug1(DEBUG_UPDATE, "saving icon as %s", tmp);
			if(FALSE == (gdk_pixbuf_save(pixbuf, tmp, "png", NULL, NULL))) {
				g_warning("favicon saving error!");
			}
			success = TRUE;
			favicon_load(fp);
		}
		gdk_pixbuf_loader_close(loader, NULL);
		g_object_unref(loader);
		g_free(tmp);
		ui_feed_update(fp);
	}
	
	if (!success) {
		if (request->flags == 1)
			favicon_download_html(fp, 2);
		else if (request->flags == 3) {
			favicon_download_4(fp);
		} else if (request->flags == 4) {
			favicon_download_5(fp);
		}
	}
}

static void favicon_download_html_request_cb(struct request *request) {
	gchar *iconUri;
	struct request *request2 = NULL;
	feedPtr fp = (feedPtr)request->user_data;
	
	if (request->size > 0 && request->data != NULL) {
		iconUri = html_discover_favicon(request->data, request->source);
		if (iconUri != NULL) {
			request2 = download_request_new(NULL);
			fp->faviconRequest = request2;
			request2->source = iconUri;
			request2->callback = &favicon_download_request_favicon_cb;
			request2->user_data = fp;
			request2->flags++;
			download_queue(request2);
		}
	}
	if (request2 == NULL) {
		if (request->flags == 0)
			favicon_download_html((feedPtr)request->user_data, 2);
		else /* flags == 2 */
			favicon_download_4((feedPtr)fp);
	}
}

static void favicon_download_html(feedPtr fp, int phase) {
	gchar			*htmlurl;
	gchar			*tmp;
	struct request	*request;
	
	/* try to download favicon */
	if (phase == 0) {
		htmlurl = g_strdup(feed_get_html_url(fp));
	} else {
		htmlurl = g_strdup(feed_get_source(fp));
		if(NULL != (tmp = strstr(htmlurl, "://"))) {
			tmp += 3;
			/* first we try to download a favicon inside the current web path
			   if the download fails the callback will try to strip parts of
			   the URL to download a root favicon. */
			if(NULL != (tmp = strrchr(tmp, '/'))) {
				*tmp = 0;
			}
		}
	}
	
	request = download_request_new(NULL);
	request->source = htmlurl;
	request->callback = &favicon_download_html_request_cb;
	request->user_data = fp;
	request->flags = phase;
	fp->faviconRequest = request;
	
	download_queue(request);
	
	debug_exit("favicon_download");
}

void favicon_download(feedPtr fp) {
	debug_enter("favicon_download");
	debug1(DEBUG_UPDATE, "trying to download favicon.ico for \"%s\"\n", feed_get_source(fp));
	
	if(fp->faviconRequest != NULL)
		return; /* It is already being downloaded */

	g_get_current_time(&fp->lastFaviconPoll);
	
	if(feed_get_html_url(fp) != NULL) {
		favicon_download_html(fp, 0);
	} else {
		favicon_download_html(fp, 2);
	}
	
	debug_exit("favicon_download");
}
