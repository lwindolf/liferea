/**
 * node_provider.c:  node provider handling
 * 
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_provider.h"
 
#include "feed_parser.h"
#include "feedlist.h"
#include "node_providers/feed.h"
#include "node_providers/folder.h"
#include "node_providers/newsbin.h"
#include "node_providers/vfolder.h"
#include "node_source.h"

static GSList *providers = NULL;

static void
node_provider_register (nodeProviderPtr provider)
{
	/* all attributes and methods except free() are mandatory! */
	g_assert (provider->id);
	g_assert (provider->import);
	g_assert (provider->export);
	g_assert (provider->load);
	g_assert (provider->save);
	g_assert (provider->update_counters);
	g_assert (provider->remove);
	g_assert (provider->request_add);
	g_assert (provider->request_properties);
	
	providers = g_slist_append (providers, (gpointer)provider);
}

const gchar *
node_provider_get_name (Node *node)
{
	/* To distinguish different feed formats (Atom, RSS...) we do
	   return different type identifiers for feed subscriptions... */
	if (IS_FEED (node)) {
		g_assert (NULL != node->data);
		return feed_type_fhp_to_str (((feedPtr)(node->data))->fhp);
	}
	
	return NODE_PROVIDER (node)->id;
}

nodeProviderPtr
node_provider_by_name (const gchar *str)
{
	GSList	*iter;

	g_assert (NULL != str);
	
	/* initialize known node types the first time... */
	if (!providers) {
		node_provider_register (feed_get_provider ());
		node_provider_register (root_get_provider ());
		node_provider_register (folder_get_provider ());
		node_provider_register (vfolder_get_provider ());
		node_provider_register (node_source_get_provider ());
		node_provider_register (newsbin_get_provider ());
	}

	if (g_str_equal (str, ""))	/* type maybe "" if initial download is not yet done */
		return feed_get_provider ();

	if (NULL != feed_type_str_to_fhp (str))
		return feed_get_provider ();
		
	/* check against all node types */
	iter = providers;
	while (iter) {
		if (g_str_equal (str, ((nodeProviderPtr)iter->data)->id))
			return (nodeProviderPtr)iter->data;
		iter = g_slist_next (iter);
	}

	return NULL;
}

gboolean
node_provider_is (Node *node, const gchar *name)
{
	g_assert (node);
	g_assert (name);
	return g_str_equal (node->provider->id, name);
}


/* Interactive node adding (e.g. feed menu->new subscription) */
gboolean
node_provider_request_add (nodeProviderPtr provider)
{

	if (!feedlist_is_writable ())
		return FALSE;

	return provider->request_add ();
}
