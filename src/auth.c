/*
 * @file auth.c  authentication helpers
 *
 * Copyright (C) 2012 Lars Windolf <lars.windolf@gmx.de>
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
#include "auth_activatable.h"
#include "plugins_engine.h"
#include "subscription.h"

#include <libpeas/peas-activatable.h>
#include <libpeas/peas-extension-set.h>

// FIXME: This should be a member of some object!
static PeasExtensionSet *extensions = NULL;	/**< Plugin management */
static gint		count = 0;		/**< Number of active auth plugins */

static void
on_extension_added (PeasExtensionSet *extensions,
                    PeasPluginInfo   *info,
                    PeasExtension    *exten,
                    gpointer         user_data)
{
	peas_extension_call (exten, "activate");
	count++;
}

static void
on_extension_removed (PeasExtensionSet *extensions,
                      PeasPluginInfo   *info,
                      PeasExtension    *exten,
                      gpointer         user_data)
{
	peas_extension_call (exten, "deactivate");
	count--;
}

static PeasExtensionSet *
liferea_auth_get_extension_set (void)
{
	if (!extensions) {
		extensions = peas_extension_set_new (PEAS_ENGINE (liferea_plugins_engine_get_default ()),
		                             LIFEREA_AUTH_ACTIVATABLE_TYPE, NULL);

		g_signal_connect (extensions, "extension-added", G_CALLBACK (on_extension_added), NULL);
		g_signal_connect (extensions, "extension-removed", G_CALLBACK (on_extension_removed), NULL);

		peas_extension_set_foreach (extensions, on_extension_added, NULL);
	}

	return extensions;
}

static void
liferea_auth_info_store_foreach (PeasExtensionSet *set,
                                 PeasPluginInfo *info,
                                 PeasExtension *exten,
                                 gpointer user_data)
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
	peas_extension_set_foreach (liferea_auth_get_extension_set (),
	                            liferea_auth_info_store_foreach, user_data);
}

void
liferea_auth_info_from_store (const gchar *id, const gchar *username, const gchar *password)
{
	nodePtr	node = node_from_id (id);

	g_assert (NULL != node->subscription);

	node->subscription->updateOptions->username = g_strdup (username);
	node->subscription->updateOptions->password = g_strdup (password);
}

static void
liferea_auth_info_query_foreach (PeasExtensionSet *set,
                               PeasPluginInfo *info,
                               PeasExtension *exten,
                               gpointer data)
{
	liferea_auth_activatable_query (LIFEREA_AUTH_ACTIVATABLE (exten), data);
}

void
liferea_auth_info_query (const gchar *authId)
{
	peas_extension_set_foreach (liferea_auth_get_extension_set (),
	                            liferea_auth_info_query_foreach, (gpointer)authId);
}

gboolean
liferea_auth_has_active_store (void)
{
	return (count > 0);
}
