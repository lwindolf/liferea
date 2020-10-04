/*
 * @file node_source_plugin.h  manage node source plugins
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

#ifndef _NODE_SOURCE_PLUGIN_H
#define _NODE_SOURCE_PLUGIN_H

#include "fl_sources/node_source.h"

/* As the node source plugin code is an GIR adapter for the node
   source type interface it needs to reimplement the node source
   interface. So all methods below are a copy of the respective
   node_source_*() methods. For simplicity the documentation is
   not copied */

/* ======================== node source adapter ============================= */

/**
 * node_source_plugin_new: (skip)
 */
void node_source_plugin_new (const gchar *typeId);

/**
 * node_source_plugin_delete: (skip)
 */
void node_source_plugin_delete (Node *node);

/**
 * node_source_plugin_free: (skip)
 */
void node_source_plugin_free (Node *node);

/**
 * node_source_plugin_import: (skip)
 */
void node_source_plugin_import (Node *node);

/**
 * node_source_plugin_export: (skip)
 */
void node_source_plugin_export (Node *node);

/**
 * node_source_plugin_get_feedlist: (skip)
 */
gchar * node_source_plugin_get_feedlist (Node *node);

/**
 * node_source_plugin_update: (skip)
 */
void node_source_plugin_update (Node *node);

/**
 * node_source_plugin_auto_update: (skip)
 */
void node_source_plugin_auto_update (Node *node);

/**
 * node_source_plugin_add_subscription: (skip)
 */
Node *node_source_plugin_add_subscription (Node *node, Subscription *subscription);

/**
 * node_source_pluign_remove_node: (skip)
 */
void node_source_plugin_remove_node (Node *node, Node *child);

/**
 * node_source_plugin_add_folder: (skip)
 */
Node *node_source_plugin_add_folder (Node *node, const gchar *title);

/**
 * node_source_plugin_item_mark_read: (skip)
 */
void node_source_plugin_item_mark_read (Node *node, itemPtr item, gboolean newState);

/**
 * node_source_plugin_set_item_flag: (skip)
 */
void node_source_plugin_item_set_flag (Node *node, itemPtr item, gboolean newState);

/**
 * node_source_plugin_convert_to_local: (skip)
 */
void node_source_plugin_convert_to_local (Node *node);

/* ==================== feed subscription type adapter ====================== */

/**
 * node_source_plugin_feed_subscription_prepare_update_request: (skip)
 */
gboolean node_source_plugin_feed_subscription_prepare_update_request (Subscription * subscription, UpdateRequest *request);

/**
 * node_source_plugin_feed_subscription_process_update_result: (skip)
 */
void node_source_plugin_feed_subscription_process_update_result (Subscription * subscription, const UpdateResult * const result, updateFlags flags);

/* ==================== source subscription type adapter ==================== */

/**
 * node_source_plugin_source_subscription_prepare_update_request: (skip)
 */
gboolean node_source_plugin_source_subscription_prepare_update_request (Subscription * subscription, UpdateRequest *request);

/**
 * node_source_plugin_source_subscription_process_update_result: (skip)
 */
void node_source_plugin_source_subscription_process_update_result (Subscription * subscription, const UpdateResult * const result, updateFlags flags);

/* ======================== helper methods ================================== */

/**
 * node_source_plugins_register: (skip)
 *
 * Make all node source plugins initialize
 */
void node_source_plugins_register (void);

/**
 * node_source_plugin_subscribe:
 * @typeId:    node source type id
 * @username:  username for authentication
 * @password:  password for authentication
 * @serverUrl: node source subscription URL
 *
 * To be called by node source provider plugins on user input
 * of all subscription details
 */
void node_source_plugin_subscribe (const gchar *typeId, const gchar *username, const gchar *password, const gchar *serverUrl);

#endif
