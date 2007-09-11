/**
 * @file node_type.h  node type interface
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

#ifndef _NODE_TYPE_H
#define _NODE_TYPE_H

#include "node.h"

/**
 * Liferea supports different types of nodes in the feed 
 * list. For those node types there can be totally different
 * implementations which just have to follow the interface
 * defined here.
 */

/** node type interface */
typedef struct nodeType {
	gulong		capabilities;	/**< bitmask of node type capabilities */
	gchar		*id;		/**< type id (used for type attribute in OPML export) */
	gpointer	icon;		/**< default icon */
	
	/* For method documentation see the wrappers defined below! 
	   All methods are mandatory for each node type. */
	void    	(*import)		(nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted);
	void    	(*export)		(nodePtr node, xmlNodePtr cur, gboolean trusted);
	itemSetPtr	(*load)			(nodePtr node);
	void 		(*save)			(nodePtr node);
	void		(*update_counters)	(nodePtr node);
	void 		(*process_update_result)(nodePtr node, const struct updateResult * const result, updateFlags flags);
	void		(*remove)		(nodePtr node);
	gchar *		(*render)		(nodePtr node);
	void		(*request_add)		(nodePtr parent);
	void		(*request_properties)	(nodePtr node);
	
	/**
	 * Called to allow node type to clean up it's specific data.
	 * The node structure itself is destroyed after this call.
	 *
	 * @param node		the node
	 */
	void		(*free)			(nodePtr node);
} *nodeTypePtr;

#define NODE_TYPE(node)	(node->type)

/**
 * Registers a new node type. Can be used by feed list
 * plugins to register own node types.
 *
 * @param nodeType	node type info 
 */
void node_type_register(nodeTypePtr nodeType);

/**
 * Changes the node type.
 *
 * @param node	the node
 * @param type	the new type
 */
void node_set_type(nodePtr node, nodeTypePtr type);

/** 
 * Maps node type to string. For feed nodes
 * it maps to the feed type string.
 *
 * @param node	the node 
 *
 * @returns type string (or NULL if unknown)
 */
const gchar *node_type_to_str(nodePtr node);

/** 
 * Maps node type string to type constant.
 *
 * @param type str	the node type as string
 *
 * @returns node type
 */
nodeTypePtr node_str_to_type(const gchar *str);

/**
 * Interactive node adding (e.g. feed menu->new subscription), 
 * launches some dialog that upon success adds a feed of the
 * given type.
 *
 * @param nodeType		the node type
 */
void node_type_request_interactive_add(nodeTypePtr nodeType);

#endif
