/*
 * @file liferea_auth_activatable.h  password provider plugin type
 *
 * Copyright (C) 2012-2024 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _LIFEREA_AUTH_ACTIVATABLE_H__
#define _LIFEREA_AUTH_ACTIVATABLE_H__

#include <glib-object.h>

#include "liferea_activatable.h"

G_BEGIN_DECLS

#define LIFEREA_AUTH_ACTIVATABLE_TYPE (liferea_auth_activatable_get_type ())
G_DECLARE_INTERFACE (LifereaAuthActivatable, liferea_auth_activatable, LIFEREA, AUTH_ACTIVATABLE, LifereaActivatable)

struct _LifereaAuthActivatableInterface
{
	GTypeInterface g_iface;

	void (*query) (LifereaAuthActivatable * activatable, const gchar *authId);
	void (*store) (LifereaAuthActivatable * activatable, const gchar *authId, const gchar *username, const gchar *password);
};

/**
 * liferea_auth_activatable_query:
 * @activatable:	a #LifereaAuthActivatable.
 * @authId:		a unique auth info id 
 *
 * Triggers a query for authentication infos for a given subscription.
 * Expects triggered plugins to use liferea_auth_info_add() to provide
 * any matches.
 */
void liferea_auth_activatable_query (LifereaAuthActivatable *activatable,
                                     const gchar *authId);

/**
 * liferea_auth_activatable_store:
 * @activatable:	a #LifereaAuthActivatable.
 * @authId:		a unique auth info id 
 * @username:		the username to store
 * @password:		the password to store
 *
 * Triggers a query for authentication infos for a given subscription.
 * Expects triggered plugins to use liferea_auth_info_add() to provide
 * any matches.
 */
void liferea_auth_activatable_store (LifereaAuthActivatable * activatable,
                                     const gchar *authId,
                                     const gchar *username,
                                     const gchar *password);

GType liferea_auth_activatable_get_type (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __LIFEREA_AUTH_ACTIVATABLE_H__ */
