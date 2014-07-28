/**
 * @file item_loader.h   Interface for asynchronous item loading
 *
 * Copyright (C) 2011 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _ITEM_LOADER_H
#define _ITEM_LOADER_H

#include <glib-object.h>

#include "node.h"

/* ItemLoader concept: an ItemLoader instance runs a fetch callback
   repeatedly collecting the items the fetch callback provides. One
   each fetch when there were items the loader emits a callback with
   the itemset as parameter for an item view to present. */

typedef struct ItemLoaderPrivate	ItemLoaderPrivate;

typedef struct ItemLoader {
	GObject parent;

	/*< private >*/
	ItemLoaderPrivate	*priv;
} ItemLoader;

typedef struct ItemLoaderClass {
	GObjectClass parent;
} ItemLoaderClass;

GType item_loader_get_type (void);

#define ITEM_LOADER_TYPE              (item_loader_get_type ())
#define ITEM_LOADER(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), ITEM_LOADER_TYPE, ItemLoader))
#define ITEM_LOADER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), ITEM_LOADER_TYPE, ItemLoaderClass))
#define IS_ITEM_LOADER(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), ITEM_LOADER_TYPE))
#define IS_ITEM_LOADER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), ITEM_LOADER_TYPE))
#define ITEM_LOADER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), ITEM_LOADER_TYPE, ItemLoaderClass))

/**
 * Definition of item loader fetch callback to be 
 * implemented by specific item loaders. This callback
 * is called multiple times to fetch item batches. The
 * batch size is determined by the specific implementation.
 * 
 * @param user_data	ItemLoader type specific data
 * @param items		Result items (to be free'd by caller)
 * 
 * @returns FALSE if loading has finished
 */
typedef gboolean (*fetchCallbackPtr)(gpointer user_data, GSList **items);

/**
 * Set up a new item loader with a specific fetch function.
 *
 * @param fetchCallback	the item fetching function
 * @param node		the node we are loading items for
 * @param user_data	ItemLoader type specific data
 *
 * @returns the new ItemLoader instance
 */
ItemLoader * item_loader_new (fetchCallbackPtr fetchCallback, nodePtr node, gpointer user_data);

/**
 * Returns the node an item loader is loading items for.
 *
 * @param il		the item loader
 *
 * @returns node
 */
nodePtr item_loader_get_node (ItemLoader *il);

/**
 * Starts the item loader to load items with idle priority.
 *
 * @param il	the item loader
 */
void item_loader_start (ItemLoader *il);

#endif
