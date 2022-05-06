/**
 * @file net.h  HTTP network access interface
 *
 * Copyright (C) 2007-2021 Lars Windolf <lars.windolf@gmx.de>
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
 * network_get_special_user_agent:
 * Get effective user agent. Considers special user settings from environment.
 *
 * @returns: user agent string (to be free'd by caller)
 */
gchar * network_get_user_agent (void);

typedef enum {
	PROXY_DETECT_MODE_AUTO = 0, 	/* Use system settings */
	PROXY_DETECT_MODE_NONE,		/* No Proxy */
	PROXY_DETECT_MODE_MANUAL	/* Manually configured proxy */
} ProxyDetectMode;

/**
 * Configures the network client to use the given proxy
 * settings. If the host name is NULL then no proxy will
 * be used.
 *
 * @param mode		indicate whether to use the system setting, no proxy or the following parameters.
 * @param host		the new proxy host
 * @param port		the new proxy port
 * @param user		the new proxy username or NULL
 * @param password	the new proxy password or NULL
 */
void network_set_proxy (ProxyDetectMode mode, gchar *host, guint port, gchar *user, gchar *password);

/**
 * Returns the proxy detect mode.
 *
 */
ProxyDetectMode network_get_proxy_detect_mode (void);

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
 * Process the given update job.
 *
 * @param request	the update request
 */
void network_process_request (const updateJobPtr job);

/**
 * Returns explanation string for the given network error code.
 *
 * @param status	libsoup status code
 *
 * @returns explanation string
 */
const char * network_strerror (gint status);

#endif
