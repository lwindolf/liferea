/**
 * node_provider.h:  node provider handling
 * 
 * Copyright (C) 2007-2024 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _NODE_PROVIDER_H
#define _NODE_PROVIDER_H

#include "node.h"

#include <libxml/tree.h>
#include <libxml/xmlmemory.h>
#include <gtk/gtk.h>

/* node type capabilities */
enum {
	NODE_CAPABILITY_SHOW_ITEM_FAVICONS	= (1<<0),	/*<< display favicons in item list (useful for recursively viewed leaf node) */
	NODE_CAPABILITY_ADD_CHILDS		= (1<<1),	/*<< allows adding new childs */
	NODE_CAPABILITY_REMOVE_CHILDS		= (1<<2),	/*<< allows removing it's childs */
	NODE_CAPABILITY_SUBFOLDERS		= (1<<3),	/*<< allows creating/removing sub folders */
	NODE_CAPABILITY_REMOVE_ITEMS		= (1<<5),	/*<< allows removing of single items */
	NODE_CAPABILITY_RECEIVE_ITEMS		= (1<<6),	/*<< is a DnD target for item copies */
	NODE_CAPABILITY_REORDER			= (1<<7),	/*<< allows DnD to reorder childs */
	NODE_CAPABILITY_SHOW_UNREAD_COUNT	= (1<<8),	/*<< display the unread item count in the feed list */
	NODE_CAPABILITY_SHOW_ITEM_COUNT		= (1<<9),	/*<< display the absolute item count in the feed list */
	NODE_CAPABILITY_UPDATE			= (1<<10),	/*<< node type always has a subscription and can be updated */
	NODE_CAPABILITY_UPDATE_CHILDS		= (1<<11),	/*<< childs of this node type can be updated */
	NODE_CAPABILITY_UPDATE_FAVICON		= (1<<12),	/*<< this node allows downloading a favicon */
	NODE_CAPABILITY_EXPORT			= (1<<13),	/*<< nodes of this type can be exported safely to OPML */
	NODE_CAPABILITY_EXPORT_ITEMS		= (1<<14)	/*<< contents of this node can be exported as a RSS2 */
};

/*
 * Liferea supports different types of nodes in the feed 
 * list. The type of a feed list node determines how the user
 * can interact with it.
 */

/* feed list node type interface */
typedef struct nodeProvider {
	gulong		capabilities;	/*<< bitmask of node type capabilities */
	const gchar	*id;		/*<< type id (used for type attribute in OPML export) */
	guint		icon;		/*<< default icon for nodes of this type (if no favicon available) */
	
	/* For method documentation see the wrappers defined below! 
	   All methods are mandatory for each node type. */
	void    	(*import)		(Node *node, Node *parent, xmlNodePtr cur, gboolean trusted);
	void    	(*export)		(Node *node, xmlNodePtr cur, gboolean trusted);
	itemSetPtr	(*load)			(Node *node);
	void 		(*save)			(Node *node);
	void		(*update_counters)	(Node *node);
	void		(*remove)		(Node *node);
	gchar *		(*render)		(Node *node);
	gboolean	(*request_add)		(void);
	void		(*request_properties)	(Node *node);
	
	/**
	 * free:
	 * @node:		the node
	 * 
	 * Called to allow node type to clean up it's specific data.
	 * The node structure itself is destroyed after this call.
	 */
	void		(*free)			(Node *node);
} *nodeProviderPtr;

#define NODE_PROVIDER(node)	(node->provider)

/**
 * node_provider_get_name:
 * @node:	the node 
 * 
 * Maps node type to string. For feed nodes
 * it maps to the feed type string.
 * 
 * Returns: type string (or NULL if unknown)
 */
const gchar *node_provider_get_name (Node *node);

/** 
 * node_provider_by_name: (skip)
 * @str: the node type as string
 * 
 * Maps node type string to type constant.
 *
 * Returns: node type
 */
nodeProviderPtr node_provider_by_name (const gchar *str);

/**
 * node_provider_is:
 * @node: the node to check
 * @name: the node provider name to test for
 * 
 * Test whether a node belongs to of a given provider type name
 * 
 * Returns: TRUE if node is of the given type
 */
gboolean node_provider_is (Node *node, const gchar *name);

#define IS_FEED(node)        node_provider_is (node, "feed")
#define IS_FOLDER(node)      node_provider_is (node, "folder")
#define IS_NEWSBIN(node)     node_provider_is (node, "newsbin")
#define IS_VFOLDER(node)     node_provider_is (node, "vfolder")
#define IS_NODE_SOURCE(node) node_provider_is (node, "node_source")

/**
 * node_provider_request_add:
 * @provider:	the node provider
 *
 * Interactive node adding (e.g. feed menu->new subscription), 
 * launches some dialog that upon success adds a feed of the
 * given type.
 *
 * Returns: TRUE on success
 */
gboolean node_provider_request_add (nodeProviderPtr provider);

#endif
