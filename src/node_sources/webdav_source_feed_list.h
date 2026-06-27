/**
 * @file webdav_source_feed_list.h  WebDAV source feed list handling
 *
 * Copyright (C) 2026  Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _WEBDAV_SOURCE_FEED_LIST_H
#define _WEBDAV_SOURCE_FEED_LIST_H

#include "node_sources/webdav_source.h"

#include "subscription.h"
#include "update.h"

void webdav_source_feed_list_upload (Node *root);
void webdav_source_feed_list_import (Node *root);

void webdav_request_get_feed_with_callback (Node *root, const gchar *feed_id, update_flow_cb callback, gpointer callback_data);
void webdav_request_get_state_with_callback (Node *root, const gchar *feed_id, update_flow_cb callback, gpointer callback_data);

gboolean webdav_subscription_prepare_update_request (subscriptionPtr subscription, UpdateRequest *request);
void webdav_subscription_process_update_result (subscriptionPtr subscription, const UpdateResult * const result, updateFlags flags);

extern struct subscriptionType webdavSourceSubscriptionType;

#endif