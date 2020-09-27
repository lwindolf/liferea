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
#include "subscription.h"
#include "update.h"

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

	/*
	 * Must return a human readable name identifying the node source
	 * type in a GUI dialog
	 */
	const gchar *	(*get_name) (LifereaNodeSourceActivatable *activatable);

	/*
	 * Must return a unique id identifying the node source type
	 * implemented by the plugin
	 */
	const gchar *	(*get_id) (LifereaNodeSourceActivatable *activatable);

	/*
	 * Must return a node source capabilities bitmask
	 */
	guint		(*get_capabilities) (LifereaNodeSourceActivatable *activatable);

	/* === Subscription type interface (see subscription_type.h!)*/

	/*
	 * MANDATORY feed subscription preparation callback.
	 */
	gboolean	(*feed_subscription_prepare_update_request)(LifereaNodeSourceActivatable *activatable, subscriptionPtr subscription, updateRequestPtr request);

	/*
	 * MANDATORY feed subscription type specific update result processing callback.
	 */
	void		(*feed_subscription_process_update_result)(LifereaNodeSourceActivatable *activatable, struct subscription * subscription, const struct updateResult * const result, updateFlags flags);

	/*
	 * MANDATORY source subscription update preparation callback.
	 */
	gboolean	(*feedlist_update_prepare)(LifereaNodeSourceActivatable *activatable, struct subscription * subscription, struct updateRequest * request);

	/*
	 * MANDATORY source subscription type specific update result processing callback.
	 */
	void		(*feedlist_update_cb)(LifereaNodeSourceActivatable *activatable, struct subscription * subscription, const struct updateResult * const result, updateFlags flags);

	/* === Feed list node handling interface (see node_source.h!) */

	/*
	 * This callback is used to create an instance
	 * of the implemented source type. It is to be called by
	 * the parent source node_request_add_*() implementation.
	 * MANDATORY for all sources except the root source.
	 */
	void 		(*new)(LifereaNodeSourceActivatable *activatable, const gchar *typeId);

	/*
	 * This callback is used to delete an instance
	 * of the implemented source type. It is to be called
	 * by the parent source node_remove() implementation.
	 * MANDATORY for all sources except the root provider source.
	 */
	void 		(*delete)(LifereaNodeSourceActivatable *activatable, nodePtr node);

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

void liferea_node_source_activatable_activate (LifereaNodeSourceActivatable * activatable);

void liferea_node_source_activatable_deactivate (LifereaNodeSourceActivatable * activatable);

G_END_DECLS

#endif /* __LIFEREA_NODE_SOURCE_ACTIVATABLE_H__ */
