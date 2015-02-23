/**
 * @file favicon.c Liferea favicon handling
 * 
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2015 Lars Windolf <lars.windolf@gmx.de>
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
#include "feed.h"
#include "html.h"
#include "metadata.h"

typedef struct faviconDownloadCtxt {
	gchar		        *id;		/**< favicon cache id */
	GSList			*urls;		/**< ordered list of URLs to try */
	updateOptionsPtr	options;	/**< download options */
	faviconUpdatedCb	callback;	/**< usually feed_favicon_updated() */
	gpointer		user_data;	/**< usually the node pointer */
} *faviconDownloadCtxtPtr;

static faviconDownloadCtxtPtr
favicon_download_ctxt_new () 
{
	return g_new0 (struct faviconDownloadCtxt, 1);
}

static void
favicon_download_ctxt_free (faviconDownloadCtxtPtr ctxt)
{
	GSList  *iter; 

	if (!ctxt) return;
	g_free (ctxt->id);
	
	for (iter = ctxt->urls; iter; iter = g_slist_next (iter))
		g_free (iter->data);

	g_slist_free (ctxt->urls);
	update_options_free (ctxt->options);
}

static void favicon_download_run(faviconDownloadCtxtPtr ctxt);

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

gboolean
favicon_update_needed(const gchar *id, updateStatePtr updateState, GTimeVal *now)
{
	gboolean	result = FALSE;

	/* check creation date and update favicon if older than one month */
	if (now->tv_sec > (updateState->lastFaviconPoll.tv_sec + 60*60*24*31))
		result = TRUE;

	return result;
}

void favicon_remove_from_cache(const gchar *id) {
	gchar		*filename;

	debug_enter("favicon_remove");
	
	/* try to load a saved favicon */
	filename = common_create_cache_filename ("favicons", id, "png");
	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		if(0 != unlink(filename))
			g_warning ("Removal of %s failed", filename);
	}
	g_free(filename);

	debug_exit("favicon_remove");
}

static void
favicon_download_icon_cb (const struct updateResult * const result, gpointer user_data, updateFlags flags)
{
	faviconDownloadCtxtPtr	ctxt = (faviconDownloadCtxtPtr)user_data;
	gchar		*tmp;
	GError		*err = NULL;
	gboolean	success = FALSE;
	
	debug4 (DEBUG_UPDATE, "icon download processing (%s, %d bytes, content type %s) for favicon %s", result->source, result->size, result->contentType, ctxt->id);

	if (result->data && 
	    result->size > 0 && 
	    result->contentType /*&&*/
	   /* the MIME type is wrong much too often, so we cannot check it... */
	   /*(!strncmp("image", result->contentType, 5))*/) {
		GdkPixbufLoader *loader = gdk_pixbuf_loader_new ();
		GdkPixbuf *pixbuf;
		if (gdk_pixbuf_loader_write (loader, (guchar *)result->data, (gsize)result->size, &err)) {
			if (gdk_pixbuf_loader_close (loader, &err)) {
				pixbuf = gdk_pixbuf_loader_get_pixbuf (loader);
				if (pixbuf) {
					tmp = common_create_cache_filename ("favicons", ctxt->id, "png");
					debug2 (DEBUG_UPDATE, "saving favicon %s to file %s", ctxt->id, tmp);
					if (!gdk_pixbuf_save (pixbuf, tmp, "png", &err, NULL)) {
						g_warning ("Could not save favicon (id=%s) to file %s!", ctxt->id, tmp);
					} else {
						success = TRUE;
						/* Run favicon-updated callback */
						if (ctxt->callback)
							(ctxt->callback) (ctxt->user_data);
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
	} else {
		debug1 (DEBUG_UPDATE, "No data in download result for favicon %s!", ctxt->id);
	}
	
	if (!success) {
		favicon_download_run (ctxt);	/* try next... */
	} else {
		favicon_download_ctxt_free (ctxt);
	}
}

static void
favicon_download_html_cb (const struct updateResult * const result, gpointer user_data, updateFlags flags) {
	faviconDownloadCtxtPtr	ctxt = (faviconDownloadCtxtPtr)user_data;
	
	if (result->size > 0 && result->data) {
		gchar *iconUri = html_discover_favicon (result->data, result->source);
		if (iconUri) {
			updateRequestPtr request;
			
			debug2 (DEBUG_UPDATE, "found link for favicon %s: %s", ctxt->id, iconUri);
			request = update_request_new ();
			request->source = iconUri;
			request->options = update_options_copy (ctxt->options);
			update_execute_request (ctxt->user_data, request, favicon_download_icon_cb, ctxt, flags);
			
			return;
		}
	}
	debug1 (DEBUG_UPDATE, "No link for favicon %s found!", ctxt->id);

	favicon_download_run (ctxt);	/* no sucess, try next... */
}

static void
favicon_download_run (faviconDownloadCtxtPtr ctxt)
{
	gchar			*url;
	updateRequestPtr	request;
	update_result_cb	callback;

	debug_enter("favicon_download_run");
	
	if (ctxt->urls) {
		url = (gchar *)ctxt->urls->data;
		ctxt->urls = g_slist_remove (ctxt->urls, url);
		debug2 (DEBUG_UPDATE, "favicon %s trying URL: %s", ctxt->id, url);

		request = update_request_new ();
		request->source = url;
		request->options = update_options_copy (ctxt->options);

		if (strstr (url, "/favicon.ico"))
			callback = favicon_download_icon_cb;	
		else
			callback = favicon_download_html_cb;

		update_execute_request (ctxt->user_data, request, callback, ctxt, FEED_REQ_PRIORITY_HIGH);
	} else {
		debug1 (DEBUG_UPDATE, "favicon %s could not be downloaded!", ctxt->id);
		/* Run favicon-updated callback */
		if (ctxt->callback)
			(ctxt->callback) (ctxt->user_data);
		favicon_download_ctxt_free (ctxt);
	}
	
	debug_exit ("favicon_download_run");
}

static gint count_slashes(const gchar *str) {
	const gchar	*tmp = str;
	gint		slashes = 0;
	
	slashes = 0;
	while(*tmp) { if(*tmp == '/') slashes++;tmp++; }
	
	return slashes;
}

void
favicon_download (subscriptionPtr subscription,
		  const gchar *html_url,
		  const gchar *source_url,
		  const updateOptionsPtr options,
		  faviconUpdatedCb callback,
		  gpointer user_data)
{
	const gchar		*id;
	faviconDownloadCtxtPtr	ctxt;
	gchar			*tmp, *tmp2;

	debug_enter("favicon_download");

	id = subscription->node->id;

	ctxt = favicon_download_ctxt_new ();
	ctxt->id = g_strdup (id);
	ctxt->options = update_options_copy (options);
	ctxt->callback = callback;
	ctxt->user_data = user_data;

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

	/* In the following These URLs will be prepared here and passed as a list to 
	   the download function that will then process in order
	   until success or the end of the list is reached... */
	debug1(DEBUG_UPDATE, "preparing download URLs for favicon %s...", ctxt->id);

	/* case 1. */
	if (metadata_list_get (subscription->metadata, "icon")) {
		tmp = g_strstrip (g_strdup (metadata_list_get (subscription->metadata, "icon")));
		ctxt->urls = g_slist_append (ctxt->urls, tmp);
		debug1 (DEBUG_UPDATE, "(1) adding favicon search URL: %s", tmp);
	}

	/* case 2. */
	if(html_url) {
		tmp = g_strstrip (g_strdup (html_url));
		ctxt->urls = g_slist_append(ctxt->urls, tmp);
		debug1(DEBUG_UPDATE, "(2) adding favicon search URL: %s", tmp);
	}

	/* case 3. */
	g_assert(source_url);
	if(*source_url != '|') {
		tmp = tmp2 = g_strstrip (g_strdup (source_url));

		if(tmp[strlen(tmp) - 1] == '/')
			tmp[strlen(tmp) - 1] = 0;	/* Strip trailing slash */

		tmp = strrchr(tmp, '/');
		if(tmp) {
			*tmp = 0;
			ctxt->urls = g_slist_append(ctxt->urls, tmp2);
			debug1(DEBUG_UPDATE, "(3) adding favicon search URL: %s", tmp2);
		}
	}

	/* case 4. */
	if(html_url) {
		if(2 < count_slashes(html_url)) {
			tmp = tmp2 = g_strstrip (g_strdup (html_url));
			tmp = strstr(tmp, "://");
			if(tmp) {
				tmp = strchr(tmp + 3, '/');
				if(tmp) {
					*tmp = 0;
					tmp = tmp2;
					tmp2 = g_strdup_printf("%s/favicon.ico", tmp);
					ctxt->urls = g_slist_append(ctxt->urls, tmp2);
					debug1(DEBUG_UPDATE, "(4) adding favicon source URL: %s", tmp2);
				}
			}
			g_free(tmp);
		}
	}

	if(*source_url != '|' && 2 < count_slashes(source_url)) {
		/* case 5 */
		tmp = tmp2 = g_strstrip (g_strdup (source_url));
		tmp = strrchr(tmp, '/');
		if(tmp) {
			*tmp = 0;
			tmp = tmp2;
			tmp2 = g_strdup_printf("%s/favicon.ico", tmp);
			ctxt->urls = g_slist_append(ctxt->urls, tmp2);
			debug1(DEBUG_UPDATE, "(5) adding favicon source URL: %s", tmp2);
		}
		g_free(tmp);

		/* case 6 */
		tmp = tmp2 = g_strstrip (g_strdup (source_url));
		tmp = strstr(tmp, "://");
		if(tmp) {
			tmp = strchr(tmp + 3, '/');	/* to skip to first subpath */
			if(tmp) {
				*tmp = 0;
				tmp = tmp2;
				tmp2 = g_strdup_printf("%s/favicon.ico", tmp);
				ctxt->urls = g_slist_append(ctxt->urls, tmp2);
				debug1(DEBUG_UPDATE, "(6) adding favicon source URL: %s", tmp2);
			}
		}
		g_free(tmp);
	}

	favicon_download_run(ctxt);

	debug_exit("favicon_download");
}
