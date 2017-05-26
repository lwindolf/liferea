/**
 * @file node_type.c  node type handling
 * 
 * Copyright (C) 2007-2008 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_type.h"
 
#include "feed.h"
#include "feed_parser.h"
#include "feedlist.h"
#include "folder.h"
#include "newsbin.h"
#include "node.h"
#include "vfolder.h"
#include "fl_sources/node_source.h"

static GSList *nodeTypes = NULL;

static void
node_type_register (nodeTypePtr nodeType)
{

	/* all attributes and methods except free() are mandatory! */
	g_assert (nodeType->id);
	g_assert (nodeType->import);
	g_assert (nodeType->export);
	g_assert (nodeType->load);
	g_assert (nodeType->save);
	g_assert (nodeType->update_counters);
	g_assert (nodeType->remove);
	g_assert (nodeType->render);
	g_assert (nodeType->request_add);
	g_assert (nodeType->request_properties);
	
	nodeTypes = g_slist_append (nodeTypes, (gpointer)nodeType);
}

const gchar *
node_type_to_str (nodePtr node)
{
	/* To distinguish different feed formats (Atom, RSS...) we do
	   return different type identifiers for feed subscriptions... */
	if (IS_FEED (node)) {
		g_assert (NULL != node->data);
		return feed_type_fhp_to_str (((feedPtr)(node->data))->fhp);
	}
	
	return NODE_TYPE (node)->id;
}

nodeTypePtr
node_str_to_type (const gchar *str)
{
	GSList	*iter;

	g_assert (NULL != str);
	
	/* initialize known node types the first time... */
	if (!nodeTypes) {
		node_type_register (feed_get_node_type ());
		node_type_register (root_get_node_type ());
		node_type_register (folder_get_node_type ());
		node_type_register (vfolder_get_node_type ());
		node_type_register (node_source_get_node_type ());
		node_type_register (newsbin_get_node_type ());
	}

	if (g_str_equal (str, ""))	/* type maybe "" if initial download is not yet done */
		return feed_get_node_type ();

	if (NULL != feed_type_str_to_fhp (str))
		return feed_get_node_type ();
		
	/* check against all node types */
	iter = nodeTypes;
	while (iter) {
		if (g_str_equal (str, ((nodeTypePtr)iter->data)->id))
			return (nodeTypePtr)iter->data;
		iter = g_slist_next (iter);
	}

	return NULL;
}

/* Interactive node adding (e.g. feed menu->new subscription) */
gboolean
node_type_request_interactive_add (nodeTypePtr nodeType)
{

	if (!feedlist_is_writable ())
		return FALSE;

	return nodeType->request_add ();
}
