/*
 * @file node_type_activatable.c  Node Source Plugin Type
 *
 * Copyright (C) 2015 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_source_activatable.h"

/**
 * SECTION:node_source_activatable
 * @short_description: Interface for activatable extensions providing a new node source type
 * @see_also: #PeasExtensionSet
 *
 * #LifereaNodeSourceActivatable is an interface which should be implemented by
 * extensions that want to a new node source type (usually online news aggregators)
 **/
G_DEFINE_INTERFACE (LifereaNodeSourceActivatable, liferea_node_source_activatable, G_TYPE_OBJECT)

void
liferea_node_source_activatable_default_init (LifereaNodeSourceActivatableInterface *iface)
{
	/* No properties yet */
}

void
liferea_node_source_activatable_activate (LifereaNodeSourceActivatable * activatable)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (IS_LIFEREA_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->activate)
		iface->activate (activatable);
}

void
liferea_node_source_activatable_deactivate (LifereaNodeSourceActivatable * activatable)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (IS_LIFEREA_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->deactivate)
		iface->deactivate (activatable);
}
