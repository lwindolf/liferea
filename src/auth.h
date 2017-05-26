/*
 * @file auth.h  authentication helpers
 *
 * Copyright (C) 2012 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _AUTH_H
#define _AUTH_H

#include <glib.h>

/**
 * liferea_auth_has_active_store: 
 *
 * Method to query whether there is an active password store.
 *
 * @returns TRUE if there is a password store
 */
gboolean liferea_auth_has_active_store (void);

/**
 * liferea_auth_info_from_store:
 *
 * @param authId		a node id
 * @param username
 * @param password
 *
 * Allow plugins to provide authentication infos
 */
void liferea_auth_info_from_store (const gchar *authId, const gchar *username, const gchar *password);

/**
 * liferea_auth_info_store:
 *
 * @param subscription		pointer to a subscription
 *
 * Save given authentication info of a given subscription into password store (if available).
 */
void liferea_auth_info_store (gpointer subscription);

/**
 * liferea_auth_info_query:
 *
 * Return auth information for a given node. Each extension able to
 * supply a user name and password for the given id is to synchronously call
 * liferea_auth_info_from_store() to set them.
 * 
 * @param authId		a node id
 * @param username		reference to return username
 * @param password		reference to return password
 */
void liferea_auth_info_query (const gchar *authId);

#endif
