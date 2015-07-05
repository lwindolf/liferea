/**
 * @file net.c  HTTP network access using libsoup
 *
 * Copyright (C) 2007-2015 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2009 Emilio Pozuelo Monfort <pochu27@gmail.com>
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "common.h"
#include "conf.h"
#include "debug.h"

#define HOMEPAGE	"http://lzone.de/liferea/"

static SoupSession *session = NULL;

static gchar	*proxyname = NULL;
static gchar	*proxyusername = NULL;
static gchar	*proxypassword = NULL;
static int	proxyport = 0;

static void
network_process_callback (SoupSession *session, SoupMessage *msg, gpointer user_data)
{
	updateJobPtr	job = (updateJobPtr)user_data;
	SoupDate	*last_modified;
	const gchar	*tmp = NULL;

	job->result->source = soup_uri_to_string (soup_message_get_uri(msg), FALSE);
	if (SOUP_STATUS_IS_TRANSPORT_ERROR (msg->status_code)) {
		job->result->returncode = msg->status_code;
		job->result->httpstatus = 0;
	} else {
		job->result->httpstatus = msg->status_code;
		job->result->returncode = 0;
	}

	debug1 (DEBUG_NET, "download status code: %d", msg->status_code);
	debug1 (DEBUG_NET, "source after download: >>>%s<<<", job->result->source);

	job->result->data = g_memdup (msg->response_body->data, msg->response_body->length+1);
	job->result->size = (size_t)msg->response_body->length;
	debug1 (DEBUG_NET, "%d bytes downloaded", job->result->size);

	job->result->contentType = g_strdup (soup_message_headers_get_content_type (msg->response_headers, NULL));

	/* Update last-modified date */
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

	/* Update ETag value */
	tmp = soup_message_headers_get_one (msg->response_headers, "ETag");
	if (tmp) {
		job->result->updateState->etag = g_strdup(tmp);
	}    

	update_process_finished_job (job);
}

static SoupURI *
network_get_proxy_uri (void)
{
	SoupURI *uri = NULL;
	
	if (!proxyname)
		return uri;
		
	uri = soup_uri_new (NULL);
	soup_uri_set_scheme (uri, SOUP_URI_SCHEME_HTTP);
	soup_uri_set_host (uri, proxyname);
	soup_uri_set_port (uri, proxyport);
	soup_uri_set_user (uri, proxyusername);
	soup_uri_set_password (uri, proxypassword);

	return uri;
}

/* Downloads a feed specified in the request structure, returns 
   the downloaded data or NULL in the request structure.
   If the the webserver reports a permanent redirection, the
   feed url will be modified and the old URL 'll be freed. The
   request structure will also contain the HTTP status and the
   last modified string.
 */
void
network_process_request (const updateJobPtr const job)
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

	/* Set the If-None-Match: header */
	if (job->request->updateState && update_state_get_etag (job->request->updateState)) {
		soup_message_headers_append(msg->request_headers,
					    "If-None-Match",
					    update_state_get_etag (job->request->updateState));
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

	/* If the feed has "dont use a proxy" selected, disable the proxy for the msg */
	if (job->request->options && job->request->options->dontUseProxy)
		soup_message_disable_feature (msg, SOUP_TYPE_PROXY_URI_RESOLVER);

	/* Add Do Not Track header according to settings */
	conf_get_bool_value (DO_NOT_TRACK, &do_not_track);
	if (do_not_track)
		soup_message_headers_append (msg->request_headers, "DNT", "1");

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

void
network_init (void)
{
	gchar		*useragent;
	SoupCookieJar	*cookies;
	gchar		*filename;
	SoupLogger	*logger;
	SoupURI		*proxy;

	/* Set an appropriate user agent */
	if (g_getenv ("LANG")) {
		/* e.g. "Liferea/1.10.0 (Linux; de_DE; http://lzone.de/liferea/) AppleWebKit (KHTML, like Gecko)" */
		useragent = g_strdup_printf ("Liferea/%s (%s; %s; %s) AppleWebKit (KHTML, like Gecko)", VERSION, OSNAME, g_getenv ("LANG"), HOMEPAGE);
	} else {
		/* e.g. "Liferea/1.10.0 (Linux; http://lzone.de/liferea/) AppleWebKit (KHTML, like Gecko)" */
		useragent = g_strdup_printf ("Liferea/%s (%s; %s) AppleWebKit (KHTML, like Gecko)", VERSION, OSNAME, HOMEPAGE);
	}

	/* Cookies */
	filename = common_create_config_filename ("cookies.txt");
	cookies = soup_cookie_jar_text_new (filename, FALSE);
	g_free (filename);

	/* Initialize libsoup */
	proxy = network_get_proxy_uri ();
	session = soup_session_async_new_with_options (SOUP_SESSION_USER_AGENT, useragent,
						       SOUP_SESSION_TIMEOUT, 120,
						       SOUP_SESSION_IDLE_TIMEOUT, 30,
						       SOUP_SESSION_ADD_FEATURE, cookies,
	                                               SOUP_SESSION_ADD_FEATURE_BY_TYPE, SOUP_TYPE_CONTENT_DECODER,
						       NULL);

	if (proxy) {
		debug1 (DEBUG_NET, "Initializing libsoup with proxy '%s'", proxy);
		g_object_set (G_OBJECT (session),
			      SOUP_SESSION_PROXY_URI, proxy,
			      NULL);
		soup_uri_free (proxy);
	} else {
		debug0 (DEBUG_NET, "Initializing libsoup with libproxy defaults");
		soup_session_add_feature_by_type (session, SOUP_TYPE_PROXY_RESOLVER_DEFAULT);
	}
		
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
network_set_proxy (gchar *host, guint port, gchar *user, gchar *password)
{
	/* FIXME: make arguments const and use the SoupURI in network_get_proxy_* ? */
	g_free (proxyname);
	g_free (proxyusername);
	g_free (proxypassword);
	proxyname = host;
	proxyport = port;
	proxyusername = user;
	proxypassword = password;

	/* The sessions will be NULL if we were called from conf_init() as that's called
	 * before net_init() */
	if (session) {
		SoupURI *newproxy = network_get_proxy_uri ();
		
		g_object_set (G_OBJECT (session),
			      SOUP_SESSION_PROXY_URI, newproxy,
			      NULL);

		if (newproxy)
			soup_uri_free (newproxy);
	}

	debug4 (DEBUG_NET, "proxy set to http://%s:%s@%s:%d", user, password, host, port);
	
	network_monitor_proxy_changed ();
}

const char *
network_strerror (gint netstatus, gint httpstatus)
{
	const gchar *tmp = NULL;
	int status = netstatus?netstatus:httpstatus;

	switch (status) {
		/* Some libsoup transport errors */
		case SOUP_STATUS_NONE:			tmp = _("The update request was cancelled"); break;
		case SOUP_STATUS_CANT_RESOLVE:		tmp = _("Unable to resolve destination host name"); break;
		case SOUP_STATUS_CANT_RESOLVE_PROXY:	tmp = _("Unable to resolve proxy host name"); break;
		case SOUP_STATUS_CANT_CONNECT:		tmp = _("Unable to connect to remote host"); break;
		case SOUP_STATUS_CANT_CONNECT_PROXY:	tmp = _("Unable to connect to proxy"); break;
		case SOUP_STATUS_SSL_FAILED:		tmp = _("A network error occurred, or the other end closed the connection unexpectedly"); break;

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
		case SOUP_STATUS_GONE:			tmp = _("Gone. Resource doesn't exist. Please unsubscribe!"); break;
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

