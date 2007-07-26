/**
 * @file net.c HTTP network access
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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
#include "conf.h"
#include "debug.h"
#include "ui/ui_htmlview.h"

#include <glib.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "common.h"

#include <curl/curl.h>
#include <curl/types.h>
#include <curl/easy.h>

// FIXME: make me static
int	NET_TIMEOUT;
char 	*useragent = NULL;
char	*proxyname = NULL;
char	*proxyusername = NULL;
char	*proxypassword = NULL;
int	proxyport = 0;
char * CookieCutter (const char *feedurl, FILE * cookies) {
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


gchar * cookies_find_matching(const gchar *url) {
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

void
network_init (void)
{
	if(0 == (NET_TIMEOUT = getNumericConfValue(NETWORK_TIMEOUT)))
		NET_TIMEOUT = 30;	/* default network timeout 30s */

	curl_global_init (CURL_GLOBAL_ALL);
}

void 
network_deinit (void)
{
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
	
	ui_htmlview_update_proxy ();
}

void
network_set_proxy_auth (gchar *newProxyUsername, gchar *newProxyPassword)
{
	g_free (proxyusername);
	g_free (proxypassword);
	proxyusername = newProxyUsername;
	proxypassword = newProxyPassword;
	
	ui_htmlview_update_proxy ();
}

struct MemoryStruct {
   char *memory;
   size_t size;
};

static size_t
callback (void *ptr, size_t size, size_t nmemb, void *data)
{
   size_t realsize = size * nmemb;
   struct MemoryStruct *mem = (struct MemoryStruct *)data;
 
   mem->memory = (char *)g_realloc(mem->memory, mem->size + realsize + 1);
   if (mem->memory) {
     memcpy(&(mem->memory[mem->size]), ptr, realsize);
     mem->size += realsize;
     mem->memory[mem->size] = 0;
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
network_process_request (struct request *request)
{
	CURL	*curl_handle;
	struct MemoryStruct chunk;
	
	chunk.memory = NULL;
	chunk.size = 0;
	debug1(DEBUG_UPDATE, "downloading %s", request->source);
	
	g_assert(request->data == NULL);
	g_assert(request->contentType == NULL);

/*	netioRequest = g_new0(struct feed_request, 1);
	netioRequest->feedurl = request->source;

	if(request->updateState) {
		netioRequest->lastmodified = request->updateState->lastModified;
		netioRequest->etag = request->updateState->etag;
		netioRequest->cookies = g_strdup(request->updateState->cookies);
	}
		
	netioRequest->problem = 0;
	netioRequest->netio_error = 0;
	netioRequest->no_proxy = request->options->dontUseProxy?1:0;
	netioRequest->content_type = NULL;
	netioRequest->contentlength = 0;
	netioRequest->authinfo = NULL;
	netioRequest->servauth = NULL;
	netioRequest->lasthttpstatus = 0; 
	
	request->data = DownloadFeed(oldurl, netioRequest, 0);

	g_free(oldurl);
	if(request->data == NULL)
		netioRequest->problem = 1;
	request->size = netioRequest->contentlength;
	request->httpstatus = netioRequest->lasthttpstatus;
	request->returncode = netioRequest->netio_error;
	request->source = netioRequest->feedurl;
	if(request->updateState) {
		request->updateState->lastModified = netioRequest->lastmodified;
		netioRequest->lastmodified = NULL;
		request->updateState->etag = netioRequest->etag;
		netioRequest->etag = NULL;
	}
	request->contentType = netioRequest->content_type;
	g_free(netioRequest->servauth);
	g_free(netioRequest->authinfo);
	g_free(netioRequest->cookies);
	g_free(netioRequest->lastmodified);
	g_free(netioRequest->etag);
	debug4(DEBUG_UPDATE, "download result - HTTP status: %d, error: %d, netio error:%d, data: %d",
	                     request->httpstatus, 
			     netioRequest->problem, 
			     netioRequest->netio_error, 
			     request->data);
	g_free(netioRequest);
*/	

	curl_handle = curl_easy_init ();
	curl_easy_setopt (curl_handle, CURLOPT_URL, request->source);
	curl_easy_setopt (curl_handle, CURLOPT_WRITEFUNCTION, callback);
	curl_easy_setopt (curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt (curl_handle, CURLOPT_USERAGENT, useragent);
	curl_easy_perform (curl_handle);
g_print("update request processed:\n");
g_print("    old source: >>>%s<<<\n",request->source);

	g_assert (NULL == request->contentType);	
	request->data = chunk.memory;
	request->size = chunk.size;
	curl_easy_getinfo (curl_handle, CURLINFO_RESPONSE_CODE, &(request->httpstatus));
	curl_easy_getinfo (curl_handle, CURLINFO_CONTENT_TYPE, &(request->contentType));
	curl_easy_getinfo (curl_handle, CURLINFO_EFFECTIVE_URL, &(request->source));
g_print("    new source: >>>%s<<<\n", request->source);
	curl_easy_cleanup (curl_handle);	
	return;
}
