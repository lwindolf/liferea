/**
 * @file node_type.h  node type interface
 * 
 * Copyright (C) 2007-2012 Lars Windolf <lars.windolf@gmx.de>
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

#include <libxml/xmlmemory.h>
#include <gtk/gtk.h>

/** node type capabilities */
enum {
	NODE_CAPABILITY_SHOW_ITEM_FAVICONS	= (1<<0),	/**< display favicons in item list (useful for recursively viewed leaf node) */
	NODE_CAPABILITY_ADD_CHILDS		= (1<<1),	/**< allows adding new childs */
	NODE_CAPABILITY_REMOVE_CHILDS		= (1<<2),	/**< allows removing it's childs */
	NODE_CAPABILITY_SUBFOLDERS		= (1<<3),	/**< allows creating/removing sub folders */
	NODE_CAPABILITY_REMOVE_ITEMS		= (1<<5),	/**< allows removing of single items */
	NODE_CAPABILITY_RECEIVE_ITEMS		= (1<<6),	/**< is a DnD target for item copies */
	NODE_CAPABILITY_REORDER			= (1<<7),	/**< allows DnD to reorder childs */
	NODE_CAPABILITY_SHOW_UNREAD_COUNT	= (1<<8),	/**< display the unread item count in the feed list */
	NODE_CAPABILITY_SHOW_ITEM_COUNT		= (1<<9),	/**< display the absolute item count in the feed list */
	NODE_CAPABILITY_UPDATE			= (1<<10),	/**< node type always has a subscription and can be updated */
	NODE_CAPABILITY_UPDATE_CHILDS		= (1<<11),	/**< childs of this node type can be updated */
	NODE_CAPABILITY_UPDATE_FAVICON		= (1<<12),	/**< this node allows downloading a favicon */
	NODE_CAPABILITY_EXPORT			= (1<<13),	/**< nodes of this type can be exported safely to OPML */
	NODE_CAPABILITY_EXPORT_ITEMS		= (1<<14)	/**< contents of this node can be exported as a RSS2 */
};

/**
 * Liferea supports different types of nodes in the feed 
 * list. The type of a feed list node determines how the user
 * can interact with it.
 */

/** node type interface */
typedef struct nodeType {
	gulong		capabilities;	/**< bitmask of node type capabilities */
	const gchar	*id;		/**< type id (used for type attribute in OPML export) */
	guint		icon;		/**< default icon for nodes of this type (if no favicon available) */
	
	/* For method documentation see the wrappers defined below! 
	   All methods are mandatory for each node type. */
	void    	(*import)		(nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted);
	void    	(*export)		(nodePtr node, xmlNodePtr cur, gboolean trusted);
	itemSetPtr	(*load)			(nodePtr node);
	void 		(*save)			(nodePtr node);
	void		(*update_counters)	(nodePtr node);
	void		(*remove)		(nodePtr node);
	gchar *		(*render)		(nodePtr node);
	gboolean	(*request_add)		(void);
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
 * Maps node type to string. For feed nodes
 * it maps to the feed type string.
 *
 * @param node	the node 
 *
 * @returns type string (or NULL if unknown)
 */
const gchar *node_type_to_str (nodePtr node);

/** 
 * Maps node type string to type constant.
 *
 * @param type str	the node type as string
 *
 * @returns node type
 */
nodeTypePtr node_str_to_type (const gchar *str);

/**
 * Interactive node adding (e.g. feed menu->new subscription), 
 * launches some dialog that upon success adds a feed of the
 * given type.
 *
 * @param nodeType		the node type
 *
 * @returns TRUE on success
 */
gboolean node_type_request_interactive_add (nodeTypePtr nodeType);

#endif
