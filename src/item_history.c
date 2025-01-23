/**
 * @file item_history.c tracking recently viewed items
 *
 * Copyright (C) 2012-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "item_history.h"

#include <glib.h>

#define MAX_HISTORY_SIZE	250

static ItemHistory *itemHistory = NULL;

G_DEFINE_TYPE (ItemHistory, item_history, G_TYPE_OBJECT)

enum {
	SIGNAL_CHANGED,
	LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = { 0 };

ItemHistory *
item_history_get_instance (void)
{
	return itemHistory;
}

static void
item_history_finalize (GObject *object)
{
	ItemHistory *self = ITEM_HISTORY (object);

	if (self->items)
		g_list_free (self->items);

	G_OBJECT_CLASS (item_history_parent_class)->finalize (object);
}

static void
item_history_class_init (ItemHistoryClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = item_history_finalize;

	signals[SIGNAL_CHANGED] =
		g_signal_new ("changed",
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
item_history_init (ItemHistory *self)
{
	g_assert (!itemHistory);
	itemHistory = self;

	self->items = NULL;
	self->current = NULL;
	self->lastId = 0;
}

void
item_history_add (guint id)
{
	/* Duplicate add by some selection effect */
	if (itemHistory->lastId == id)
		return;

	/* Duplicate add by history navigation */
	if (itemHistory->current && GPOINTER_TO_UINT (itemHistory->current->data) == id)
		return;

	itemHistory->items = g_list_append (itemHistory->items, GUINT_TO_POINTER (id));
	itemHistory->current = g_list_last (itemHistory->items);
	itemHistory->lastId = id;

	/* if list has reached max size remove first element */
	if (g_list_length (itemHistory->items) > MAX_HISTORY_SIZE)
		itemHistory->items = g_list_remove (itemHistory->items, itemHistory->items);

	g_signal_emit_by_name (itemHistory, "changed");
}

itemPtr
item_history_get_next (void)
{
	itemPtr item = NULL;

	if (!itemHistory->current)
		return NULL;

	while (!item && item_history_has_next ()) {
		if ((itemHistory->current = g_list_next (itemHistory->current)))
			item = item_load (GPOINTER_TO_UINT (itemHistory->current->data));
	}

	g_signal_emit_by_name (itemHistory, "changed");

	return item;
}

itemPtr
item_history_get_previous (void)
{
	itemPtr item = NULL;

	if (!itemHistory->current)
		return NULL;

	while (!item && item_history_has_previous ()) {
		if ((itemHistory->current = g_list_previous (itemHistory->current)))
			item = item_load (GPOINTER_TO_UINT (itemHistory->current->data));
	}

	g_signal_emit_by_name (itemHistory, "changed");

	return item;
}

gboolean
item_history_has_next (void)
{
	if (!itemHistory || !itemHistory->items)
		return FALSE;

	if (g_list_last (itemHistory->items) == itemHistory->current)
		return FALSE;

	return TRUE;
}

gboolean
item_history_has_previous (void)
{
	if (!itemHistory || !itemHistory->items)
		return FALSE;

	if (itemHistory->items == itemHistory->current)
		return FALSE;

	return TRUE;
}
