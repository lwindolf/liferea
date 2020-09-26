/*
 * @file node_source_activatable_activatable.h  Node Source Plugin Type
 *
 * Copyright (C) 2020 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _LIFEREA_NODE_SOURCE_ACTIVATABLE_H__
#define _LIFEREA_NODE_SOURCE_ACTIVATABLE_H__

#include <glib-object.h>

#include "item.h"
#include "node.h"

G_BEGIN_DECLS

#define LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE		(liferea_node_source_activatable_get_type ())
#define LIFEREA_NODE_SOURCE_ACTIVATABLE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE, LifereaNodeSourceActivatable))
#define LIFEREA_NODE_SOURCE_ACTIVATABLE_IFACE(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE, LifereaNodeSourceActivatableInterface))
#define LIFEREA_IS_NODE_SOURCE_ACTIVATABLE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE))
#define LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE(obj)	(G_TYPE_INSTANCE_GET_INTERFACE ((obj), LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE, LifereaNodeSourceActivatableInterface))

typedef struct _LifereaNodeSourceActivatable LifereaNodeSourceActivatable;
typedef struct _LifereaNodeSourceActivatableInterface LifereaNodeSourceActivatableInterface;

struct _LifereaNodeSourceActivatableInterface
{
	GTypeInterface g_iface;

	void (*activate) (LifereaNodeSourceActivatable *activatable);
	void (*deactivate) (LifereaNodeSourceActivatable *activatable);


	// FIXME: get_name()
	// FIXME: get_id()
	// FIXME: get_capabilities()

	// FIXME: source subscription interface

	// FIXME: feed subscription interface

	/*
	 * This OPTIONAL callback is used to delete an instance
	 * of the implemented source type. It is to be called
	 * by the parent source node_remove() implementation.
	 * Mandatory for all sources except the root provider source.
	 */
	void 		(*delete)(LifereaNodeSourceActivatable *activatable, nodePtr node);

	/*
	 * This MANDATORY method is called when the source is to
	 * create the feed list subtree attached to the source root
	 * node.
	 */
	void 		(*import)(LifereaNodeSourceActivatable *activatable, nodePtr node);

	/*
	 * This MANDATORY method is called when the source is to
	 * save it's feed list subtree (if necessary at all). This
	 * is not a request to save the data of the attached nodes!
	 */
	void 		(*export)(LifereaNodeSourceActivatable *activatable, nodePtr node);

	/*
	 * This MANDATORY method is called to get an OPML representation
	 * of the feedlist of the given node source. Returns a newly
	 * allocated filename string that is to be freed by the
	 * caller.
	 */
	gchar *		(*get_feedlist)(LifereaNodeSourceActivatable *activatable, nodePtr node);

	/*
	 * This MANDATARY method is called to request the source to update
	 * its subscriptions list and the child subscriptions according
	 * the its update interval.
	 */
	void		(*auto_update)(LifereaNodeSourceActivatable *activatable, nodePtr node);

	/*
	 * Frees all data of the given node source instance. To be called
	 * during node_free() for a source node.
	 */
	void		(*free) (LifereaNodeSourceActivatable *activatable, nodePtr node);

	/*
	 * Changes the flag state of an item.  This is to allow node source type
	 * implementations to synchronize remote item states.
	 *
	 * This is an OPTIONAL method.
	 */
	void		(*item_set_flag) (LifereaNodeSourceActivatable *activatable, nodePtr node, itemPtr item, gboolean newState);

	/*
	 * Mark an item as read. This is to allow node source type
	 * implementations to synchronize remote item states.
	 *
	 * This is an OPTIONAL method.
	 */
	void            (*item_mark_read) (LifereaNodeSourceActivatable *activatable, nodePtr node, itemPtr item, gboolean newState);

	/*
	 * Add a new folder to the feed list provided by node
	 * source. OPTIONAL, but must be implemented when
	 * liferea_node_source_activatable_CAPABILITY_WRITABLE_FEEDLIST and
	 * liferea_node_source_activatable_CAPABILITY_HIERARCHIC_FEEDLIST are set.
	 *
	 * Adding of the folder to the feed list has to happen asynchronously.
	 */
	void		(*add_folder) (LifereaNodeSourceActivatable *activatable, nodePtr node, const gchar *title);

	/*
	 * Add a new subscription to the feed list provided
	 * by the node source. OPTIONAL method, that must be implemented
	 * when liferea_node_source_activatable_CAPABILITY_WRITABLE_FEEDLIST is set.
	 *
	 * The implementation could propagate the added subscription
	 * to a remote feed list service.
	 *
	 * The implementation MUST create a new child node with the given
	 * subscription which might be changed as necessary. Adding of the
	 * node to the feed list has to happen asynchronously.
	 */
	void		(*add_subscription) (LifereaNodeSourceActivatable *activatable, nodePtr node, struct subscription *subscription);

	/*
	 * Removes an existing node (subscription or folder) from the feed list
	 * provided by the node source. OPTIONAL method that must be
	 * implemented when liferea_node_source_activatable_CAPABILITY_WRITABLE_FEEDLIST is set.
	 */
	void		(*remove_node) (LifereaNodeSourceActivatable *activatable, nodePtr node, nodePtr child);

	/*
	 * Converts all subscriptions to default source subscriptions.
	 *
	 * This is an OPTIONAL method.
	 */
	void		(*convert_to_local) (LifereaNodeSourceActivatable *activatable, nodePtr node);
};

GType liferea_node_source_activatable_get_type (void) G_GNUC_CONST;

void liferea_node_source_activatable_activate (LifereaNodeSourceActivatable *activatable);

void liferea_node_source_activatable_deactivate (LifereaNodeSourceActivatable *activatable);

/**
 * liferea_node_source_activatable_update:
 * @activatable:	the node source
 * @node:		the node source root node
 *
 * Force the source to update its subscription list and
 * the child subscriptions themselves.
 */
void liferea_node_source_activatable_update (LifereaNodeSourceActivatable *activatable, nodePtr node);

/**
 * liferea_node_source_activatable_auto_update:
 * @activatable:	the node source
 * @node:		the node source root node
 *
 * Request the source to update its subscription list and
 * the child subscriptions if necessary according to the
 * update interval of the source.
 */
void liferea_node_source_activatable_auto_update (LifereaNodeSourceActivatable *activatable, nodePtr node);

/**
 * liferea_node_source_activatable_add_subscription:
 * @activatable:	the node source
 * @node:		the node source root node
 * @subscription:	the new subscription
 *
 * Called when a new subscription has been added to the node source.
 */
void liferea_node_source_activatable_add_subscription (LifereaNodeSourceActivatable *activatable, nodePtr node, struct subscription *subscription);

/**
 * liferea_node_source_activatable_remove_node:
 * @activatable:	the node source
 * @node:		the node source root node
 * @child:		the child node to remove
 *
 * Called when an existing subscription is to be removed from a node source.
 */
void liferea_node_source_activatable_remove_node (LifereaNodeSourceActivatable *activatable, nodePtr node, nodePtr child);

/**
 * liferea_node_source_activatable_add_folder:
 * @activatable:	the node source
 * @node:		the node source root node
 * @title:      	the folder title
 *
 * Called when a new folder is to be added to a node source feed list.
 */
void liferea_node_source_activatable_add_folder (LifereaNodeSourceActivatable *activatable, nodePtr node, const gchar *title);

/**
 * liferea_node_source_activatable_update_folder:
 * @activatable:	the node source
 * @node:		any node
 * @folder:     	the target folder
 *
 * Called to update a nodes folder. If current folder != given folder
 * the node will be reparented.
 */
void liferea_node_source_activatable_update_folder (LifereaNodeSourceActivatable *activatable, nodePtr node, nodePtr folder);

/**
 * liferea_node_source_activatable_item_mark_read:
 * @activatable:	the node source
 * @node:		the node source root node
 * @item:		the affected item
 * @newState:   	the new item read state
 *
 * Called when the read state of an item changes.
 */
void liferea_node_source_activatable_item_mark_read (LifereaNodeSourceActivatable *activatable, nodePtr node, itemPtr item, gboolean newState);

/**
 * liferea_node_source_activatable_set_item_flag:
 * @activatable:	the node source
 * @node:		the node source root node
 * @item:		the affected item
 * @newState:   	the new item flag state
 *
 * Called when the flag state of an item changes.
 */
void liferea_node_source_activatable_item_set_flag (LifereaNodeSourceActivatable *activatable, nodePtr node, itemPtr item, gboolean newState);

/**
 * liferea_node_source_activatable_convert_to_local:
 * @activatable:	the node source
 * @node:		the node source root node
 *
 * Converts all subscriptions to default source subscriptions.
 */
void liferea_node_source_activatable_convert_to_local (LifereaNodeSourceActivatable *activatable, nodePtr node);


G_END_DECLS

#endif /* __LIFEREA_NODE_SOURCE_ACTIVATABLE_H__ */
