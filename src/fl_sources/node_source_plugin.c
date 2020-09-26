/*
* @file node_source_plugin.c  manage node source plugins
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

#include "fl_sources/node_source_plugin.h"

#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "metadata.h"
#include "node.h"
#include "plugins_engine.h"
#include "subscription.h"
#include "fl_sources/node_source_activatable.h"

#include <libpeas/peas-activatable.h>
#include <libpeas/peas-extension-set.h>

static PeasExtensionSet *extensions = NULL;	/*<< Plugin management */

static void
on_extension_added (PeasExtensionSet *extensions,
                    PeasPluginInfo   *info,
                    PeasExtension    *exten,
                    gpointer         user_data)
{
	peas_extension_call (exten, "activate");
}

static void
on_extension_removed (PeasExtensionSet *extensions,
                      PeasPluginInfo   *info,
                      PeasExtension    *exten,
                      gpointer         user_data)
{
	peas_extension_call (exten, "deactivate");
}

void
node_source_plugins_register (void)
{
	g_assert (!extensions);

	extensions = peas_extension_set_new (PEAS_ENGINE (liferea_plugins_engine_get_default ()),
	                                     LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE, NULL);
	g_signal_connect (extensions, "extension-added", G_CALLBACK (on_extension_added), NULL);
	g_signal_connect (extensions, "extension-removed", G_CALLBACK (on_extension_removed), NULL);
	peas_extension_set_foreach (extensions, on_extension_added, NULL);
}

void
node_source_plugin_subscribe (const gchar *typeId, const gchar *serverUrl, const gchar* username, const gchar *password)
{
	debug4 (DEBUG_UPDATE, "Subscribing by plugin: id=%s url=%s user=%s password=%s\n", typeId, serverUrl, username, password);

	nodePtr node = node_source_new (typeId, "");
g_assert(node->subscription);
	metadata_list_set (&node->subscription->metadata, "node-source-subscription-url", serverUrl);

	subscription_set_auth_info (node->subscription, username, password);

	feedlist_node_added (node);
	node_source_update (node);

	db_node_update (node);	/* because of metadate_list_set() above */
}

typedef struct findData {
	const gchar *typeId;
	LifereaNodeSourceActivatable *activatable;
} plugin;

static void
node_source_plugin_foreach_find_by_id (
	PeasExtensionSet *extensions,
	PeasPluginInfo   *info,
	PeasExtension    *exten,
	gpointer         userdata
) {
	LifereaNodeSourceActivatable *activatable = LIFEREA_NODE_SOURCE_ACTIVATABLE (exten);
	LifereaNodeSourceActivatableInterface *iface;
	plugin *fd = (plugin *)userdata;

	g_return_if_fail (LIFEREA_IS_NODE_SOURCE_ACTIVATABLE (activatable));

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->get_id && g_str_equal (fd->typeId, iface->get_id (activatable)))
		fd->activatable = activatable;
}

LifereaNodeSourceActivatable *
node_source_plugin_by_id (const gchar *typeId)
{
	struct findData fd;
	fd.typeId = typeId;
	fd.activatable = NULL;

	peas_extension_set_foreach (extensions, node_source_plugin_foreach_find_by_id, &fd);

	return fd.activatable;
}

void
node_source_plugin_new (const gchar *typeId)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (typeId);
	LifereaNodeSourceActivatableInterface *iface;

	if (!activatable) {
		g_warning("Failed to create node source, implementing plugin could not be resolved!");
		return;
	}

	iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->new)
		iface->new (activatable, typeId);
}

void
node_source_plugin_delete (nodePtr node)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (node->source->type->id);
	LifereaNodeSourceActivatableInterface *iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);

	if (iface->delete)
		iface->delete (activatable, node);
}

void
node_source_plugin_free (nodePtr node)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (node->source->type->id);
	LifereaNodeSourceActivatableInterface *iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->free)
		iface->free (activatable, node);
}

void
node_source_plugin_import (nodePtr node)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (node->source->type->id);
	LifereaNodeSourceActivatableInterface *iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->import)
		iface->import (activatable, node);
}

void
node_source_plugin_export (nodePtr node)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (node->source->type->id);
	LifereaNodeSourceActivatableInterface *iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->export)
		iface->export (activatable, node);
}

gchar *
node_source_plugin_get_feedlist (nodePtr node)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (node->source->type->id);
	LifereaNodeSourceActivatableInterface *iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->get_feedlist)
		return iface->get_feedlist (activatable, node);

	return NULL;
}

void
node_source_plugin_auto_update (nodePtr node)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (node->source->type->id);
	LifereaNodeSourceActivatableInterface *iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->auto_update)
		iface->auto_update (activatable, node);
}

nodePtr
node_source_plugin_add_subscription (nodePtr node, struct subscription *subscription)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (node->source->type->id);
	LifereaNodeSourceActivatableInterface *iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->add_subscription)
		iface->add_subscription (activatable, node, subscription);
}

void
node_source_plugin_remove_node (nodePtr node, nodePtr child)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (node->source->type->id);
	LifereaNodeSourceActivatableInterface *iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->remove_node)
		iface->remove_node (activatable, node, child);
}

nodePtr
node_source_plugin_add_folder (nodePtr node, const gchar *title)
{
	LifereaNodeSourceActivatable *activatable = node_source_plugin_by_id (node->source->type->id);
	LifereaNodeSourceActivatableInterface *iface = LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE (activatable);
	if (iface->add_folder)
		iface->add_folder (activatable, node, title);
}

void
node_source_plugin_item_mark_read (nodePtr node, itemPtr item, gboolean newState)
{
}

void
node_source_plugin_item_set_flag (nodePtr node, itemPtr item, gboolean newState)
{
}

void
node_source_plugin_convert_to_local (nodePtr node)
{
}
