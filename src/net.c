/**
 * @file net.c  HTTP network access
 * 
 * Copyright (C) 2007-2008 Lars Lindner <lars.lindner@gmail.com>
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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "ui/liferea_htmlview.h"
#include "ui/liferea_shell.h"
#include "glibcurl/glibcurl.h"

static CURLSH *share_handle = NULL;

static int	NET_TIMEOUT;
static char 	*useragent = NULL;
static char	*proxyname = NULL;
static char	*proxyusername = NULL;
static char	*proxypassword = NULL;
static int	proxyport = 0;

static gboolean	online = FALSE;

static char *
CookieCutter (const char *feedurl, FILE * cookies)
{
	char buf[4096];					/* File read buffer. */
	char tmp[512];
	char *result = NULL;
	char *host;						/* Current feed hostname. */
	char *path;						/* Current feed path. */
	char *url;
	char *freeme, *tmpstr;
	char *tmphost;
	char *cookie;
	char *cookiehost = NULL;
	char *cookiepath = NULL;
	char *cookiename = NULL;
	char *cookievalue = NULL;
	int cookieexpire = 0;
	int cookiesecure = 0;
	int i;
	int len = 0;
	int cookienr = 0;
	time_t tunix;
	
	/* Get current time. */
	tunix = time(0);
	
	url = g_strdup (feedurl);
	freeme = url;
	
	strsep (&url, "/");
	strsep (&url, "/");
	tmphost = url;
	strsep (&url, "/");
	if (url == NULL) {
		g_free (freeme);
		return NULL;
	}
	
	/* If tmphost contains an '@' strip authinfo off url. */
	if (strchr (tmphost, '@') != NULL) {
		strsep (&tmphost, "@");
	}
	
	host = g_strdup (tmphost);
	url--;
	url[0] = '/';
	if (url[strlen(url)-1] == '\n') {
		url[strlen(url)-1] = '\0';
	}
	
	path = g_strdup (url);
	g_free (freeme);
	freeme = NULL;
	
	while (!feof(cookies)) {
		if ((fgets (buf, sizeof(buf), cookies)) == NULL)
			break;
		
		/* Filter \n lines. But ignore them so we can read a NS cookie file. */
		if (buf[0] == '\n')
			continue;
		
		/* Allow adding of comments that start with '#'.
		   Makes it possible to symlink Mozilla's cookies.txt. */
		if (buf[0] == '#')
			continue;
				
		cookie = g_strdup (buf);
		freeme = cookie;
		
		/* Munch trailing newline. */
		if (cookie[strlen(cookie)-1] == '\n')
			cookie[strlen(cookie)-1] = '\0';
		
		/* Decode the cookie string. */
		for (i = 0; i <= 6; i++) {
			tmpstr = strsep (&cookie, "\t");
			
			if (tmpstr == NULL)
				break;
			
			switch (i) {
				case 0:
					/* Cookie hostname. */
					cookiehost = g_strdup (tmpstr);
					break;
				case 1:
					/* Discard host match value. */
					break;
				case 2:
					/* Cookie path. */
					cookiepath = g_strdup (tmpstr);
					break;
				case 3:
					/* Secure cookie? */
					if (strcasecmp (tmpstr, "TRUE") == 0)
						cookiesecure = 1;
					break;
				case 4:
					/* Cookie expiration date. */
					cookieexpire = atoi (tmpstr);
					break;
				case 5:
					/* NAME */
					cookiename = g_strdup (tmpstr);
					break;
				case 6:
					/* VALUE */
					cookievalue = g_strdup (tmpstr);
					break;
				default:
					break;
			}
		}
		
		/* See if current cookie matches cur_ptr.
		   Hostname and path must match.
		   Ignore secure cookies.
		   Discard cookie if it has expired. */
		if ((strstr (host, cookiehost) != NULL) &&
			(strstr (path, cookiepath) != NULL) &&
			(!cookiesecure) &&
			(cookieexpire > (int) tunix)) {					/* Cast time_t tunix to int. */
			cookienr++;
			
			/* Construct and append cookiestring.
			
			   Cookie: NAME=VALUE; NAME=VALUE */
			if (cookienr == 1) {
				len = 8 + strlen(cookiename) + 1 + strlen(cookievalue) + 1;
				result = g_malloc (len);
				strcpy (result, "Cookie: ");
				strcat (result, cookiename);
				strcat (result, "=");
				strcat (result, cookievalue);
			} else {
				len += strlen(cookiename) + 1 + strlen(cookievalue) + 3;
				result = g_realloc (result, len);
				strcat (result, "; ");
				strcat (result, cookiename);
				strcat (result, "=");
				strcat (result, cookievalue);
			}
		} else if ((strstr (host, cookiehost) != NULL) &&
					(strstr (path, cookiepath) != NULL) &&
					(cookieexpire < (int) tunix)) {			/* Cast time_t tunix to int. */
			/* Print cookie expire warning. */
			snprintf (tmp, sizeof(tmp), _("Cookie for %s has expired!"), cookiehost);
		}

		g_free (freeme);
		freeme = NULL;
		g_free (cookiehost);
		g_free (cookiepath);
		g_free (cookiename);
		g_free (cookievalue);
	}
	
	g_free (host);
	g_free (path);
	g_free (freeme);
	
	/* Append \r\n to result */
	if (result != NULL) {
		result = g_realloc (result, len+2);
		strcat (result, "\r\n");
	}
	
	return result;
}


gchar *
cookies_find_matching (const gchar *url)
{
	gchar	*filename;
	gchar	*result;
	FILE	*cookies;

	filename = common_create_cache_filename("", "cookies", "txt");
	cookies = fopen (filename, "r");
	g_free(filename);
	
	if (cookies == NULL) {
		/* No cookies to load. */
		return NULL;
	} else {
		result = CookieCutter (url, cookies);
	}
	fclose (cookies);

	return result;
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
		memcpy (&(result->data[result->size]), ptr, realsize);
		result->size += realsize;
		result->data[result->size] = 0;
	} else {
		realsize = 0;
	}
	
	return realsize;
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
	CURL		*curl_handle;
		
	debug1(DEBUG_UPDATE, "downloading %s", job->request->source);
	g_assert(NULL != job->request);

	curl_handle = curl_easy_init ();
	curl_easy_setopt (curl_handle, CURLOPT_URL, job->request->source);
	curl_easy_setopt (curl_handle, CURLOPT_WRITEFUNCTION, network_write_callback);
	curl_easy_setopt (curl_handle, CURLOPT_WRITEDATA, job->result);
	curl_easy_setopt (curl_handle, CURLOPT_PRIVATE, job);
	curl_easy_setopt (curl_handle, CURLOPT_AUTOREFERER, 1);
	curl_easy_setopt (curl_handle, CURLOPT_USERAGENT, useragent);
	curl_easy_setopt (curl_handle, CURLOPT_FOLLOWLOCATION,  TRUE);
	curl_easy_setopt (curl_handle, CURLOPT_TIMECONDITION, CURL_TIMECOND_IFMODSINCE);
	if (job->request->postdata)
		curl_easy_setopt (curl_handle, CURLOPT_POSTFIELDS, job->request->postdata);	
	if (job->request->updateState) {
		curl_easy_setopt (curl_handle, CURLOPT_COOKIE, update_state_get_cookies (job->request->updateState));
		curl_easy_setopt (curl_handle, CURLOPT_TIMEVALUE, job->request->updateState->lastModified);
	}
	curl_easy_setopt (curl_handle, CURLOPT_FILETIME, 1L);
	if (proxyname) {
		curl_easy_setopt(curl_handle, CURLOPT_PROXY, proxyname);
		if (proxyport > 0)
			curl_easy_setopt(curl_handle, CURLOPT_PROXYPORT, proxyport);
	}
	glibcurl_add (curl_handle);
}

void
network_glibcurl_callback (void *data)
{
	CURLMsg*		msg;
	struct curl_slist	*cookies;
 	gint			inQueue;
	updateJobPtr		job;

	while (1) {
		msg = curl_multi_info_read (glibcurl_handle (), &inQueue);
		if (msg == 0)
			break;
			
		curl_easy_getinfo (msg->easy_handle, CURLINFO_PRIVATE, &job);
		
		if (msg->msg != CURLMSG_DONE) {
			continue;
		} else {
			if (msg->data.result == CURLE_OK) 
				debug0 (DEBUG_UPDATE, "curl download done!");
			else
				debug1 (DEBUG_UPDATE, "curl download failed: status code %d", msg->data.result);

			long		tmp;
			gchar		*contentType, *effectiveSource;

			if (CURLE_OK == curl_easy_getinfo (msg->easy_handle, CURLINFO_CONTENT_TYPE, &contentType))
				job->result->contentType = g_strdup (contentType);
			if (CURLE_OK == curl_easy_getinfo (msg->easy_handle, CURLINFO_EFFECTIVE_URL, &effectiveSource))
				job->result->source = g_strdup (effectiveSource);
			if (CURLE_OK == curl_easy_getinfo (msg->easy_handle, CURLINFO_RESPONSE_CODE, &tmp))
				job->result->httpstatus = (guint)tmp;
			if (CURLE_OK == curl_easy_getinfo (msg->easy_handle, CURLINFO_FILETIME, &tmp)) {
				if (tmp == -1)
					tmp = 0;
				update_state_set_lastmodified (job->result->updateState, tmp);
			}

			/* extract cookies */
			if (CURLE_OK == curl_easy_getinfo (msg->easy_handle, CURLINFO_COOKIELIST, &cookies)) {
				gchar *cookieStr = NULL;
				struct curl_slist *cookie = cookies;
				while (cookie) {
					gchar *tmp = cookieStr;
					cookieStr = g_strdup_printf ("%s%s%s", tmp?tmp:"", tmp?";":"", cookieStr);
					g_free (tmp);
					cookie++;
				}
				if (cookieStr)
					update_state_set_cookies (job->result->updateState, cookieStr);
				curl_slist_free_all (cookies);
			}
			
			debug1 (DEBUG_UPDATE, "source after download: >>>%s<<<\n", job->result->source);
			debug1 (DEBUG_UPDATE, "%d bytes downloaded", job->result->size);
			curl_easy_cleanup (msg->easy_handle);
			
			/* Throw away all received data in case of HTTP errors */
			if (job->result->httpstatus >= 400) {
				g_free (job->result->data);
				job->result->data = NULL;
				job->result->size = 0;
			}
				
			update_process_finished_job (job);
		}
	}
}

void
network_init (void)
{
	if (0 == (NET_TIMEOUT = conf_get_int_value (NETWORK_TIMEOUT)))
		NET_TIMEOUT = 30;	/* default network timeout 30s */

/*	g_assert(curl_global_init(CURL_GLOBAL_ALL) == 0);
	share_handle = curl_share_init();*/
	
	glibcurl_init ();
	glibcurl_set_callback (network_glibcurl_callback, 0);
}

void 
network_deinit (void)
{
	glibcurl_cleanup ();
	
	g_free (useragent);
	g_free (proxyname);
	g_free (proxyusername);
	g_free (proxypassword);
}

void
network_set_user_agent (gchar *newUserAgent)
{
	g_free (useragent);
	useragent = newUserAgent;
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
network_set_proxy (gchar *newProxyName, guint newProxyPort)
{
	g_free (proxyname);
	proxyname = newProxyName;
	proxyport = newProxyPort;
	
	liferea_htmlview_update_proxy ();
}

void
network_set_proxy_auth (gchar *newProxyUsername, gchar *newProxyPassword)
{
	g_free (proxyusername);
	g_free (proxypassword);
	proxyusername = newProxyUsername;
	proxypassword = newProxyPassword;
	
	liferea_htmlview_update_proxy ();
}

const gchar * network_strerror(gint netstatus) {
	gchar	*tmp = NULL;
	
	switch(netstatus) {
/*		case NET_ERR_URL_INVALID:    tmp = _("URL is invalid"); break;
		case NET_ERR_PROTO_INVALID:  tmp = _("Unsupported network protocol"); break;
		case NET_ERR_UNKNOWN:
		case NET_ERR_CONN_FAILED:
		case NET_ERR_SOCK_ERR:       tmp = _("Error connecting to remote host"); break;
		case NET_ERR_HOST_NOT_FOUND: tmp = _("Hostname could not be found"); break;
		case NET_ERR_CONN_REFUSED:   tmp = _("Network connection was refused by the remote host"); break;
		case NET_ERR_TIMEOUT:        tmp = _("Remote host did not finish sending data"); break;

		case NET_ERR_REDIRECT_COUNT_ERR: tmp = _("Too many HTTP redirects were encountered"); break;
		case NET_ERR_REDIRECT_ERR:
		case NET_ERR_HTTP_PROTO_ERR: 
		case NET_ERR_GZIP_ERR:           tmp = _("Remote host sent an invalid response"); break;

		case NET_ERR_AUTH_FAILED:
		case NET_ERR_AUTH_NO_AUTHINFO: tmp = _("Authentication failed"); break;
		case NET_ERR_AUTH_GEN_AUTH_ERR:
		case NET_ERR_AUTH_UNSUPPORTED: tmp = _("Webserver's authentication method incompatible with Liferea"); break;
	*/
		default:
			tmp = _("Network error");
			break;
	}
	
	return tmp;
}

void
network_set_online (gboolean mode)
{
	if (online != mode) {
		online = mode;
		debug1 (DEBUG_UPDATE, "Changing online mode to %s", mode?"online":"offline");
		liferea_shell_online_status_changed (mode);
	}
}

gboolean
network_is_online (void)
{
	return online;
}
