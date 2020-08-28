/**
 * @file item_loader.c   Asynchronously loading items
 *
 * Copyright (C) 2011-2020 Lars Windolf <lars.windolf@gmx.de>
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

#include "item_loader.h"

#define ITEM_LOADER_GET_PRIVATE item_loader_get_instance_private

struct ItemLoaderPrivate {
	fetchCallbackPtr	fetchCallback;		/**< the function to call after each item fetch */
	gpointer		fetchCallbackData;	/**< user data for the fetch callback */

	nodePtr		node;			/**< the node we are loading items for */

	guint		idleId;			/**< fetch callback source id */
};

enum {
	ITEM_BATCH_FETCHED,
	FINISHED,
	LAST_SIGNAL
};

static guint item_loader_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

G_DEFINE_TYPE_WITH_CODE (ItemLoader, item_loader, G_TYPE_OBJECT, G_ADD_PRIVATE (ItemLoader));

static void
item_loader_finalize (GObject *object)
{
	ItemLoader *il = ITEM_LOADER (object);

	if (il->priv->idleId) {
		g_source_remove (il->priv->idleId);
		il->priv->idleId = 0;
	}

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
item_loader_class_init (ItemLoaderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = item_loader_finalize;

	item_loader_signals[ITEM_BATCH_FETCHED] =
		g_signal_new ("item-batch-fetched",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE,
		1,
		G_TYPE_POINTER);

	item_loader_signals[FINISHED] =
		g_signal_new ("finished",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0);
}

static void
item_loader_init (ItemLoader *il)
{
	il->priv = ITEM_LOADER_GET_PRIVATE (il);
}

nodePtr
item_loader_get_node (ItemLoader *il)
{
	return il->priv->node;
}

static gboolean
item_loader_fetch (gpointer user_data)
{
	ItemLoader	*il = ITEM_LOADER (user_data);
	GSList		*resultItems = NULL;
	gboolean	result;

	result = (*il->priv->fetchCallback)(il->priv->fetchCallbackData, &resultItems);
	if (result)
		g_signal_emit_by_name (il, "item-batch-fetched", resultItems);
	else {
		il->priv->idleId = 0;
		g_signal_emit_by_name (il, "finished");
	}

	return result;
}

void
item_loader_start (ItemLoader *il)
{
	il->priv->idleId = g_idle_add (item_loader_fetch, il);
}

ItemLoader *
item_loader_new (fetchCallbackPtr fetchCallback, nodePtr node, gpointer fetchCallbackData)
{
	ItemLoader *il;

	il = ITEM_LOADER (g_object_new (ITEM_LOADER_TYPE, NULL));
	il->priv->node = node;
	il->priv->fetchCallback = fetchCallback;
	il->priv->fetchCallbackData = fetchCallbackData;

	return il;
}
