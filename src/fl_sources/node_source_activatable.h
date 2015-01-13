/*
 * @file node_source_activatable.h  Node Source Plugin Type
 *
 * Copyright (C) 2015 Lars Windolf <lars.windolf@gmx.de>
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
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE		(liferea_node_source_activatable_get_type ())
#define LIFEREA_NODE_SOURCE_ACTIVATABLE(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE, LifereaNodeSourceActivatable))
#define LIFEREA_NODE_SOURCE_ACTIVATABLE_IFACE(obj)	(G_TYPE_CHECK_CLASS_CAST ((obj), LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE, LifereaNodeSourceActivatableInterface))
#define IS_LIFEREA_NODE_SOURCE_ACTIVATABLE(obj)	(G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE))
#define LIFEREA_NODE_SOURCE_ACTIVATABLE_GET_IFACE(obj)	(G_TYPE_INSTANCE_GET_INTERFACE ((obj), LIFEREA_NODE_SOURCE_ACTIVATABLE_TYPE, LifereaNodeSourceActivatableInterface))

typedef struct _LifereaNodeSourceActivatable LifereaNodeSourceActivatable;
typedef struct _LifereaNodeSourceActivatableInterface LifereaNodeSourceActivatableInterface;

struct _LifereaNodeSourceActivatableInterface
{
	GTypeInterface g_iface;

	void (*activate) (LifereaNodeSourceActivatable * activatable);
	void (*deactivate) (LifereaNodeSourceActivatable * activatable);
};

GType liferea_node_source_activatable_get_type (void) G_GNUC_CONST;

void liferea_node_source_activatable_activate (LifereaNodeSourceActivatable *activatable);

void liferea_node_source_activatable_deactivate (LifereaNodeSourceActivatable *activatable);

G_END_DECLS

#endif /* __LIFEREA_NODE_SOURCE_ACTIVATABLE_H__ */
