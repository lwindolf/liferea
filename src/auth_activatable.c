/*
 * @file auth_activatable.c  Auth Plugin Type
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

#include "auth_activatable.h"

/**
 * SECTION:liferea_auth_activatable
 * @short_description: Interface for activatable extensions providing auth infos
 * @see_also: #PeasExtensionSet
 *
 * #LifereaAuthActivatable is an interface which should be implemented by
 * extensions that want to provide a password store
 **/
G_DEFINE_INTERFACE (LifereaAuthActivatable, liferea_auth_activatable, G_TYPE_OBJECT)

void
liferea_auth_activatable_default_init (LifereaAuthActivatableInterface *iface)
{
	/* No properties yet */
}

void
liferea_auth_activatable_activate (LifereaAuthActivatable * activatable)
{
	LifereaAuthActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_AUTH_ACTIVATABLE (activatable));

	iface = LIFEREA_AUTH_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->activate)
		iface->activate (activatable);
}

void
liferea_auth_activatable_deactivate (LifereaAuthActivatable * activatable)
{
	LifereaAuthActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_AUTH_ACTIVATABLE (activatable));

	iface = LIFEREA_AUTH_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->deactivate)
		iface->deactivate (activatable);
}

void
liferea_auth_activatable_query (LifereaAuthActivatable * activatable,
                                const gchar *authId)
{
	LifereaAuthActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_AUTH_ACTIVATABLE (activatable));

	iface = LIFEREA_AUTH_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->query)
		iface->query (activatable, authId);
}

void
liferea_auth_activatable_store (LifereaAuthActivatable * activatable,
                                const gchar *authId,
                                const gchar *username,
                                const gchar *password)
{
	LifereaAuthActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_AUTH_ACTIVATABLE (activatable));

	iface = LIFEREA_AUTH_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->store)
		iface->store (activatable, authId, username, password);
}


