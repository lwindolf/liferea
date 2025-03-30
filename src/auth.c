/*
 * @file auth.c  authentication helpers
 *
 * Copyright (C) 2012-2024 Lars Windolf <lars.windolf@gmx.de>
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

#include "auth.h"
#include "plugins/auth_activatable.h"
#include "plugins/plugins_engine.h"
#include "subscription.h"

static void
liferea_auth_info_store_foreach (gpointer exten, gpointer user_data)
{
	subscriptionPtr subscription = (subscriptionPtr)user_data;

	g_assert (subscription != NULL);
	g_assert (subscription->node != NULL);
	g_assert (subscription->updateOptions != NULL);

	liferea_auth_activatable_store (LIFEREA_AUTH_ACTIVATABLE (exten),
	                                subscription->node->id,
	                                subscription->updateOptions->username,
	                                subscription->updateOptions->password);
}

void
liferea_auth_info_store (gpointer user_data)
{
	liferea_plugin_call (LIFEREA_AUTH_ACTIVATABLE_TYPE, &liferea_auth_info_store_foreach, user_data);
}

void
liferea_auth_info_from_store (const gchar *id, const gchar *username, const gchar *password)
{
	Node	*node = node_from_id (id);

	g_assert (NULL != node->subscription);

	node->subscription->updateOptions->username = g_strdup (username);
	node->subscription->updateOptions->password = g_strdup (password);
}

static void
liferea_auth_info_query_foreach (gpointer exten, gpointer data)
{
	liferea_auth_activatable_query (LIFEREA_AUTH_ACTIVATABLE (exten), data);
}

void
liferea_auth_info_query (const gchar *authId)
{
	liferea_plugin_call (LIFEREA_AUTH_ACTIVATABLE_TYPE, &liferea_auth_info_query_foreach, (gpointer)authId);
}

gboolean
liferea_auth_has_active_store (void)
{
	return liferea_plugin_is_active (LIFEREA_AUTH_ACTIVATABLE_TYPE);
}
