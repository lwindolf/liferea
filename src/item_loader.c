/**
 * @file item_loader.c   Asynchronously loading items
 *
 * Copyright (C) 2011 Lars Lindner <lars.lindner@gmail.com>
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

static void item_loader_class_init	(ItemLoaderClass *klass);
static void item_loader_init		(ItemLoader *il);

#define ITEM_LOADER_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), ITEM_LOADER_TYPE, ItemLoaderPrivate))

struct ItemLoaderPrivate {
	fetchCallbackPtr	fetchCallback;		/**< the function to call after each item fetch */
	gpointer		fetchCallbackData;	/**< user data for the fetch callback */

	guint		idleId;			/**< fetch callback source id */
};

enum {
	ITEM_BATCH_FETCHED,
	LAST_SIGNAL
};

static guint item_loader_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;

GType
item_loader_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (ItemLoaderClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) item_loader_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (ItemLoader),
			0, /* n_preallocs */
			(GInstanceInitFunc) item_loader_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "ItemLoader",
					       &our_info, 0);
	}

	return type;
}

static void
item_loader_finalize (GObject *object)
{
	ItemLoader *il = ITEM_LOADER (object);

	if (il->priv->idleId)
		g_source_remove (il->priv->idleId);

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


	g_type_class_add_private (object_class, sizeof(ItemLoaderPrivate));
}

static void
item_loader_init (ItemLoader *il)
{
	il->priv = ITEM_LOADER_GET_PRIVATE (il);
}

gboolean
item_loader_fetch (gpointer user_data)
{
	ItemLoader	*il = ITEM_LOADER (user_data);
	GSList		*resultItems = NULL;
	gboolean	result;

	result = (*il->priv->fetchCallback)(il->priv->fetchCallbackData, &resultItems);
	if (result)
		g_signal_emit_by_name (il, "item-batch-fetched", resultItems);

	return result;
}

void
item_loader_start (ItemLoader *il) 
{
	il->priv->idleId = g_idle_add (item_loader_fetch, il);
}

ItemLoader *
item_loader_new (fetchCallbackPtr fetchCallback, gpointer fetchCallbackData)
{
	ItemLoader *il;

	il = ITEM_LOADER (g_object_new (ITEM_LOADER_TYPE, NULL));
	il->priv->fetchCallback = fetchCallback;
	il->priv->fetchCallbackData = fetchCallbackData;

	return il;
}
