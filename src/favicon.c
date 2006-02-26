/**
 * @file favicon.c Liferea favicon handling
 * 
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include "ui/ui_feedlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_feed.h"

void favicon_load(nodePtr np) {
	struct stat	statinfo;
	GTimeVal	now;
	gchar		*filename;
	GdkPixbuf	*pixbuf;
	GError 		*error = NULL;

	debug_enter("favicon_load");
	
	/* try to load a saved favicon */
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", node_get_id(np), "png");
	
	if(0 == stat((const char*)filename, &statinfo)) {
		pixbuf = gdk_pixbuf_new_from_file(filename, &error);
		if(pixbuf != NULL) {
			if(np->icon != NULL)
				g_object_unref(np->icon);
			np->icon = gdk_pixbuf_scale_simple(pixbuf, 16, 16, GDK_INTERP_BILINEAR);
			g_object_unref(pixbuf);
		} else { /* Error */
			fprintf(stderr, "Failed to load pixbuf file: %s: %s\n",
			        filename, error->message);
			g_error_free(error);
		}
		
		/* check creation date and update favicon if older than one month */
		g_get_current_time(&now);
		if(now.tv_sec > (((feedPtr)(np->data))->lastFaviconPoll.tv_sec + 60*60*24*31)) {
			debug1(DEBUG_UPDATE, "updating favicon %s\n", filename);
			favicon_download(np);
		}
	}
	g_free(filename);	

	debug_exit("favicon_load");
}

void favicon_remove(nodePtr np) {
	gchar		*filename;

	debug_enter("favicon_remove");
	
	/* try to load a saved favicon */
	filename = common_create_cache_filename( "cache" G_DIR_SEPARATOR_S "favicons", node_get_id(np), "png");
	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		if(0 != unlink(filename))
			/* What can we do? The file probably doesn't exist. Or permissions are wrong. Oh well.... */;
	}
	g_free(filename);

	debug_exit("favicon_remove");
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
static void favicon_download_html(nodePtr node, int phase);

static void favicon_download_5(nodePtr node) {
	feedPtr		feed = (feedPtr)node->data;
	gchar 		*baseurl, *tmp;
	struct request	*request;
	
	baseurl = g_strdup(feed_get_source(feed));
	if(tmp = strstr(baseurl, "://")) {
		tmp += 3;
		if(tmp = strchr(tmp, '/')) {
			*tmp = 0;
			request = download_request_new(NULL);
			request->source = g_strdup_printf("%s/favicon.ico", baseurl);
			
			request->callback = &favicon_download_request_favicon_cb;
			request->user_data = node;
			request->flags = 5;
			node->requests = g_slist_append(node->requests, request);
			
			debug1(DEBUG_UPDATE, "trying to download server root favicon.ico for \"%s\"\n", request->source);
			
			download_queue(request);
		}
	}
	g_free(baseurl);
}

static void favicon_download_4(nodePtr node) {
	feedPtr		feed = (feedPtr)node->data;
	gchar 		*baseurl, *tmp;
	struct request	*request;
	
	baseurl = g_strdup(feed_get_source(feed));
	if(tmp = strstr(baseurl, "://")) {
		tmp += 3;
		if(tmp = strrchr(tmp, '/')) {
			*tmp = 0;
			
			request = download_request_new(NULL);
			request->source = g_strdup_printf("%s/favicon.ico", baseurl);
			request->callback = &favicon_download_request_favicon_cb;
			request->user_data = node;
			request->flags = 4;
			node->requests = g_slist_append(node->requests, request);
			
			debug1(DEBUG_UPDATE, "trying to download favicon.ico for \"%s\"\n", request->source);
			
			download_queue(request);
		}
	}
	g_free(baseurl);
}

static void favicon_download_request_favicon_cb(struct request *request) {
	nodePtr		node = (nodePtr)request->user_data;
	feedPtr		feed = (feedPtr)node->data;
	gchar		*tmp;
	GError		*err = NULL;
	gboolean	success = FALSE;
	
	debug2(DEBUG_UPDATE, "icon download processing (%s, %d bytes)", request->source, request->size);
	node->requests = g_slist_remove(node->requests, request);
	
	if(NULL != request->data && request->size > 0) {
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new();
		GdkPixbuf *pixbuf;
		tmp = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", node_get_id(node), "png");
		if(gdk_pixbuf_loader_write(loader, (guchar *)request->data, request->size, &err)) {
			if(pixbuf = gdk_pixbuf_loader_get_pixbuf(loader)) {
				debug1(DEBUG_UPDATE, "saving icon as %s", tmp);
				if(FALSE == (gdk_pixbuf_save(pixbuf, tmp, "png", &err, NULL))) {
					g_warning("favicon saving error!");
				}
				success = TRUE;
				favicon_load(node);
			}
		}

		if(err) {
			g_warning("%s\n", err->message);
			g_error_free(err);
		}

		gdk_pixbuf_loader_close(loader, NULL);
		g_object_unref(loader);
		g_free(tmp);
		ui_node_update(node);
	}
	
	if(!success) {
		if(request->flags == 1)
			favicon_download_html(node, 2);
		else if(request->flags == 3) {
			favicon_download_4(node);
		} else if(request->flags == 4) {
			favicon_download_5(node);
		}
	}
}

static void favicon_download_html_request_cb(struct request *request) {
	gchar		*iconUri;
	struct request	*request2 = NULL;
	nodePtr		node = (nodePtr)request->user_data;
	feedPtr		feed = (feedPtr)node->data;
	
	node->requests = g_slist_remove(node->requests, request);
		
	if(request->size > 0 && request->data != NULL) {
		iconUri = html_discover_favicon(request->data, request->source);
		if(iconUri) {
			request2 = download_request_new(NULL);
			request2->source = iconUri;
			request2->callback = &favicon_download_request_favicon_cb;
			request2->user_data = node;
			request2->flags++;
			node->requests = g_slist_append(node->requests, request2);
			download_queue(request2);
		}
	}

	if(request2) {
		if(request->flags == 0)
			favicon_download_html(node, 2);
		else /* flags == 2 */
			favicon_download_4(node);
	}
}

static void favicon_download_html(nodePtr node, int phase) {
	gchar		*htmlurl, *tmp;
	struct request	*request;
	feedPtr		feed = (feedPtr)node->data;
	
	/* try to download favicon */
	if(phase == 0) {
		htmlurl = g_strdup(feed_get_html_url(feed));
	} else {
		htmlurl = g_strdup(feed_get_source(feed));
		if(tmp = strstr(htmlurl, "://")) {
			tmp += 3;
			/* first we try to download a favicon inside the current web path
			   if the download fails the callback will try to strip parts of
			   the URL to download a root favicon. */
			if(tmp = strrchr(tmp, '/')) {
				*tmp = 0;
			}
		}
	}
	
	request = download_request_new(NULL);
	request->source = htmlurl;
	request->callback = &favicon_download_html_request_cb;
	request->user_data = node;
	request->flags = phase;
	node->requests = g_slist_append(node->requests, request);	
	download_queue(request);
	
	debug_exit("favicon_download");
}

void favicon_download(nodePtr node) {
	
	debug_enter("favicon_download");
	debug1(DEBUG_UPDATE, "trying to download favicon.ico for \"%s\"\n",
	                     node_get_title(node));
	
	ui_mainwindow_set_status_bar(_("Updating feed icon for \"%s\""),
	                             node_get_title(node));

	g_get_current_time(&((feedPtr)node->data)->lastFaviconPoll);
	node->needsCacheSave = TRUE;
	
	if(feed_get_html_url(node->data))
		favicon_download_html(node, 0);
	else
		favicon_download_html(node, 2);
	
	debug_exit("favicon_download");
}
