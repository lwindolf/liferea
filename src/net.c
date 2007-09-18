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

updateResultPtr
network_process_request (const struct updateRequest * const request)
{
	updateResultPtr 	result;
	struct feed_request	*netioRequest;
	
	debug1 (DEBUG_UPDATE, "downloading %s", request->source);
	
	/* 1. Prepare request structure for SnowNews code */
	netioRequest = g_new0 (struct feed_request, 1);
	netioRequest->feedurl = g_strdup (request->source);

	if (request->updateState) {
		netioRequest->lastmodified = g_strdup (update_state_get_lastmodified (request->updateState));
		netioRequest->etag = g_strdup (update_state_get_etag (request->updateState));
		netioRequest->cookies = g_strdup (update_state_get_cookies (request->updateState));
	}

	netioRequest->problem = 0;
	netioRequest->netio_error = 0;
	/* FIXME: HTTP auth username and password are encoded in URI, extraction done in netio.c... */
	if (request->options) {
		netioRequest->no_proxy = request->options->dontUseProxy?1:0;
	}
	netioRequest->content_type = NULL;
	netioRequest->contentlength = 0;
	netioRequest->authinfo = NULL;
	netioRequest->servauth = NULL;
	netioRequest->lasthttpstatus = 0; /* This might, or might not mean something to someone */

	result = update_result_new ();	
	result->data = DownloadFeed (request->source, netioRequest, 0);

	/* 2. Fill in SnowNews download results in result structure */
	
	if (result->data == NULL)
		netioRequest->problem = 1;
		
	result->size = netioRequest->contentlength;
	result->httpstatus = netioRequest->lasthttpstatus;
	result->returncode = netioRequest->netio_error;
	result->source = netioRequest->feedurl;
	result->contentType = netioRequest->content_type;
	
	result->updateState = update_state_new ();
	update_state_set_lastmodified (result->updateState, netioRequest->lastmodified);
	update_state_set_etag (result->updateState, netioRequest->etag);
	update_state_set_cookies (result->updateState, netioRequest->cookies);

	g_free (netioRequest->servauth);
	g_free (netioRequest->authinfo);
	g_free (netioRequest->lastmodified);
	g_free (netioRequest->etag);
	g_free (netioRequest->cookies);
	
	debug4 (DEBUG_UPDATE, "download result - HTTP status: %d, error: %d, netio error:%d, data: %d",
	                      result->httpstatus, 
			      netioRequest->problem, 
			      result->returncode, 
			      result->data);
	g_free (netioRequest);
	
	return result;
}
