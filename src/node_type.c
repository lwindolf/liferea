/**
 * @file node_type.c  node type handling
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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
 
#include "feed.h"
#include "feedlist.h"
#include "node_type.h"
#include "fl_sources/node_source.h"

static GSList *nodeTypes = NULL;

void
node_type_register (nodeTypePtr nodeType)
{

	/* all attributes and methods are mandatory! */
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

void
node_set_type (nodePtr node, nodeTypePtr type) 
{
	node->type = type;
}

const gchar *
node_type_to_str (nodePtr node)
{
	if (IS_FEED (node)) {
		g_assert (NULL != node->data);
		return feed_type_fhp_to_str (((feedPtr)(node->data))->fhp);
	}
	
	return NODE_TYPE (node)->id;
}

nodeTypePtr
node_str_to_type (const gchar *str)
{
	GSList	*iter = nodeTypes;

	g_assert (NULL != str);

	if (g_str_equal (str, ""))	/* type maybe "" if initial download is not yet done */
		return feed_get_node_type ();

	if (NULL != feed_type_str_to_fhp (str))
		return feed_get_node_type ();
		
	/* check against all node types */
	while (iter) {
		if (g_str_equal (str, ((nodeTypePtr)iter->data)->id))
			return (nodeTypePtr)iter->data;
		iter = g_slist_next (iter);
	}

	return NULL;
}

/* Interactive node adding (e.g. feed menu->new subscription) */
void
node_type_request_interactive_add (nodeTypePtr nodeType)
{
	nodePtr		parent;

	parent = feedlist_get_insertion_point ();

	if (0 == (NODE_TYPE (parent->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
		return;

	nodeType->request_add (parent);
}
