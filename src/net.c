/**
 * @file net.c  HTTP network access using libsoup
 *
 * Copyright (C) 2007-2023 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2009 Emilio Pozuelo Monfort <pochu27@gmail.com>
 * Copyright (C) 2021 Lorenzo L. Ancora <admin@lorenzoancora.info>
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

#include "net.h"

#include <glib.h>
#include <libsoup/soup.h>
#include <curl/curl.h>

#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "conf.h"
#include "debug.h"

#define HOMEPAGE	"https://lzone.de/liferea/"

static ProxyDetectMode proxymode = PROXY_DETECT_MODE_AUTO;

static GMutex init_lock;

static GPrivate init_key;

static gint
get_init_count(void)
{
    return GPOINTER_TO_INT (g_private_get(&init_key));
}

static void
set_init_count(gint count)
{
    g_private_set( &init_key, GINT_TO_POINTER(count));
}

static void
network_process_redirect_callback (SoupMessage *msg, gpointer user_data)
{
	updateJobPtr	job = (updateJobPtr)user_data;
	const gchar	*location = NULL;
	GUri		*newuri;
	SoupStatus	status = soup_message_get_status (msg);

	if (SOUP_STATUS_MOVED_PERMANENTLY == status || SOUP_STATUS_PERMANENT_REDIRECT == status) {
		if (g_uri_is_valid (location, G_URI_FLAGS_PARSE_RELAXED, NULL)) {
			location = soup_message_headers_get_one (soup_message_get_response_headers (msg), "Location");
			newuri = g_uri_parse (location, G_URI_FLAGS_PARSE_RELAXED, NULL);

			if (!soup_uri_equal (newuri, soup_message_get_uri (msg))) {
				job->result->httpstatus = status;
				job->result->source = g_uri_to_string_partial (newuri, 0);
				debug (DEBUG_NET, "\"%s\" permanently redirects to new location \"%s\"",
				       job->request->source, job->result->source);
			}
		}
	}
}

static size_t
network_write_callback (void *ptr, size_t size, size_t nmemb, void *data)
{
        int realsize = size * nmemb;
        updateResultPtr result = (updateResultPtr)data;

        if (result->data == NULL)
            result->data = g_malloc0 (realsize+2);
        else
            result->data = g_realloc (result->data, result->size + realsize + 2);

        if (result->data) {
            memcpy (result->data + result->size, ptr, realsize);
            result->size += realsize;
            result->data[result->size] = 0;
        } else {
            realsize = 0;
        }

        return realsize;
}

static void
network_process_callback (CURL *curl_handle, updateJobPtr job)
{
	GDateTime		*last_modified;
	const gchar		*tmp = NULL;
	GHashTable		*params;
	gboolean		revalidated = FALSE;
	gint			maxage;
	gint			age;
        long   info;
        gchar  *infoval;

        if (CURLE_OK == curl_easy_getinfo (curl_handle, CURLINFO_CONTENT_TYPE, &infoval))
                job->result->contentType = g_strdup (infoval);
        if (CURLE_OK == curl_easy_getinfo (curl_handle, CURLINFO_EFFECTIVE_URL, &infoval))
                job->result->source = g_strdup (infoval);
        if (CURLE_OK == curl_easy_getinfo (curl_handle, CURLINFO_RESPONSE_CODE, &info))
                job->result->httpstatus = (guint)info;
        if (CURLE_OK == curl_easy_getinfo (curl_handle, CURLINFO_FILETIME, &info)) {
                info - (-1 == info) ? 0 : info;
                update_state_set_lastmodified (job->result->updateState, info);
        }

	debug (DEBUG_NET, "download status code: %d", job->result->httpstatus);
	debug (DEBUG_NET, "source after download: >>>%s<<<", job->result->source);
	debug (DEBUG_NET, "%d bytes downloaded", job->result->size);

        if (400 < job->result->httpstatus) {
            // is this correct ?
            g_free(job->result->data);
            job->result->data = NULL;
            job->result->size = 0;
            curl_easy_cleanup(curl_handle);
            return;
        }

	/* keep some request headers for revalidated responses */
	revalidated = (304 == job->result->httpstatus);

        struct curl_header *header;

	/* Update last-modified date */
	if (revalidated) {
		job->result->updateState->lastModified = update_state_get_lastmodified (job->request->updateState);
	} else if (CURLE_OK == curl_easy_header(curl_handle, "Last-Modified", 0, CURLH_HEADER, 0, &header)) {
                last_modified = soup_date_time_new_from_http_string (header->value);
                if (last_modified) {
                        job->result->updateState->lastModified = g_date_time_to_unix (last_modified);
                        g_date_time_unref (last_modified);
                }
	}

	/* Update ETag value */
	if (revalidated) {
		job->result->updateState->etag = g_strdup (update_state_get_etag (job->request->updateState));
	} else if (CURLE_OK == curl_easy_header(curl_handle, "ETag", 0, CURLH_HEADER, 0, &header)) {
                job->result->updateState->etag = g_strdup (header->value);
	}

	/* Update cache max-age  */
	if (CURLE_OK == curl_easy_header(curl_handle, "Cache-Control", 0, CURLH_HEADER, 0, &header)) {
		params = soup_header_parse_param_list (header->value);
		if (params) {
			tmp = g_hash_table_lookup (params, "max-age");
			if (tmp) {
				maxage = atoi (tmp);
				if (0 < maxage) {
					/* subtract Age from max-age */
                                        if (CURLE_OK == curl_easy_header(curl_handle, "Age", 0, CURLH_HEADER, 0, &header)) {
						age = atoi (header->value);
						if (0 < age) {
							maxage = maxage - age;
						}
					}
					if (0 < maxage) {
						job->result->updateState->maxAgeMinutes = ceil ( (float) (maxage / 60));
					}
				}
			}
		}
		soup_header_free_param_list (params);
	}
        curl_easy_cleanup(curl_handle);
}

struct curl_slist *
slist_append(struct curl_slist *head, char *str, gchar *val)
{
    gchar *value = g_strdup_printf("%s: %s", str, val);
    head = curl_slist_append(head, value);
    g_free(value);
    return head;
}

/* Downloads a URL specified in the request structure, returns
   the downloaded data or NULL in the request structure.
   If the webserver reports a permanent redirection, the
   URL will be modified and the old URL 'll be freed. The
   request structure will also contain the HTTP status and the
   last modified string.
 */
void
network_process_request (const updateJobPtr job)
{
	g_autoptr(GUri)		sourceUri;
	gboolean		do_not_track = FALSE, do_not_sell = false;
	g_autofree gchar	*scheme = NULL, *user = NULL, *password = NULL, *auth_params = NULL, *host = NULL, *path = NULL, *query = NULL, *fragment = NULL;
	gint			port;

	g_assert (NULL != job->request);
	debug (DEBUG_NET, "downloading %s", job->request->source);
	if (job->request->postdata && (debug_get_flags () & DEBUG_NET))
		debug (DEBUG_NET, "   postdata=>>>%s<<<", job->request->postdata);

	g_uri_split_with_user (job->request->source,
	                       G_URI_FLAGS_ENCODED,
			       &scheme,
			       &user,
			       &password,
			       &auth_params,
			       &host,
			       &port,
			       &path,
			       &query,
			       &fragment,
			       NULL);

	sourceUri = g_uri_build_with_user (
		SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED,
		scheme,
                NULL,
                NULL,
		auth_params,
		host,
		port,
		path,
		query,
		fragment
	);

	if (!sourceUri) {
		g_warning ("The request for %s could not be parsed! (%s)", job->request->source, sourceUri);
		return;
        }

        struct curl_slist *header = NULL;

        CURL *curl_handle = curl_easy_init ();
        curl_easy_setopt (curl_handle, CURLOPT_URL, job->request->source);
        curl_easy_setopt (curl_handle, CURLOPT_WRITEFUNCTION, network_write_callback);
        curl_easy_setopt (curl_handle, CURLOPT_WRITEDATA, job->result);
        curl_easy_setopt (curl_handle, CURLOPT_PRIVATE, job);
        curl_easy_setopt (curl_handle, CURLOPT_AUTOREFERER, 1);
        curl_easy_setopt (curl_handle, CURLOPT_USERAGENT, network_get_user_agent());
        curl_easy_setopt (curl_handle, CURLOPT_FOLLOWLOCATION,  TRUE);
        curl_easy_setopt (curl_handle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
        curl_easy_setopt (curl_handle, CURLOPT_TIMEOUT, 120);

        if (job->request->authValue && job->request->options 
            && job->request->options->username && job->request->options->password) {
            gchar *authstr = g_strdup_printf ("%s:%s", job->request->options->username, job->request->options->password);
            curl_easy_setopt (curl_handle, CURLOPT_HTTPAUTH, CURLAUTH_ANY);
            curl_easy_setopt (curl_handle, CURLOPT_USERPWD, authstr);
            g_free (authstr);
        }

        /* Session cookies */
        gchar *filename = common_create_config_filename ("session_cookies.txt");
        curl_easy_setopt (curl_handle, CURLOPT_COOKIEJAR, filename);
        g_free(filename);

        // user-agent
        gchar *useragent = network_get_user_agent ();
        curl_easy_setopt (curl_handle, CURLOPT_USERAGENT, useragent);
        g_free(useragent);

        /* Set the postdata for the request */
        if (job->request->postdata)
            curl_easy_setopt (curl_handle, CURLOPT_POSTFIELDS, job->request->postdata);     

        curl_easy_setopt (curl_handle, CURLOPT_FILETIME, 1L);

        /* Set the If-Modified-Since: header */
        if (job->request->updateState && update_state_get_lastmodified (job->request->updateState)) {
                curl_easy_setopt (curl_handle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
                curl_easy_setopt (curl_handle, CURLOPT_TIMEVALUE, update_state_get_lastmodified (job->request->updateState));
        }

        /* Set the If-None-Match header */
        if (0 && job->request->updateState && update_state_get_etag (job->request->updateState)) {
                header = slist_append(header, "If-None-Match", update_state_get_etag (job->request->updateState));
        }

        /* Set the I-AM header */
        if (job->request->updateState &&
            (update_state_get_lastmodified (job->request->updateState) || update_state_get_etag (job->request->updateState))) {
                header = curl_slist_append(header, "A-IM: feed");
        }

        /* Support HTTP content negotiation */
        header = curl_slist_append(header, "Accept: application/atom+xml,application/xml;q=0.9,text/xml;q=0.8,*/*;q=0.7");

        /* Add Authorization header */
        if (job->request->authValue) {
            header = slist_append(header, "Authorization", job->request->authValue);
        }

        /* Add requested cookies */
        if (job->request->updateState && job->request->updateState->cookies) {
                curl_easy_setopt (curl_handle, CURLOPT_COOKIE, update_state_get_cookies (job->request->updateState));
        }

        /* Add Do Not Track header according to settings */
        do_not_track = conf_get_bool_value (DO_NOT_TRACK, &do_not_track);
        if (do_not_track) {
                header = curl_slist_append(header, "DNT: 1");
                header = curl_slist_append(header, "Sec-GPC: 1");
        }

        /* Process permanent redirects (update feed location) */
        // soup_message_add_status_code_handler (msg, "got_body", 301, (GCallback) network_process_redirect_callback, job);
        // soup_message_add_status_code_handler (msg, "got_body", 308, (GCallback) network_process_redirect_callback, job);

        curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, header);

        gchar *dbgerr = g_malloc0 (CURL_ERROR_SIZE);
        curl_easy_setopt(curl_handle, CURLOPT_ERRORBUFFER, dbgerr);

        // FIX-ME  proxy://
        // gsettings list-recursively org.gnome.system.proxy

        CURLcode ccode;

        if (CURLE_OK != (ccode = curl_easy_perform(curl_handle))) {
		g_warning ("The request for %s could not performed (%d)", job->request->source, ccode);
		debug (DEBUG_NET, "ERROR INFO: %s", dbgerr);
                g_free(dbgerr);
                curl_easy_cleanup(curl_handle);
                return;
        }
        g_free(dbgerr);
        curl_slist_free_all(header);

        network_process_callback(curl_handle, job);
}

static void
network_set_soup_session_proxy (SoupSession *session, ProxyDetectMode mode)
{
	switch (mode) {
		case PROXY_DETECT_MODE_MANUAL:
			/* Manual mode is not supported anymore, so we fall through to AUTO */
		case PROXY_DETECT_MODE_AUTO:
			debug (DEBUG_CONF, "proxy auto detect is configured");
			soup_session_set_proxy_resolver (session, g_object_ref (g_proxy_resolver_get_default ()));
			break;
		case PROXY_DETECT_MODE_NONE:
			debug (DEBUG_CONF, "proxy is disabled by user");
			soup_session_set_proxy_resolver (session, NULL);
			break;
	}
}

gchar *
network_get_user_agent (void)
{
	gchar *useragent = NULL;
	gchar const *sysua = g_getenv("LIFEREA_UA");
	
	if(sysua == NULL) {
		bool anonua = g_getenv("LIFEREA_UA_ANONYMOUS") != NULL;
		if(anonua) {
			/* Set an anonymized, randomic user agent,
			 * e.g. "Liferea/0.28.0 (Android; Mobile; https://lzone.de/liferea/) AppleWebKit (KHTML, like Gecko)" */
			useragent = g_strdup_printf ("Liferea/%.2f.0 (Android; Mobile; %s) AppleWebKit (KHTML, like Gecko)", g_random_double(), HOMEPAGE);
		} else {
			/* Set an exact user agent,
			 * e.g. "Liferea/1.10.0 (Android 14; Mobile; https://lzone.de/liferea/) AppleWebKit (KHTML, like Gecko)" */
			useragent = g_strdup_printf ("Liferea/%s (Android 14; Mobile; %s) AppleWebKit (KHTML, like Gecko)", VERSION, HOMEPAGE);
		}
	} else {
		/* Set an arbitrary user agent from the environment variable LIFEREA_UA */
		useragent = g_strdup (sysua);
	}

	g_assert_nonnull (useragent);
	
	return useragent;
}

void
network_deinit (void)
{
}

void
network_init (void)
{
	gchar		*useragent;
	SoupCookieJar	*cookies;
	gchar		*filename;
	SoupLogger	*logger;

        g_mutex_lock(&init_lock);
        curl_global_init(CURL_GLOBAL_ALL);
        g_mutex_unlock(&init_lock);

        if (1 == get_init_count())
            return;

        set_init_count(1);

	useragent = network_get_user_agent ();
	debug (DEBUG_NET, "user-agent set to \"%s\"", useragent);

	g_free (useragent);
}

ProxyDetectMode
network_get_proxy_detect_mode (void)
{
	return proxymode;
}

extern void network_monitor_proxy_changed (void);

void
network_set_proxy (ProxyDetectMode mode)
{
	proxymode = mode;

	network_monitor_proxy_changed ();
}

const char *
network_strerror (gint status)
{
	const gchar *tmp = NULL;

	switch (status) {
		/* Some libsoup transport errors */
		case SOUP_STATUS_NONE:			tmp = _("The update request was cancelled"); break;

		/* http 3xx redirection */
		case SOUP_STATUS_MOVED_PERMANENTLY:	tmp = _("The resource moved permanently to a new location"); break;

		/* http 4xx client error */
		case SOUP_STATUS_UNAUTHORIZED:		tmp = _("You are unauthorized to download this feed. Please update your username and "
								"password in the feed properties dialog box"); break;
		case SOUP_STATUS_PAYMENT_REQUIRED:	tmp = _("Payment required"); break;
		case SOUP_STATUS_FORBIDDEN:		tmp = _("You're not allowed to access this resource"); break;
		case SOUP_STATUS_NOT_FOUND:		tmp = _("Resource Not Found"); break;
		case SOUP_STATUS_METHOD_NOT_ALLOWED:	tmp = _("Method Not Allowed"); break;
		case SOUP_STATUS_NOT_ACCEPTABLE:	tmp = _("Not Acceptable"); break;
		case SOUP_STATUS_PROXY_UNAUTHORIZED:	tmp = _("Proxy authentication required"); break;
		case SOUP_STATUS_REQUEST_TIMEOUT:	tmp = _("Request timed out"); break;
		case SOUP_STATUS_GONE:			tmp = _("The webserver indicates this feed is discontinued. It's no longer available. Liferea won't update it anymore but you can still access the cached headlines."); break;

		/* http 5xx server errors */
		case SOUP_STATUS_INTERNAL_SERVER_ERROR:	tmp = _("Internal Server Error"); break;
		case SOUP_STATUS_NOT_IMPLEMENTED:	tmp = _("Not Implemented"); break;
		case SOUP_STATUS_BAD_GATEWAY:		tmp = _("Bad Gateway"); break;
		case SOUP_STATUS_SERVICE_UNAVAILABLE:	tmp = _("Service Unavailable"); break;
		case SOUP_STATUS_GATEWAY_TIMEOUT:	tmp = _("Gateway Timeout"); break;
		case SOUP_STATUS_HTTP_VERSION_NOT_SUPPORTED: tmp = _("HTTP Version Not Supported"); break;
	}

	if (!tmp)
		tmp = _("An unknown networking error happened!");

	g_assert (tmp);

	return tmp;
}
