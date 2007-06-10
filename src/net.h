/**
 * @file net.h HTTP network access interface
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
 
#ifndef _NET_H
#define _NET_H

#include <glib.h>
#include "update.h"

/* Simple glue layer to abstract network code */

/** 
 * Initialize HTTP client networking support.
 */
void network_init (void);

/**
 * Cleanup HTTP client networking support.
 */
void network_deinit (void);

/**
 * Configure the user agent string to use for HTTP requests.
 * 
 * @param useragent	the user agent string
 */
void network_set_user_agent (gchar *useragent);

/**
 * Configures the network client to use the given proxy
 * host and port setting. If the host name is NULL then
 * no proxy will be used.
 *
 * @param host		the new proxy host
 * @param port		the new proxy port
 */
void network_set_proxy (gchar *host, guint port);

/**
 * Configures the network client to use the given proxy
 * authentification values. If both parameters are NULL 
 * then no authentification will be used.
 *
 * @param username	the username
 * @param password	the password
 */
void network_set_proxy_auth (gchar *username, gchar *password);

/**
 * Process the given request. As a result the different
 * member values of the request will be set or changed.
 *
 * @param request	the request
 */
void network_process_request (struct request *request);

#endif
