/**
 * @file item_history.c tracking recently viewed items
 *
 * Copyright (C) 2012 Lars Windolf <lars.windolf@gmx.de>
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

#include "ui/liferea_shell.h"

#define MAX_HISTORY_SIZE	250

static struct itemHistory {
	GList	*items;		/**< FIFO list of all items viewed */
	GList	*current;	/**< the current list element */
	guint	lastId;		/**< Avoid duplicate add */
} *itemHistory = NULL;

void
item_history_add (guint id)
{
	if (!itemHistory)
		itemHistory = g_new0 (struct itemHistory, 1);

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

	liferea_shell_update_history_actions ();
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

	liferea_shell_update_history_actions ();

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

	liferea_shell_update_history_actions ();

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
