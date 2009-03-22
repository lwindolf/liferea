/**
 * @file net.c  HTTP network access using libsoup
 *
 * Copyright (C) 2007-2009 Lars Lindner <lars.lindner@gmail.com>
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
#include "ui/liferea_htmlview.h"
#include "ui/liferea_shell.h"

#define HOMEPAGE	"http://liferea.sf.net/"

static SoupSession *session;
static SoupSession *session_no_proxy;
static SoupSession *session_no_cookies;
static SoupSession *session_no_cookies_no_proxy;
static SoupURI *proxy;

static gchar	*proxyname = NULL;
static gchar	*proxyusername = NULL;
static gchar	*proxypassword = NULL;
static int	proxyport = 0;

static gboolean	online = FALSE;

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
	debug1 (DEBUG_NET, "source after download: >>>%s<<<\n", job->result->source);

	job->result->data = g_memdup (msg->response_body->data, msg->response_body->length+1);
	job->result->size = (size_t)msg->response_body->length;
	debug1 (DEBUG_NET, "%d bytes downloaded", job->result->size);

	job->result->contentType = g_strdup (soup_message_headers_get_content_type (msg->response_headers, NULL));

	/* Update last-modified date */
	tmp = soup_message_headers_get (msg->response_headers, "Last-Modified");
	if (tmp) {
		/* The string may be badly formatted, which will make
		 * soup_date_new_from_string() return NULL */
		last_modified = soup_date_new_from_string (tmp);
		if (last_modified) {
			job->result->updateState->lastModified = soup_date_to_time_t (last_modified);
			soup_date_free (last_modified);
		}
	}

	update_process_finished_job (job);
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
	g_assert(NULL != job->request);
	debug1(DEBUG_NET, "downloading %s", job->request->source);

	SoupMessage	*msg;
	SoupDate	*date;
	gboolean	no_proxy = FALSE;
	gboolean	no_cookies = FALSE;

	/* Prepare the SoupMessage */
	if (job->request->postdata) {
		msg = soup_message_new (SOUP_METHOD_POST, job->request->source);

		/* set the postdata for the request */
		soup_message_set_request (msg,
					  "application/x-www-form-urlencoded",
					  SOUP_MEMORY_STATIC, /* libsoup won't free the postdata */
					  job->request->postdata,
					  strlen (job->request->postdata));
	} else {
		msg = soup_message_new (SOUP_METHOD_GET, job->request->source);
	}

	/* Set the If-Modified-Since: header */
	if (job->request->updateState && job->request->updateState->lastModified) {
		gchar *datestr;

		date = soup_date_new_from_time_t (job->request->updateState->lastModified);
		datestr = soup_date_to_string (date, SOUP_DATE_HTTP);
		soup_message_headers_append (msg->request_headers,
					     "If-Modified-Since",
					     datestr);
		g_free (datestr);
		soup_date_free (date);
	}

	/* Set the authentication */
	if (job->request->options &&
	    job->request->options->username &&
	    job->request->options->password) {
		SoupURI *uri = soup_message_get_uri (msg);

		soup_uri_set_user (uri, job->request->options->username);
		soup_uri_set_password (uri, job->request->options->password);
	}

	/* Add requested cookies */
	if (job->request->updateState && job->request->updateState->cookies) {
		soup_message_headers_append (msg->request_headers, "Cookie",
						job->request->updateState->cookies);
		no_cookies = TRUE;
	}

	/* TODO: Right now we send the msg, and if it requires authentication and
	 * we didn't provide one, the petition fails and when the job is processed
	 * it sees it needs authentication and displays a dialog, and if credentials
	 * are entered, it queues a new job with auth credentials. Instead of that,
	 * we should probably handle authentication directly here, connecting the
	 * msg to a callback in case of 401 (see soup_message_add_status_code_handler())
	 * displaying the dialog ourselves, and requeing the msg if we get credentials */

	/* We queue the message in one session or the other depending on whether the
	 * feed properties has the "dont use a proxy" checkbox enabled */
	if (job->request->options && job->request->options->dontUseProxy) {
		no_proxy = TRUE;
	}

	if (no_proxy && no_cookies) {
		soup_session_queue_message (session_no_cookies_no_proxy, msg, network_process_callback, job);
	} else if (no_proxy) {
		soup_session_queue_message (session_no_proxy, msg, network_process_callback, job);
	} else if (no_cookies) {
		soup_session_queue_message (session_no_cookies, msg, network_process_callback, job);
	} else {
		soup_session_queue_message (session, msg, network_process_callback, job);
	}
}

void
network_init (void)
{
	gchar		*useragent;
	SoupCookieJar	*cookies;
	gchar		*filename;
	SoupLogger	*logger;

	/* Set an appropriate user agent */
	if (g_getenv ("LANG")) {
		/* e.g. "Liferea/1.6.0 (Linux; de_DE; http://liferea.sf.net/)" */
		useragent = g_strdup_printf ("Liferea/%s (%s; %s; %s)", VERSION, OSNAME, g_getenv ("LANG"), HOMEPAGE);
	} else {
		/* e.g. "Liferea/1.6.0 (Linux; http://liferea.sf.net/)" */
		useragent = g_strdup_printf ("Liferea/%s (%s; %s)", VERSION, OSNAME, HOMEPAGE);
	}

	/* Cookies */
	filename = common_create_cache_filename ("", "cookies", "txt");
	cookies = soup_cookie_jar_text_new (filename, FALSE);
	g_free (filename);

	/* Initialize libsoup */
	session = soup_session_async_new_with_options (SOUP_SESSION_USER_AGENT, useragent,
						       SOUP_SESSION_IDLE_TIMEOUT, 30,
						       SOUP_SESSION_PROXY_URI, proxy,
						       SOUP_SESSION_ADD_FEATURE, cookies,
						       NULL);

	/* This session is for those cases where we are told not to use the proxy */
	session_no_proxy = soup_session_async_new_with_options (SOUP_SESSION_USER_AGENT, useragent,
								SOUP_SESSION_IDLE_TIMEOUT, 30,
								SOUP_SESSION_ADD_FEATURE, cookies,
								NULL);

	/* This session is for those cases where we need to add specific cookies, e.g. Google Reader.
	 * Once GNOME #574571 is fixed, we will be able to use the normal session */
	session_no_cookies = soup_session_async_new_with_options (SOUP_SESSION_USER_AGENT, useragent,
								  SOUP_SESSION_IDLE_TIMEOUT, 30,
								  SOUP_SESSION_PROXY_URI, proxy,
								  NULL);

	/* And this one is for cases where we need to use our own cookies, and bypass the proxy, e.g.
	 * Google Reader subscription where the "ignore proxy" preference is set */
	session_no_cookies_no_proxy = soup_session_async_new_with_options (SOUP_SESSION_USER_AGENT, useragent,
									   SOUP_SESSION_IDLE_TIMEOUT, 30,
									   NULL);

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

void
network_set_proxy (gchar *host, guint port, gchar *user, gchar *password)
{
	SoupURI *newproxy = NULL;

	/* FIXME: make arguments const and use the SoupURI in network_get_proxy_* ? */
	g_free (proxyname);
	g_free (proxyusername);
	g_free (proxypassword);
	proxyname = host;
	proxyport = port;
	proxyusername = user;
	proxypassword = password;

	if (host) {
		newproxy = soup_uri_new (host);

		if (newproxy) {
			soup_uri_set_port (newproxy, port);
			soup_uri_set_user (newproxy, user);
			soup_uri_set_password (newproxy, password);
		}
	}

	/* The sessions will be NULL if we were called from conf_init() as that's called
	 * before net_init() */
	if (session) {
		g_object_set (session,
			      SOUP_SESSION_PROXY_URI, newproxy,
			      NULL);
		g_object_set (session_no_cookies,
			      SOUP_SESSION_PROXY_URI, newproxy,
			      NULL);
	}

	if (proxy) {
		soup_uri_free (proxy);
	}
	proxy = newproxy;

	if (proxy) {
		debug1 (DEBUG_NET, "proxy set to %s", soup_uri_to_string (proxy, FALSE));
	} else {
		debug0 (DEBUG_NET, "proxy unset!");
	}

	liferea_htmlview_update_proxy ();
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

void
network_set_online (gboolean mode)
{
	if (online != mode) {
		online = mode;
		debug1 (DEBUG_NET, "Changing online mode to %s", mode?"online":"offline");
		liferea_shell_online_status_changed (mode);
	}
}

gboolean
network_is_online (void)
{
	return online;
}
