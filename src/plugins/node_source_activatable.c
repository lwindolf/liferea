/*
 * @file node_type_activatable.c  Node Source Plugin Type
 *
 * Copyright (C) 2015-2024 Lars Windolf <lars.windolf@gmx.de>
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

G_DEFINE_INTERFACE (LifereaNodeSourceActivatable, liferea_node_source_activatable, LIFEREA_TYPE_ACTIVATABLE)

void
liferea_node_source_activatable_default_init (LifereaNodeSourceActivatableInterface *iface)
{
	/* No properties yet */
}