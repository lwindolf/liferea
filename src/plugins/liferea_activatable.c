/*
 * @file liferea_activatable.c  Base Plugin Interface
 *
 * Copyright (C) 2024 Lars Windolf <lars.windolf@gmx.de>
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

#include "liferea_activatable.h"

G_DEFINE_INTERFACE (LifereaActivatable, liferea_activatable, G_TYPE_OBJECT)

void
liferea_activatable_default_init (LifereaActivatableInterface *iface)
{
        /* No properties per default */
}

void
liferea_activatable_create_configure_widget (LifereaActivatable * activatable)
{
	LifereaActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_ACTIVATABLE (activatable));

	iface = LIFEREA_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->create_configure_widget)
		iface->create_configure_widget (activatable);
}

void
liferea_activatable_activate (LifereaActivatable * activatable)
{
	LifereaActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_ACTIVATABLE (activatable));

	iface = LIFEREA_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->activate)
		iface->activate (activatable);
}

void
liferea_activatable_deactivate (LifereaActivatable * activatable)
{
	LifereaActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_ACTIVATABLE (activatable));

	iface = LIFEREA_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->deactivate)
		iface->deactivate (activatable);
}