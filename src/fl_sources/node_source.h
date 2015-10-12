/**
 * @file node_source.h  generic node source interface
 * 
 * Copyright (C) 2005-2014 Lars Windolf <lars.windolf@gmx.de>
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
#include "subscription_type.h"
#include "fl_sources/google_reader_api.h"

/* Liferea allows to have different sources in the feed list. These
   sources are called "node sources" henceforth. Node sources can 
   (but do not need to) be single instance only. Node sources do
   provide a subtree of the feed list that can be read-only
   or not. A node source might allow or not allow to add sub folders
   and reorder (DnD) folder contents. A node source might allow 
   hierarchic grouping of its subtree or not. These properties
   are determined by the node source type capability flags.
   
   The node source concept itself is a node type. The implementation
   of this node type can be found in node_source.c.

   The default node source type must be capable of serving as the root
   node for all other source types. This mean it has to ensure to load
   all other node source instances at their insertion nodes in
   the feed list.

   Each source type has to be able to serve user requests and is 
   responsible for keeping its feed list node's states up-to-date.
   A source type implementation can omit all callbacks marked as 
   optional. */

typedef enum {
	NODE_SOURCE_CAPABILITY_IS_ROOT			= (1<<0),	/**< flag only for default feed list source */
	NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION		= (1<<1),	/**< feed list source is user created */
	NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST	= (1<<2),	/**< the feed list tree of the source can be changed */
	NODE_SOURCE_CAPABILITY_ADD_FEED			= (1<<3),	/**< feeds can be added to the source */
	NODE_SOURCE_CAPABILITY_ADD_FOLDER		= (1<<4),	/**< folders can be added to the source */
	NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST	= (1<<5),	/**< the feed list tree of the source can have hierarchic folders */
	NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC		= (1<<6),	/**< the item state can and should be sync'ed with remote */
	NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL		= (1<<7),	/**< node sources of this type can be converted to internal subscription lists */
	NODE_SOURCE_CAPABILITY_GOOGLE_READER_API	= (1<<8),	/**< node sources of this type are Google Reader clones */
	NODE_SOURCE_CAPABILITY_CAN_LOGIN		= (1<<9)	/**< node source needs login (means loginState member is to be used) */
} nodeSourceCapability;

/** Node source state model */
typedef enum { 
	NODE_SOURCE_STATE_NONE = 0,		/**< no authentication tried so far */
	NODE_SOURCE_STATE_IN_PROGRESS,		/**< authentication in progress */
	NODE_SOURCE_STATE_ACTIVE,		/**< authentication succeeded */
	NODE_SOURCE_STATE_NO_AUTH,		/**< authentication has failed */
	NODE_SOURCE_STATE_MIGRATE,		/**< source will be migrated, do not do anything anymore! */
} nodeSourceState;

/** Node source subscription update flags */
typedef enum { 
	/**
	 * Update only the subscription list, and not each node underneath it.
	 * Note: Uses higher 16 bits to avoid conflict.
	 */
	NODE_SOURCE_UPDATE_ONLY_LIST = (1<<16),
	/**
	 * Only login, do not do any updates. 
	 */
	NODE_SOURCE_UPDATE_ONLY_LOGIN = (1<<17)
} nodeSourceUpdate;

/**
 * Number of auth failures after which we stop bothering the user while
 * auto-updating until he manually updates again.
 */
#define NODE_SOURCE_MAX_AUTH_FAILURES		3

/** feed list node source type */
typedef struct nodeSourceType {
	const gchar	*id;		/**< a unique feed list source type identifier */
	const gchar	*name;		/**< a descriptive source name (for preferences and menus) */
	gulong		capabilities;	/**< bitmask of feed list source capabilities */
	googleReaderApi	api;		/**< OPTIONAL endpoint definitions for Google Reader like JSON API */

	/** The subscription type for all child nodes that are subscriptions */
	subscriptionTypePtr	feedSubscriptionType;

	/** The subscription type for the source itself (can be NULL) */
	subscriptionTypePtr	sourceSubscriptionType;

	/* source type loading and unloading methods */
	void		(*source_type_init)(void);
	void 		(*source_type_deinit)(void);

	/**
	 * This OPTIONAL callback is used to create an instance
	 * of the implemented source type. It is to be called by 
	 * the parent source node_request_add_*() implementation. 
	 * Mandatory for all sources except the root source.
	 */
	void 		(*source_new)(void);

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

	/**
	 * Changes the flag state of an item.  This is to allow node source type 
	 * implementations to synchronize remote item states.
	 *
	 * This is an OPTIONAL method.
	 */
	void		(*item_set_flag) (nodePtr node, itemPtr item, gboolean newState);

	/**
	 * Mark an item as read. This is to allow node source type 
	 * implementations to synchronize remote item states.
	 *
	 * This is an OPTIONAL method.
	 */
	void            (*item_mark_read) (nodePtr node, itemPtr item, gboolean newState);
	
	/**
	 * Add a new folder to the feed list provided by node
	 * source. OPTIONAL, but must be implemented when
	 * NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST and
	 * NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST are set.
	 */
	nodePtr		(*add_folder) (nodePtr node, const gchar *title);

	/**
	 * Add a new subscription to the feed list provided
	 * by the node source. OPTIONAL method, that must be implemented
	 * when NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST is set.
	 *
	 * The implementation could propagate the added subscription 
	 * to a remote feed list service.
	 *
	 * The implementation MUST create and return a new child node 
	 * setup with the given subscription which might be changed as necessary.
	 *
	 * The returned node will be automatically added to the feed list UI.
	 * Initial update and state saving will be triggered automatically.
	 */
	nodePtr		(*add_subscription) (nodePtr node, struct subscription *subscription);
	 
	/**
	 * Removes an existing node (subscription or folder) from the feed list 
	 * provided by the node source. OPTIONAL method that must be
	 * implemented when NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST is set.
	 */
	void		(*remove_node) (nodePtr node, nodePtr child);

	/**
	 * Converts all subscriptions to default source subscriptions.
	 *
	 * This is an OPTIONAL method.
	 */
	void		(*convert_to_local) (nodePtr node);

} *nodeSourceTypePtr;

/** feed list source instance */
typedef struct nodeSource {
	nodeSourceTypePtr	type;		/**< node source type of this source instance */
	nodePtr			root;		/**< insertion node of this node source instance */
	GQueue			*actionQueue;	/**< queue for async actions */
	gint			loginState;	/**< The current login state */

	gchar			*authToken;	/**< The authorization token */
	gint			authFailures;	/**< Number of authentication failures */
} *nodeSourcePtr;

/** Use this to cast the node source type from a node structure. */
#define NODE_SOURCE_TYPE(node) ((nodeSourcePtr)(node->source))->type

#define NODE_SOURCE_TYPE_DUMMY_ID "fl_dummy"

/**
 * Get the root node of a feed list source for any given child node.
 *
 * @param node	any child node
 *
 * @returns node source root node
 */
nodePtr node_source_root_from_node (nodePtr node);

/** 
 * Scans the source type list for the root source provider.
 * If found creates a new root source and starts it's import.
 *
 * @returns a newly created root node
 */
nodePtr node_source_setup_root (void);

/**
 * Creates a new source and assigns it to the given new node. 
 * To be used to prepare a source node before adding it to the 
 * feed list. This method takes care of setting the proper source
 * subscription type and setting up the subscription if url != NULL.
 * The caller needs set additional auth info for the subscription.
 *
 * @param node			a newly created node
 * @param nodeSourceType	the node source type
 * @param url			subscription URL
 */
void node_source_new (nodePtr node, nodeSourceTypePtr nodeSourceType, const gchar *url);

/**
 * node_source_set_state
 *
 * Change state of the node source by node
 *
 * @node:		the node source node
 * @newState:		the new state
 */
void node_source_set_state (nodePtr node, gint newState);

/**
 * Store any type of authentication token (e.g. a cookie or session id)
 *
 * FIXME: maybe drop this in favour of node metadata
 *
 * @param node			a node
 * @param token			a string
 */
void node_source_set_auth_token (nodePtr node, gchar *token);

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
 * Called when a new subscription has been added to the node source.
 *
 * @param node		the source node
 * @param subscription	the new subscription
 *
 * @returns a new node intialized with the new subscription
 */
nodePtr node_source_add_subscription (nodePtr node, struct subscription *subscription);

/**
 * Called when an existing subscription is to be removed from a node source.
 *
 * @param node		the source node
 * @param child		the child node to remove
 */
void node_source_remove_node (nodePtr node, nodePtr child);

/**
 * Called when a new folder is to be added to a node source feed list.
 *
 * @param node		the source node
 * @param title		the folder title
 *
 * @returns a new node representing the new folder
 */
nodePtr node_source_add_folder (nodePtr node, const gchar *title);

/**
 * Called to update a nodes folder. If current folder != given folder 
 * the node will be reparented.
 *
 * @param node		any node
 * @param folder	the target folder
 */
void node_source_update_folder (nodePtr node, nodePtr folder);

/* 
 * Find a folder by the name under parent or create it. 
 * 
 * If a node source doesn't provide ids the category display name should be 
 * used as id. The worst thing happening then is to evenly named categories 
 * being merged into one (which the user can easily workaround by renaming 
 * on the remote side).
 * 
 * @param parent	Parent folder (or source root node)
 * @param id		Folder/category id (or NULL)
 * @param name		Folder display name
 *
 * @returns a valid nodePtr
 */
nodePtr node_source_find_or_create_folder (nodePtr parent, const gchar *id, const gchar *label);

/**
 * Called when the read state of an item changes.
 *
 * @param node		the source node 
 * @param item		the affected item
 * @param newState	the new item read state
 */
void node_source_item_mark_read (nodePtr node, itemPtr item, gboolean newState);

/**
 * Called when the flag state of an item changes.
 *
 * @param node		the source node 
 * @param item		the affected item
 * @param newState	the new item flag state
 */
void node_source_item_set_flag (nodePtr node, itemPtr item, gboolean newState);

/**
 * Converts all subscriptions to default source subscriptions.
 *
 * @param node		the source node
 */
void node_source_convert_to_local (nodePtr node);

/**
 * Registers a new node source type. Needs to be called before feed list import!
 * To be used only via NodeSourceTypeActivatable
 *
 * @param type		the type to register
 */
gboolean node_source_type_register (nodeSourceTypePtr type);


/* implementation of the node type interface */

#define IS_NODE_SOURCE(node) (node->type == node_source_get_node_type ())

/** 
 * Returns the node source node type implementation.
 */
nodeTypePtr node_source_get_node_type (void);

#endif
