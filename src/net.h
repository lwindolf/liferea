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
 * Returns the currently configured proxy host.
 *
 * @returns the proxy host
 */
const gchar * network_get_proxy_host (void);

/**
 * Returns the currently configured proxy port.
 *
 * @returns the proxy port
 */
guint network_get_proxy_port (void);

/**
 * Returns the currently configured proxy user name 
 *
 * @returns the proxy user name (or NULL)
 */
const gchar * network_get_proxy_username (void);

/**
 * Returns the currently configured proxy password.
 *
 * @returns the proxy password (or NULL)
 */
const gchar * network_get_proxy_password (void);

/**
 * Process the given request.
 *
 * @param request	the update request
 * 
 * @returns update result (to be free'd using update_result_free())
 */
updateResultPtr network_process_request (const struct updateRequest * const request);

#endif
