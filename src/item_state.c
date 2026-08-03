/**
 * @file item_state.c   item state controller
 * 
 * Copyright (C) 2007-2026 Lars Windolf <lars.windolf@gmx.de>
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
 
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "item.h"
#include "item_state.h"
#include "itemset.h"
#include "itemlist.h"
#include "node.h"
#include "node_providers/vfolder.h"
#include "node_source.h"

void
item_set_flag_state (itemPtr item, gboolean newState) 
{	
	if (newState == item->flagStatus)
		return;

	node_source_item_set_flag (node_from_id (item->nodeId), item, newState);

	/* no duplicate state propagation to avoid copies 
	   in the "Important" search folder */
}

void
item_flag_state_changed (itemPtr item, gboolean newState)
{
	/* 1. set value in memory */	
	item->flagStatus = newState;

	/* 2. save state to DB */
	db_item_state_update (item);

	/* 3. node and vfolder counters update */
	vfolder_foreach (node_update_counters);

	/* 4. update item list GUI state */
	itemlist_update_item (item);
}

void
item_set_read_state (itemPtr item, gboolean newState) 
{ 
	if (newState == item->readStatus)
		return;
	
	Node *node = node_from_id (item->nodeId);
	if (!node)
		return;

	node_source_item_mark_read (node, item, newState);
}

void
item_read_state_changed (itemPtr item, gboolean newState)
{
	Node *node;

	/* 1. set values in memory */	
	item->readStatus = newState;

	/* 2. apply to DB */
	db_item_state_update (item);

	/* 3. propagate to vfolders */
	vfolder_foreach (node_update_counters);
	
	/* 4. update item list GUI state */
	itemlist_update_item (item);

	/* 5. updated feed list unread counters */
	node = node_from_id (item->nodeId);
	node_update_counters (node);

	/* 6. duplicate state propagation (only for transition to read!) */
	if (item->validGuid && newState == TRUE) {
		GSList *duplicates, *iter;

		duplicates = iter = db_item_get_duplicates (item->sourceId);
		while (iter) {
			itemPtr duplicate = item_load (GPOINTER_TO_UINT (iter->data));

			/* The check on node_from_id() is an evil workaround
			   to handle "lost" items in the DB that have no 
			   associated node in the feed list. This should be 
			   fixed by having the feed list in the DB too, so
			   we can clean up correctly after crashes. */
			if (duplicate && duplicate->id != item->id && node_from_id (duplicate->nodeId)) {
				item_set_read_state (duplicate, newState);
			}
			if (duplicate)
				item_unload (duplicate);
			iter = g_slist_next (iter);
		}
		g_slist_free (duplicates);
	}

}

void
itemset_mark_read (Node *node)
{
	// FIXME: implement batch for e.g. >1000 items
	itemSetPtr itemSet = node_get_itemset (node);
	GList *iter = itemSet->ids;
	while (iter) {
		gulong id = GPOINTER_TO_UINT (iter->data);
		itemPtr item = item_load (id);
		if (item) {
			if (!item->readStatus)
				item_set_read_state (item, TRUE);
			item_unload (item);
		}
		iter = g_list_next (iter);
	}

	itemset_free (itemSet);
}
