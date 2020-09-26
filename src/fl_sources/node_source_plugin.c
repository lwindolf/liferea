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

/*void
liferea_auth_info_store (gpointer user_data)
{
	peas_extension_set_foreach (liferea_auth_get_extension_set (),
	liferea_auth_info_store_foreach, user_data);
}*/


void
node_source_plugin_new (void)
{
}

void
node_source_plugin_delete (nodePtr node)
{
}

void
node_source_plugin_free (nodePtr node)
{
}

void
node_source_plugin_import (nodePtr node)
{
}

void
node_source_plugin_export (nodePtr node)
{
}

gchar *
node_source_plugin_get_feedlist (nodePtr node)
{
}

void
node_source_plugin_update (nodePtr node)
{
}

void
node_source_plugin_auto_update (nodePtr node)
{
}

nodePtr
node_source_plugin_add_subscription (nodePtr node, struct subscription *subscription)
{
}

void
node_source_plugin_remove_node (nodePtr node, nodePtr child)
{
}

nodePtr
node_source_plugin_add_folder (nodePtr node, const gchar *title)
{
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
