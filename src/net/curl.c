/*
 * @file curl.c libcurl interface for Liferea
 *
 * Copyright (C) 2004 Nathan Conrad <conrad@bungled.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#include <stdlib.h>
#include <string.h>
#include <glib.h>
#include <curl/curl.h>
#include "../update.h"
#include "downloadlib.h"

extern char *proxyname;                 /* Hostname of proxyserver. */
extern unsigned short proxyport;        /* Port on proxyserver to use. */
extern char *useragent;

static CURL *easy_handle;
static CURLSH *share_handle = NULL;

void downloadlib_init() {
	g_assert(curl_global_init(CURL_GLOBAL_ALL) == 0);
	share_handle = curl_share_init();
}

void downloadlib_shutdown() {
	curl_share_cleanup(share_handle);
	curl_easy_cleanup(easy_handle);
	curl_global_cleanup();
}

static size_t
WriteMemoryCallback(void *ptr, size_t size, size_t nmemb, void *data)
{
	int realsize = size * nmemb;
	struct request *request = (struct request*)data;

	if (request->data == NULL)
		request->data = malloc(realsize+2);
	else
		request->data = realloc(request->data, request->size + realsize + 2);

	if (request->data != NULL) {
		memcpy(&(request->data[request->size]), ptr, realsize);
		request->size += realsize;
		request->data[request->size] = 0;
	}
	return realsize;
}


void downloadlib_process_url(struct request *request) {
	CURL *easy_handle = curl_easy_init();
	long l;
	request->data = NULL; /* we expect realloc(NULL, size) to work */
	request->size = 0;    /* no data at this point */

	curl_easy_setopt(easy_handle, CURLOPT_SHARE, share_handle);
	curl_easy_setopt(easy_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(easy_handle, CURLOPT_WRITEDATA, request);
	curl_easy_setopt(easy_handle, CURLOPT_URL, request->source);
	if (proxyname != NULL) {
		curl_easy_setopt(easy_handle, CURLOPT_PROXY, proxyname);
		if (proxyport > 0)
			curl_easy_setopt(easy_handle, CURLOPT_PROXYPORT, proxyport);
	}
	curl_easy_setopt(easy_handle, CURLOPT_FOLLOWLOCATION,  TRUE);
	curl_easy_setopt(easy_handle, CURLOPT_USERAGENT, useragent);
	curl_easy_setopt(easy_handle, CURLOPT_TIMECONDITION,CURL_TIMECOND_IFMODSINCE);
	curl_easy_setopt(easy_handle, CURLOPT_TIMEVALUE, request->lastmodified.tv_sec);
	curl_easy_setopt(easy_handle, CURLOPT_FILETIME, 1L);
	
	curl_easy_perform(easy_handle);

	curl_easy_getinfo(easy_handle, CURLINFO_RESPONSE_CODE, &l);
	request->httpstatus = l;

	curl_easy_getinfo(easy_handle, CURLINFO_FILETIME, &l);
	if (l == -1)
		l = 0;
	request->lastmodified.tv_sec = l;
	
	/* cleanup curl stuff */
	curl_easy_cleanup(easy_handle);
	if (request->data != NULL) {
		request->data[request->size] = '\0';
	}
}
