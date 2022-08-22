/**
 * @file net.c  HTTP network access using libsoup
 *
 * Copyright (C) 2007-2021 Lars Windolf <lars.windolf@gmx.de>
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

static SoupSession *session = NULL;	/* Session configured for preferences */
static SoupSession *session2 = NULL;	/* Session for "Don't use proxy feature" */

static ProxyDetectMode proxymode = PROXY_DETECT_MODE_AUTO;
static gchar	*proxyname = NULL;
static gchar	*proxyusername = NULL;
static gchar	*proxypassword = NULL;
static int	proxyport = 0;


static void
network_process_redirect_callback (SoupMessage *msg, gpointer user_data)
{
	updateJobPtr	job = (updateJobPtr)user_data;
	const gchar	*location = NULL;
	SoupURI		*newuri;

	if (301 == msg->status_code || 308 == msg->status_code)
	{
		location = soup_message_headers_get_one (msg->response_headers, "Location");
		newuri = soup_uri_new (location);

		if (SOUP_URI_IS_VALID (newuri) && ! soup_uri_equal (newuri, soup_message_get_uri (msg))) {
			debug2 (DEBUG_NET, "\"%s\" permanently redirects to new location \"%s\"", soup_uri_to_string (soup_message_get_uri (msg), FALSE),
							            soup_uri_to_string (newuri, FALSE));
			job->result->httpstatus = msg->status_code;
			job->result->source = soup_uri_to_string (newuri, FALSE);
		}
	}
}

static void
network_process_callback (SoupSession *session, SoupMessage *msg, gpointer user_data)
{
	updateJobPtr	job = (updateJobPtr)user_data;
	SoupDate	*last_modified;
	const gchar	*tmp = NULL;
	GHashTable	*params;
	gboolean	revalidated = FALSE;
	gint		maxage;
	gint		age;

	job->result->source = soup_uri_to_string (soup_message_get_uri(msg), FALSE);
	job->result->httpstatus = msg->status_code;

	/* keep some request headers for revalidated responses */
	revalidated = (304 == job->result->httpstatus);

	debug1 (DEBUG_NET, "download status code: %d", msg->status_code);
	debug1 (DEBUG_NET, "source after download: >>>%s<<<", job->result->source);

#ifdef HAVE_G_MEMDUP2
	job->result->data = g_memdup2 (msg->response_body->data, msg->response_body->length+1);
#else
	job->result->data = g_memdup (msg->response_body->data, msg->response_body->length+1);
#endif

	job->result->size = (size_t)msg->response_body->length;
	debug1 (DEBUG_NET, "%d bytes downloaded", job->result->size);

	job->result->contentType = g_strdup (soup_message_headers_get_content_type (msg->response_headers, NULL));

	/* Update last-modified date */
	if (revalidated) {
		 job->result->updateState->lastModified = update_state_get_lastmodified (job->request->updateState);
	} else {
		tmp = soup_message_headers_get_one (msg->response_headers, "Last-Modified");
		if (tmp) {
			/* The string may be badly formatted, which will make
			* soup_date_new_from_string() return NULL */
			last_modified = soup_date_new_from_string (tmp);
			if (last_modified) {
				job->result->updateState->lastModified = soup_date_to_time_t (last_modified);
				soup_date_free (last_modified);
			}
		}
	}

	/* Update ETag value */
	if (revalidated) {
		job->result->updateState->etag = g_strdup (update_state_get_etag (job->request->updateState));
	} else {
		tmp = soup_message_headers_get_one (msg->response_headers, "ETag");
		if (tmp) {
			job->result->updateState->etag = g_strdup (tmp);
		}
	}

	/* Update cache max-age  */
	tmp = soup_message_headers_get_list (msg->response_headers, "Cache-Control");
	if (tmp) {
		params = soup_header_parse_param_list (tmp);
		if (params) {
			tmp = g_hash_table_lookup (params, "max-age");
			if (tmp) {
				maxage = atoi (tmp);
				if (0 < maxage) {
					/* subtract Age from max-age */
					tmp = soup_message_headers_get_one (msg->response_headers, "Age");
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
}

/* Downloads a feed specified in the request structure, returns
   the downloaded data or NULL in the request structure.
   If the webserver reports a permanent redirection, the
   feed url will be modified and the old URL 'll be freed. The
   request structure will also contain the HTTP status and the
   last modified string.
 */
void
network_process_request (const updateJobPtr job)
{
	SoupMessage	*msg;
	SoupDate	*date;
	gboolean	do_not_track = FALSE;

	g_assert (NULL != job->request);
	debug1 (DEBUG_NET, "downloading %s", job->request->source);
	if (job->request->postdata && (debug_level & DEBUG_VERBOSE) && (debug_level & DEBUG_NET))
		debug1 (DEBUG_NET, "   postdata=>>>%s<<<", job->request->postdata);

	/* Prepare the SoupMessage */
	msg = soup_message_new (job->request->postdata ? SOUP_METHOD_POST : SOUP_METHOD_GET,
				job->request->source);

	if (!msg) {
		g_warning ("The request for %s could not be parsed!", job->request->source);
		return;
	}

	/* Set the postdata for the request */
	if (job->request->postdata) {
		soup_message_set_request (msg,
					  "application/x-www-form-urlencoded",
					  SOUP_MEMORY_STATIC, /* libsoup won't free the postdata */
					  job->request->postdata,
					  strlen (job->request->postdata));
	}

	/* Set the If-Modified-Since: header */
	if (job->request->updateState && update_state_get_lastmodified (job->request->updateState)) {
		gchar *datestr;

		date = soup_date_new_from_time_t (update_state_get_lastmodified (job->request->updateState));
		datestr = soup_date_to_string (date, SOUP_DATE_HTTP);
		soup_message_headers_append (msg->request_headers,
					     "If-Modified-Since",
					     datestr);
		g_free (datestr);
		soup_date_free (date);
	}

	/* Set the If-None-Match header */
	if (job->request->updateState && update_state_get_etag (job->request->updateState)) {
		soup_message_headers_append(msg->request_headers,
					    "If-None-Match",
					    update_state_get_etag (job->request->updateState));
	}

	/* Set the I-AM header */
	if (job->request->updateState &&
	    (update_state_get_lastmodified (job->request->updateState) ||
	     update_state_get_etag (job->request->updateState))) {
		soup_message_headers_append(msg->request_headers,
					    "A-IM",
					    "feed");
	}

	/* Support HTTP content negotiation */
	soup_message_headers_append(msg->request_headers, "Accept", "application/atom+xml,application/xml;q=0.9,text/xml;q=0.8,*/*;q=0.7");

	/* Set the authentication */
	if (!job->request->authValue &&
	    job->request->options &&
	    job->request->options->username &&
	    job->request->options->password) {
		SoupURI *uri = soup_message_get_uri (msg);

		soup_uri_set_user (uri, job->request->options->username);
		soup_uri_set_password (uri, job->request->options->password);
	}

	if (job->request->authValue) {
		soup_message_headers_append (msg->request_headers, "Authorization",
					     job->request->authValue);
	}

	/* Add requested cookies */
	if (job->request->updateState && job->request->updateState->cookies) {
		soup_message_headers_append (msg->request_headers, "Cookie",
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

	/* Add Do Not Track header according to settings */
	conf_get_bool_value (DO_NOT_TRACK, &do_not_track);
	if (do_not_track)
		soup_message_headers_append (msg->request_headers, "DNT", "1");

	/* Process permanent redirects (update feed location) */
	soup_message_add_status_code_handler (msg, "got_body", 301, (GCallback) network_process_redirect_callback, job);
	soup_message_add_status_code_handler (msg, "got_body", 308, (GCallback) network_process_redirect_callback, job);

	/* If the feed has "dont use a proxy" selected, use 'session2' which is non-proxy */
	if (job->request->options && job->request->options->dontUseProxy)
		soup_session_queue_message (session2, msg, network_process_callback, job);
	else
		soup_session_queue_message (session, msg, network_process_callback, job);
}

static void
network_authenticate (
	SoupSession *session,
	SoupMessage *msg,
        SoupAuth *auth,
	gboolean retrying,
	gpointer data)
{
	if (!retrying && msg->status_code == SOUP_STATUS_PROXY_UNAUTHORIZED) {
		soup_auth_authenticate (auth, g_strdup (proxyusername), g_strdup (proxypassword));
	}

	// FIXME: Handle HTTP 401 too
}

static void
network_set_soup_session_proxy (SoupSession *session, ProxyDetectMode mode, const gchar *host, guint port, const gchar *user, const gchar *password)
{
	SoupURI *uri = NULL;

	switch (mode) {
		case PROXY_DETECT_MODE_AUTO:
			/* Sets proxy-resolver to the default resolver, this unsets proxy-uri. */
			g_object_set (G_OBJECT (session),
				SOUP_SESSION_PROXY_RESOLVER, g_proxy_resolver_get_default (),
				NULL);
			break;
		case PROXY_DETECT_MODE_NONE:
			/* Sets proxy-resolver to NULL, this unsets proxy-uri. */
			g_object_set (G_OBJECT (session),
				SOUP_SESSION_PROXY_RESOLVER, NULL,
				NULL);
			break;
		case PROXY_DETECT_MODE_MANUAL:
			uri = soup_uri_new (NULL);
			soup_uri_set_scheme (uri, SOUP_URI_SCHEME_HTTP);
			soup_uri_set_host (uri, host);
			soup_uri_set_port (uri, port);
			soup_uri_set_user (uri, user);
			soup_uri_set_password (uri, password);
			soup_uri_set_path (uri, "/");

			if (SOUP_URI_IS_VALID (uri)) {
				/* Sets proxy-uri, this unsets proxy-resolver. */
				g_object_set (G_OBJECT (session),
					SOUP_SESSION_PROXY_URI, uri,
					NULL);
			}
			soup_uri_free (uri);
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
			 * e.g. "Liferea/1.10.0 (Android 12; Mobile; https://lzone.de/liferea/) AppleWebKit (KHTML, like Gecko)" */
			useragent = g_strdup_printf ("Liferea/%s (Android 12; Mobile; %s) AppleWebKit (KHTML, like Gecko)", VERSION, HOMEPAGE);
		}
	} else {
		/* Set an arbitrary user agent from the environment variable LIFEREA_UA */
		useragent = g_strdup (sysua);
	}

	g_assert_nonnull (useragent);
	
	return useragent;
}

void
network_init (void)
{
	gchar		*useragent;
	SoupCookieJar	*cookies;
	gchar		*filename;
	SoupLogger	*logger;

	useragent = network_get_user_agent ();
	debug1 (DEBUG_NET, "user-agent set to \"%s\"", useragent);

	/* Session cookies */
	filename = common_create_config_filename ("session_cookies.txt");
	cookies = soup_cookie_jar_text_new (filename, TRUE);
	g_free (filename);

	/* Initialize libsoup */
	session = soup_session_new_with_options (SOUP_SESSION_USER_AGENT, useragent,
						 SOUP_SESSION_TIMEOUT, 120,
						 SOUP_SESSION_IDLE_TIMEOUT, 30,
						 SOUP_SESSION_ADD_FEATURE, cookies,
						 NULL);
	session2 = soup_session_new_with_options (SOUP_SESSION_USER_AGENT, useragent,
						  SOUP_SESSION_TIMEOUT, 120,
						  SOUP_SESSION_IDLE_TIMEOUT, 30,
						  SOUP_SESSION_ADD_FEATURE, cookies,
						  SOUP_SESSION_PROXY_URI, NULL,
						  SOUP_SESSION_PROXY_RESOLVER, NULL,
						  NULL);

	/* Only 'session' gets proxy, 'session2' is for non-proxy requests */
	network_set_soup_session_proxy (session, network_get_proxy_detect_mode(),
		network_get_proxy_host (),
		network_get_proxy_port (),
		network_get_proxy_username (),
		network_get_proxy_password ());

	g_signal_connect (session, "authenticate", G_CALLBACK (network_authenticate), NULL);

	/* Soup debugging */
	if (debug_level & DEBUG_NET) {
		logger = soup_logger_new (SOUP_LOGGER_LOG_HEADERS, -1);
		soup_session_add_feature (session, SOUP_SESSION_FEATURE (logger));
	}

	g_free (useragent);
}

void
network_deinit (void)
{
	g_free (proxyname);
	g_free (proxyusername);
	g_free (proxypassword);
}

ProxyDetectMode
network_get_proxy_detect_mode (void)
{
	return proxymode;
}

const gchar *
network_get_proxy_host (void)
{
	return proxyname;
}

guint
network_get_proxy_port (void)
{
	return proxyport;
}

const gchar *
network_get_proxy_username (void)
{
	return proxyusername;
}

const gchar *
network_get_proxy_password (void)
{
	return proxypassword;
}

extern void network_monitor_proxy_changed (void);

void
network_set_proxy (ProxyDetectMode mode, gchar *host, guint port, gchar *user, gchar *password)
{
	g_free (proxyname);
	g_free (proxyusername);
	g_free (proxypassword);
	proxymode = mode;
	proxyname = host;
	proxyport = port;
	proxyusername = user;
	proxypassword = password;

	/* session will be NULL if we were called from conf_init() as that's called
	 * before net_init() */
	if (session)
		network_set_soup_session_proxy (session, mode, host, port, user, password);

	debug4 (DEBUG_NET, "proxy set to http://%s:%s@%s:%d", user, password, host, port);

	network_monitor_proxy_changed ();
}

const char *
network_strerror (gint status)
{
	const gchar *tmp = NULL;

	switch (status) {
		/* Some libsoup transport errors */
		case SOUP_STATUS_NONE:			tmp = _("The update request was cancelled"); break;
		case SOUP_STATUS_CANT_RESOLVE:		tmp = _("Unable to resolve destination host name"); break;
		case SOUP_STATUS_CANT_RESOLVE_PROXY:	tmp = _("Unable to resolve proxy host name"); break;
		case SOUP_STATUS_CANT_CONNECT:		tmp = _("Unable to connect to remote host"); break;
		case SOUP_STATUS_CANT_CONNECT_PROXY:	tmp = _("Unable to connect to proxy"); break;
		case SOUP_STATUS_SSL_FAILED:		tmp = _("SSL/TLS negotiation failed. Possible outdated or unsupported encryption algorithm. Check your operating system settings."); break;

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

	if (!tmp) {
		if (SOUP_STATUS_IS_TRANSPORT_ERROR (status)) {
			tmp = _("There was an internal error in the update process");
		} else if (SOUP_STATUS_IS_REDIRECTION (status)) {
			tmp = _("Feed not available: Server requested unsupported redirection!");
		} else if (SOUP_STATUS_IS_CLIENT_ERROR (status)) {
			tmp = _("Client Error");
		} else if (SOUP_STATUS_IS_SERVER_ERROR (status)) {
			tmp = _("Server Error");
		} else {
			tmp = _("An unknown networking error happened!");
		}
	}

	g_assert (tmp);

	return tmp;
}
