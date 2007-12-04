/**
 * @file item_state.c   item state handling
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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
#include "item.h"
#include "item_state.h"
#include "itemset.h"
#include "itemlist.h"
#include "newsbin.h"
#include "node.h"
#include "vfolder.h"

static void
item_state_set_recount_flag (nodePtr node)
{
	node->needsRecount = TRUE;
}

void
item_state_set_flagged (itemPtr item, gboolean newStatus) 
{	
	if (newStatus != item->flagStatus) {

		/* 1. No propagation because we recount search folders in step 3... */
		
		/* 2. save state to DB */
		item->flagStatus = newStatus;
		db_item_update (item);

		/* 3. update item list GUI state */
		itemlist_update_item (item);

		/* 4. no update of feed list necessary... */
		vfolder_foreach (vfolder_update_counters);

		/* 5. update notification statistics */
		feedlist_reset_new_item_count ();
		
		/* no duplicate state propagation to avoid copies 
		   in the "Important" search folder */
	}
}

void
item_state_set_read (itemPtr item, gboolean newStatus) 
{
	if (newStatus != item->readStatus) {
		debug_start_measurement (DEBUG_GUI);
		
		/* 1. save state to DB */
		item->readStatus = newStatus;
		db_item_update (item);
		
		/* 2. add propagate to vfolders (must happen after changing the item state) */
		vfolder_foreach (vfolder_update_counters);
			
		/* 3. update item list GUI state */
		itemlist_update_item (item);

		/* 4. updated feed list unread counters */
		node_update_counters (node_from_id (item->nodeId));
		
		/* 5. update notification statistics */
		feedlist_reset_new_item_count ();

		/* 6. duplicate state propagation */
		if (item->validGuid) {
			GSList *duplicates, *iter;
			
			duplicates = iter = db_item_get_duplicates (item->sourceId);
			while (iter)
			{
				itemPtr duplicate = item_load (GPOINTER_TO_UINT (iter->data));
				
				/* The check on node_from_id() is an evil workaround
				   to handle "lost" items in the DB that have no 
				   associated node in the feed list. This should be 
				   fixed by having the feed list in the DB too, so
				   we can clean up correctly after crashes. */
				if (duplicate && node_from_id (duplicate->nodeId))
				{
					item_state_set_read (duplicate, newStatus);
					db_item_update (duplicate);
					item_unload (duplicate);
				}
				iter = g_slist_next (iter);
			}
			g_slist_free (duplicates);
		}
		
		debug_end_measurement (DEBUG_GUI, "set read status");
	}
}

void
item_state_set_updated (itemPtr item, const gboolean newStatus) 
{ 	
	if (newStatus != item->updateStatus) {	

		/* 1. save state to DB */
		item->updateStatus = newStatus;
		db_item_update (item);

		/* 2. update item list state */
		itemlist_update_item (item);

		/* 3. update notification statistics */
		feedlist_reset_new_item_count ();
	
		/* no duplicate state propagation necessary */
	}
}

/**
 * In difference to all the other item state handling methods
 * item_state_set_all_read does not immediately applies the 
 * changes to the GUI because it is usually called recursively
 * and would be to slow. Instead the node structure flag for
 * recounting is set. By calling feedlist_update() afterwards
 * those recounts are executed and applied to the GUI.
 */
void
item_state_set_all_read (nodePtr node) 
{
	itemSetPtr	itemSet;
	
	if (!node->unreadCount)
		return;
	
	itemSet = node_get_itemset (node);
	GList *iter = itemSet->ids;
	while (iter) {
		gulong id = GPOINTER_TO_UINT (iter->data);
		itemPtr item = item_load (id);
		if (item) {
			if (!item->readStatus) {
				db_item_mark_read (item);
				itemlist_update_item (item);
				node_from_id (item->nodeId)->needsRecount = TRUE;

				debug_start_measurement (DEBUG_GUI);

				GSList *duplicates = db_item_get_duplicate_nodes (item->sourceId);
				GSList *duplicate = duplicates;
				while (duplicate) {
					gchar *nodeId = (gchar *)duplicate->data;
					nodePtr affectedNode = node_from_id (nodeId);
					if (affectedNode != NULL) {
						affectedNode->needsRecount = TRUE;
					}
					g_free (nodeId);
					duplicate = g_slist_next (duplicate);
				}
				g_slist_free(duplicates);

				debug_end_measurement (DEBUG_GUI, "mark read of duplicates");
			}
			item_unload (item);
		}
		iter = g_list_next (iter);
	}
		
	/* we surely have changed the search folder states... */
	vfolder_foreach (item_state_set_recount_flag);
}

void
item_state_set_all_old (const gchar *nodeId)
{
	db_itemset_mark_all_old (nodeId);
	
	/* No GUI updating necessary... */
}

void
item_state_set_all_popup (const gchar *nodeId)
{
	db_itemset_mark_all_popup (nodeId);
	
	/* No GUI updating necessary... */
}
