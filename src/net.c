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
#include "net/netio.h"
#include "ui/ui_htmlview.h"

/* configuration values for the SnowNews HTTP code used from within netio.c */
int	NET_TIMEOUT;
char 	*useragent = NULL;
char	*proxyname = NULL;
char	*proxyusername = NULL;
char	*proxypassword = NULL;
int	proxyport = 0;

void
network_init (void)
{
	if(0 == (NET_TIMEOUT = getNumericConfValue(NETWORK_TIMEOUT)))
		NET_TIMEOUT = 30;	/* default network timeout 30s */
		
	netio_init ();
}

void 
network_deinit (void)
{
	netio_deinit ();
	
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
	struct feed_request	*netioRequest;
	gchar *oldurl = g_strdup(request->source);
	
	debug1(DEBUG_UPDATE, "downloading %s", request->source);
	
	g_assert(request->data == NULL);
	g_assert(request->contentType == NULL);

	netioRequest = g_new0(struct feed_request, 1);
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
	netioRequest->lasthttpstatus = 0; /* This might, or might not mean something to someone */
	
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
	return;
}
