/*
 * @file liferea_auth_activatable.h  password provider plugin type
 *
 * Copyright (C) 2012-2026 Lars Windolf <lars.windolf@gmx.de>
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

       	gchar * (*get) (LifereaAuthActivatable * activatable, const gchar *authId);
        void (*set) (LifereaAuthActivatable * activatable, const gchar *authId, const gchar *label, const gchar *value);
	void (*query) (LifereaAuthActivatable * activatable, const gchar *authId);
	void (*store) (LifereaAuthActivatable * activatable, const gchar *authId, const gchar *username, const gchar *password);
};

/**
 * liferea_auth_activatable_get:
 * @activatable:	a #LifereaAuthActivatable.
 * @authId:		a unique auth info id 
 *
 * Returns: (transfer full) (nullable): the secret text (or NULL)
 */
gchar * liferea_auth_activatable_get (LifereaAuthActivatable *activatable,
                                      const gchar *authId);

/**
 * liferea_auth_activatable_set:
 * @activatable:	a #LifereaAuthActivatable
 * @authId:		a unique auth info id 
 * @label:              info string describing the secret
 * @value:              credential
 */
void liferea_auth_activatable_set (LifereaAuthActivatable *activatable,
                                   const gchar *authId,
                                   const gchar *label,
                                   const gchar *value);

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

G_END_DECLS

#endif /* __LIFEREA_AUTH_ACTIVATABLE_H__ */
