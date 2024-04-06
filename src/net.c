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

static GCancellable *cancellable = NULL;	/* GCancellable for all request handling */
static SoupSession *session = NULL;	/* Session configured for preferences */
static SoupSession *session2 = NULL;	/* Session for "Don't use proxy feature" */

static ProxyDetectMode proxymode = PROXY_DETECT_MODE_AUTO;

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

static void
network_process_callback (GObject *obj, GAsyncResult *res, gpointer user_data)
{
	SoupSession		*session = SOUP_SESSION (obj);
	SoupMessage		*msg;
	updateJobPtr		job = (updateJobPtr)user_data;
	GDateTime		*last_modified;
	const gchar		*tmp = NULL;
	GHashTable		*params;
	gboolean		revalidated = FALSE;
	gint			maxage;
	gint			age;
	gint			body_size;
	g_autoptr(GBytes)	body;

	msg = soup_session_get_async_result_message (session, res);
	body = soup_session_send_and_read_finish (session, res, NULL);	// FIXME: handle errors!

	job->result->source = g_uri_to_string_partial (soup_message_get_uri (msg), 0);
	job->result->httpstatus = soup_message_get_status (msg);
	body_size = g_bytes_get_size (body);
	job->result->data = g_malloc(1 + body_size);
	memmove(job->result->data, g_bytes_get_data (body, &job->result->size), body_size);
	*(job->result->data + job->result->size) = 0;

	/* keep some request headers for revalidated responses */
	revalidated = (304 == job->result->httpstatus);

	debug (DEBUG_NET, "download status code: %d", job->result->httpstatus);
	debug (DEBUG_NET, "source after download: >>>%s<<<", job->result->source);
	debug (DEBUG_NET, "%d bytes downloaded", job->result->size);

	job->result->contentType = g_strdup (soup_message_headers_get_content_type (soup_message_get_response_headers (msg), NULL));

	/* Update last-modified date */
	if (revalidated) {
		 job->result->updateState->lastModified = update_state_get_lastmodified (job->request->updateState);
	} else {
		tmp = soup_message_headers_get_one (soup_message_get_response_headers (msg), "Last-Modified");
		if (tmp) {
			/* The string may be badly formatted, which will make
			* soup_date_new_from_string() return NULL */
			last_modified = soup_date_time_new_from_http_string (tmp);
			if (last_modified) {
				job->result->updateState->lastModified = g_date_time_to_unix (last_modified);
				g_date_time_unref (last_modified);
			}
		}
	}

	/* Update ETag value */
	if (revalidated) {
		job->result->updateState->etag = g_strdup (update_state_get_etag (job->request->updateState));
	} else {
		tmp = soup_message_headers_get_one (soup_message_get_response_headers (msg), "ETag");
		if (tmp) {
			job->result->updateState->etag = g_strdup (tmp);
		}
	}

	/* Update cache max-age  */
	tmp = soup_message_headers_get_list (soup_message_get_response_headers (msg), "Cache-Control");
	if (tmp) {
		params = soup_header_parse_param_list (tmp);
		if (params) {
			tmp = g_hash_table_lookup (params, "max-age");
			if (tmp) {
				maxage = atoi (tmp);
				if (0 < maxage) {
					/* subtract Age from max-age */
					tmp = soup_message_headers_get_one (soup_message_get_response_headers (msg), "Age");
					if (tmp) {
						age = atoi (tmp);
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

	update_process_finished_job (job);
	g_bytes_unref (body);
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
	g_autoptr(SoupMessage)	msg = NULL;
	SoupMessageHeaders	*request_headers;
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

	/* Prepare the SoupMessage */
	sourceUri = g_uri_build_with_user (
		SOUP_HTTP_URI_FLAGS | G_URI_FLAGS_PARSE_RELAXED,
		scheme,
		// FIXME: allow passing user/password from above?
		(!job->request->authValue && job->request->options && job->request->options->username)?job->request->options->username:NULL,
		(!job->request->authValue && job->request->options && job->request->options->password)?job->request->options->password:NULL,
		auth_params,
		host,
		port,
		path,
		query,
		fragment
	);

	if (sourceUri)
		msg = soup_message_new_from_uri (job->request->postdata?"POST":"GET", sourceUri);
	if (!msg) {
		g_warning ("The request for %s could not be parsed!", job->request->source);
		return;
	}

	request_headers = soup_message_get_request_headers (msg);

	/* Set the postdata for the request */
	if (job->request->postdata) {
		g_autoptr(GBytes) postdata = g_bytes_new (job->request->postdata, strlen (job->request->postdata));
		soup_message_set_request_body_from_bytes (msg,
		                                          "application/x-www-form-urlencoded",
		                                          postdata);
	}

	/* Set the If-Modified-Since: header */
	if (job->request->updateState && update_state_get_lastmodified (job->request->updateState)) {
		g_autofree gchar *datestr;
		g_autoptr(GDateTime) date;

		date = g_date_time_new_from_unix_utc (update_state_get_lastmodified (job->request->updateState));
		datestr = soup_date_time_to_string (date, SOUP_DATE_HTTP);
		soup_message_headers_append (request_headers,
					     "If-Modified-Since",
					     datestr);
	}

	/* Set the If-None-Match header */
	if (job->request->updateState && update_state_get_etag (job->request->updateState)) {
		soup_message_headers_append(request_headers,
					    "If-None-Match",
					    update_state_get_etag (job->request->updateState));
	}

	/* Set the A-IM header */
	if (job->request->updateState &&
	    (update_state_get_lastmodified (job->request->updateState) ||
	     update_state_get_etag (job->request->updateState))) {
		soup_message_headers_append(request_headers,
					    "A-IM",
					    "feed");
	}

	/* Support HTTP content negotiation */
	soup_message_headers_append (request_headers, "Accept", "application/atom+xml,application/xml;q=0.9,text/xml;q=0.8,*/*;q=0.7");

	/* Add Authorization header */
	if (job->request->authValue) {
		soup_message_headers_append (request_headers, "Authorization",
					     job->request->authValue);
	}

	/* Add requested cookies */
	if (job->request->updateState && job->request->updateState->cookies) {
		soup_message_headers_append (request_headers, "Cookie",
		                             job->request->updateState->cookies);
		soup_message_disable_feature (msg, SOUP_TYPE_COOKIE_JAR);
	}

	/* TODO: Right now we send the msg, and if it requires authentication and
	 * we didn't provide one, the petition fails and when the job is processed
	 * it sees it needs authentication and displays a dialog, and if credentials
	 * are entered, it queues a new job with auth credentials. Instead of that,
	 * we should probably handle authentication directly here, connecting the
	 * msg to a callback in case of 401 (see soup_message_add_status_code_handler())
	 * displaying the dialog ourselves, and requeing the msg if we get credentials */

	/* Add DNT / GPC headers according to settings */
	conf_get_bool_value (DO_NOT_TRACK, &do_not_track);
	conf_get_bool_value (DO_NOT_SELL, &do_not_sell);
	if (do_not_track)
		soup_message_headers_append (request_headers, "DNT", "1");
	if (do_not_track)
		soup_message_headers_append (request_headers, "Sec-GPC", "1");

	/* Process permanent redirects (update feed location) */
	soup_message_add_status_code_handler (msg, "got_body", 301, (GCallback) network_process_redirect_callback, job);
	soup_message_add_status_code_handler (msg, "got_body", 308, (GCallback) network_process_redirect_callback, job);

	/* If the feed has "dont use a proxy" selected, use 'session2' which is non-proxy */
	if (job->request->options && job->request->options->dontUseProxy)
		soup_session_send_and_read_async (session2, msg, 0 /* IO priority */, cancellable, network_process_callback, job);
	else
		soup_session_send_and_read_async (session, msg, 0 /* IO priority */, cancellable, network_process_callback, job);
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
	g_cancellable_cancel (cancellable);
	g_free (cancellable);

	soup_session_abort (session);
	soup_session_abort (session2);

	g_free (session);
	g_free (session2);
}

void
network_init (void)
{
	gchar		*useragent;
	SoupCookieJar	*cookies;
	gchar		*filename;
	SoupLogger	*logger;

	cancellable = g_cancellable_new ();

	useragent = network_get_user_agent ();
	debug (DEBUG_NET, "user-agent set to \"%s\"", useragent);

	/* Session cookies */
	filename = common_create_config_filename ("session_cookies.txt");
	cookies = soup_cookie_jar_text_new (filename, TRUE);
	g_free (filename);

	/* Initialize libsoup */
	session = soup_session_new_with_options ("user-agent", useragent,
						 "timeout", 120,
						 "idle-timeout", 30,
						 NULL);
	session2 = soup_session_new_with_options ("user-agent", useragent,
						  "timeout", 120,
						  "idle-timeout", 30,
						  NULL);

	soup_session_add_feature (session, SOUP_SESSION_FEATURE (cookies));
	soup_session_add_feature (session2, SOUP_SESSION_FEATURE (cookies));

	/* Only 'session' gets proxy, 'session2' is for non-proxy requests */
	soup_session_set_proxy_resolver (session2, NULL);
	network_set_soup_session_proxy (session, network_get_proxy_detect_mode());

	/* Soup debugging */
	if (debug_get_flags() & DEBUG_NET) {
		logger = soup_logger_new (SOUP_LOGGER_LOG_HEADERS);
		soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
	}

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

	/* session will be NULL if we were called from conf_init() as that's called
	 * before net_init() */
	if (session)
		network_set_soup_session_proxy (session, mode);

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
