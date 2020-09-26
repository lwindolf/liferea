/*
 * @file node_type_activatable.c  Node Source Plugin Type
 *
 * Copyright (C) 2020 Lars Windolf <lars.windolf@gmx.de>
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

#include "fl_sources/node_source_activatable.h"

#include "fl_sources/node_source.h"

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
liferea_node_source_activatable_init_dummy (void)
{
}

void
liferea_node_source_activatable_default_init (LifereaNodeSourceActivatableInterface *iface)
{
	/* No properties yet */
}

void
liferea_node_source_activatable_activate (LifereaNodeSourceActivatable * activatable)
{
	LifereaNodeSourceActivatableInterface *iface;
	nodeSourceTypePtr nst;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->activate)
		iface->activate (activatable);

	nst = g_new0(struct nodeSourceProvider, 1);
	g_object_get (G_OBJECT (activatable), "id", &nst->id, NULL);
	g_object_get (G_OBJECT (activatable), "name", &nst->name, NULL);
	g_object_get (G_OBJECT (activatable), "capabilities", &nst->capabilities, NULL);

	// Initialization happens through libpeas...
	nst->source_type_init = liferea_node_source_activatable_init_dummy;
	nst->source_type_deinit = liferea_node_source_activatable_init_dummy;

	nst->source_new = NULL;	// FIXME!
	nst->source_delete = NULL; // FIXME!
	nst->source_import = NULL; // FIXME!
	nst->source_export = NULL; // FIXME!
	nst->source_get_feedlist = NULL; // FIXME!

	nst->source_auto_update = iface->auto_update;
	nst->free = iface->free;
	nst->item_set_flag = iface->item_set_flag;
	nst->item_mark_read = iface->item_mark_read;
	nst->add_folder = iface->add_folder;
	nst->add_subscription = iface->add_subscription;
	nst->remove_node = iface->remove_node;
	nst->convert_to_local = iface->convert_to_local;

	node_source_type_register(nst);
}

void
liferea_node_source_activatable_deactivate (LifereaNodeSourceActivatable * activatable)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->deactivate)
		iface->deactivate (activatable);
}

void liferea_node_source_activatable_auto_update (LifereaNodeSourceActivatable *activatable, nodePtr node)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->auto_update)
		iface->auto_update (activatable, node);
}

void liferea_node_source_activatable_add_subscription (LifereaNodeSourceActivatable *activatable, nodePtr node, struct subscription *subscription)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->add_subscription)
		iface->add_subscription (activatable, node, subscription);
}

void liferea_node_source_activatable_remove_node (LifereaNodeSourceActivatable *activatable, nodePtr node, nodePtr child)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);

	if (iface->remove_node)
		iface->remove_node (activatable, node, child);
}

void liferea_node_source_activatable_add_folder (LifereaNodeSourceActivatable *activatable, nodePtr node, const gchar *title)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->add_folder)
		iface->add_folder (activatable, node, title);
}

void liferea_node_source_activatable_item_mark_read (LifereaNodeSourceActivatable *activatable, nodePtr node, itemPtr item, gboolean newState)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->item_mark_read)
		iface->item_mark_read (activatable, node, item, newState);
}

void liferea_node_source_activatable_item_set_flag (LifereaNodeSourceActivatable *activatable, nodePtr node, itemPtr item, gboolean newState)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->item_set_flag)
		iface->item_set_flag (activatable, node, item, newState);
}

void liferea_node_source_activatable_convert_to_local (LifereaNodeSourceActivatable *activatable, nodePtr node)
{
	LifereaNodeSourceActivatableInterface *iface;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->convert_to_local)
		iface->convert_to_local (activatable, node);
}

G_END_DECLS
