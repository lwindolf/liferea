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
#include "fl_sources/node_source_plugin.h"
#include "fl_sources/opml_source.h"

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

struct subscriptionType nodeSourcePluginFeedSubscriptionType = {
	node_source_plugin_feed_subscription_prepare_update_request,
	node_source_plugin_feed_subscription_process_update_result
};

struct subscriptionType nodeSourcePluginSourceSubscriptionType = {
	node_source_plugin_source_subscription_prepare_update_request,
	node_source_plugin_source_subscription_process_update_result
};

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
	nst->id = g_strdup (iface->get_id (activatable));
	nst->name = g_strdup (iface->get_name (activatable));
	nst->capabilities = iface->get_capabilities (activatable);

	nst->sourceSubscriptionType = &nodeSourcePluginSourceSubscriptionType;
	nst->feedSubscriptionType   = &nodeSourcePluginFeedSubscriptionType;

	// Initialization happens through libpeas...
	nst->source_type_init = liferea_node_source_activatable_init_dummy;
	nst->source_type_deinit = liferea_node_source_activatable_init_dummy;

	nst->source_new          = node_source_plugin_new;
	nst->source_delete       = node_source_plugin_delete;
	nst->source_import       = node_source_plugin_import;
	nst->source_export       = opml_source_export;
	nst->source_get_feedlist = opml_source_get_feedlist;

	nst->source_auto_update  = node_source_plugin_auto_update;
	nst->free                = node_source_plugin_free;
	nst->item_set_flag       = node_source_plugin_item_set_flag;
	nst->item_mark_read      = node_source_plugin_item_mark_read;
	nst->add_folder          = node_source_plugin_add_folder;
	nst->add_subscription    = node_source_plugin_add_subscription;
	nst->remove_node         = node_source_plugin_remove_node;
	nst->convert_to_local    = node_source_plugin_convert_to_local;

	if (!node_source_type_register (nst)) {
		g_warning ("Failed to register feed list plugin '%s'!", iface->get_name?iface->get_name (activatable):"unknown");
		return;
	}
}

void
liferea_node_source_activatable_deactivate (LifereaNodeSourceActivatable * activatable)
{
	LifereaNodeSourceActivatableInterface *iface;
	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	node_source_type_unregister (iface->get_name (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->deactivate)
		iface->deactivate (activatable);
}

G_END_DECLS
