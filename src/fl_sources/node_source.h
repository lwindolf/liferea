/**
 * @file node_source.h generic feed list provider interface
 * 
 * Copyright (C) 2005-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifndef _NODE_SOURCE_H
#define _NODE_SOURCE_H

#include <glib.h>
#include <gmodule.h>
#include "node.h"
#include "node_type.h"

/* Liferea allows to have different sources in the feed list.
   Sources can but do not need to be single instance only. Sources
   provide a subtree of the feed list that can be read-only
   or not. A source might allow or not allow to add sub folders
   and reorder (DnD) folder contents.

   The default node source type must be capable of serving as the root
   node for all other source types. This mean it has to ensure to load
   all other node source instances at their insertion nodes in
   the feed list.

   Each source type has to be able to serve user requests and is 
   responsible for keeping its feed list node's states up-to-date.
   A source type implementation can omit all callbacks marked as 
   optional. */

#define NODE_SOURCE_TYPE_API_VERSION 5

enum {
	NODE_SOURCE_CAPABILITY_IS_ROOT			= (1<<0),	/**< flag only for default feed list source */
	NODE_SOURCE_CAPABILITY_MULTI_INSTANCES		= (1<<1),	/**< allows multiple source instances */
	NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION		= (1<<2),	/**< feed list source is user created */
	NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST	= (1<<3)	/**< the feed list tree of the source can be changed */
};

/** feed list node source type */
typedef struct nodeSourceType {
	unsigned int	api_version;
	
	gchar		*id;		/**< a unique feed list source type identifier */
	gchar		*name;		/**< a descriptive source name (for preferences and menus) */
	gchar		*description;	/**< more detailed source type description (up to some sentences) */
	gulong		capabilities;	/**< bitmask of feed list source capabilities */

	/* source type loading and unloading methods */
	void		(*source_type_init)(void);
	void 		(*source_type_deinit)(void);

	/**
	 * This OPTIONAL callback is used to create an instance
	 * of the implemented source type. It is to be called by 
	 * the parent source node_request_add_*() implementation. 
	 * Mandatory for all sources except the root source.
	 */
	void 		(*source_new)(nodePtr parent);

	/**
	 * This OPTIONAL callback is used to delete an instance
	 * of the implemented source type. It is to be called
	 * by the parent source node_remove() implementation.
	 * Mandatory for all sources except the root provider source.
	 */
	void 		(*source_delete)(nodePtr node);

	/**
	 * This MANDATORY method is called when the source is to
	 * create the feed list subtree attached to the source root
	 * node.
	 */
	void 		(*source_import)(nodePtr node);

	/**
	 * This MANDATORY method is called when the source is to
	 * save it's feed list subtree (if necessary at all). This
	 * is not a request to save the data of the attached nodes!
	 */
	void 		(*source_export)(nodePtr node);
	
	/**
	 * This MANDATORY method is called to get an OPML representation
	 * of the feedlist of the given node source. Returns a newly
	 * allocated filename string that is to be freed by the
	 * caller.
	 */
	gchar *		(*source_get_feedlist)(nodePtr node);
	
	/**
	 * This MANDATARY method is called to force the source to update
	 * its subscriptions list and the child subscriptions themselves.
	 */
	void		(*source_update)(nodePtr node);
	
	/**
	 * This MANDATARY method is called to request the source to update
	 * its subscriptions list and the child subscriptions according
	 * the its update interval.
	 */
	void		(*source_auto_update)(nodePtr node);
	
	/**
	 * Frees all data of the given node source instance. To be called
	 * during node_free() for a source node.
	 */
	void		(*free) (nodePtr node);
} *nodeSourceTypePtr;

/** feed list source instance */
typedef struct nodeSource {
	nodeSourceTypePtr	type;		/**< node source type of this source instance */
	nodePtr			root;		/**< insertion node of this node source instance */
} *nodeSourcePtr;

/** Use this to cast the node source type from a node structure. */
#define NODE_SOURCE_TYPE(node) ((nodeSourcePtr)(node->source))->type

#define NODE_SOURCE_TYPE_DUMMY_ID "fl_dummy"

/** 
 * Scans the source type list for the root source provider.
 * If found creates a new root source and starts it's import.
 *
 * @returns a newly created root node
 */
nodePtr node_source_setup_root (void);

/**
 * Registers a node source type.
 *
 * @param nodeSourceType	node source type
 *
 * @returns TRUE on success
 */
gboolean node_source_type_register (nodeSourceTypePtr type);

/**
 * Creates a new source and assigns it to the given new node. 
 * To be used to prepare a source node before adding it to the 
 * feed list.
 *
 * @param node			a newly created node
 * @param nodeSourceType	the node source type
 */
void node_source_new (nodePtr node, nodeSourceTypePtr nodeSourceType);

/**
 * Force the source to update its subscription list and
 * the child subscriptions themselves.
 *
 * @param node			the source node
 */
void node_source_update (nodePtr node);

/**
 * Request the source to update its subscription list and
 * the child subscriptions if necessary according to the
 * update interval of the source.
 *
 * @param node			the source node
 */
void node_source_auto_update (nodePtr node);

/**
 * Launches a source creation dialog. The new source
 * instance will be added to the given node.
 *
 * @param node	the parent node
 */
void ui_node_source_type_dialog (nodePtr node);

/* implementation of the node type interface */

#define IS_NODE_SOURCE(node) (node->type == node_source_get_node_type ())

/** 
 * Returns the node source node type implementation.
 */
nodeTypePtr node_source_get_node_type (void);

#endif
