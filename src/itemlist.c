/**
 * @file itemlist.c itemlist handling
 *
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
 
#include "comments.h"
#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "itemlist.h"
#include "itemset.h"
#include "itemview.h"
#include "node.h"
#include "rule.h"
#include "script.h"
#include "vfolder.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"

/* This is a simple controller implementation for itemlist handling. 
   It manages the currently displayed itemset, realizes filtering
   and provides synchronisation for backend and GUI access to this 
   itemset.  
 */
 
static struct itemlist_priv 
{
	GSList	 	*filter;		/**< currently active filter rule */
	nodePtr		currentNode;		/**< the node whose own or its child items are currently displayed */
	gulong		selectedId;		/**< the currently selected (and displayed) item id */
	gboolean	hasEnclosures;		/**< TRUE if at least one item of the current itemset has an enclosure */
	
	guint 		viewMode;		/**< current viewing mode */
	guint 		loading;		/**< if >0 prevents selection effects when loading the item list */

	gboolean 	deferredRemove;		/**< TRUE if selected item needs to be removed from cache on unselecting */
	gboolean 	deferredFilter;		/**< TRUE if selected item needs to be filtered on unselecting */
} itemlist_priv;

void
itemlist_free (void)
{
	ui_itemlist_destroy (); 
	
	// FIXME: free filter rules
	g_slist_free (itemlist_priv.filter);
}

itemPtr
itemlist_get_selected (void)
{
	return item_load(itemlist_priv.selectedId);
}

static void
itemlist_set_selected (itemPtr item)
{
	if(itemlist_priv.selectedId)
		script_run_for_hook(SCRIPT_HOOK_ITEM_UNSELECT);

	itemlist_priv.selectedId = item?item->id:0;
		
	if(itemlist_priv.selectedId)
		script_run_for_hook(SCRIPT_HOOK_ITEM_SELECTED);
}

nodePtr
itemlist_get_displayed_node (void)
{
	return itemlist_priv.currentNode; 
}

/* called when unselecting the item or unloading the item list */
static void
itemlist_check_for_deferred_action (void) 
{
	itemPtr	item;
	
	if(itemlist_priv.selectedId) {
		gulong id = itemlist_priv.selectedId;
		itemlist_set_selected(NULL);

		/* check for removals caused by itemlist filter rule */
		if(itemlist_priv.deferredFilter) {
			itemlist_priv.deferredFilter = FALSE;
			item = item_load(id);
			itemview_remove_item(item);
			ui_node_update(item->nodeId);
		}

		/* check for removals caused by vfolder rules */
		if(itemlist_priv.deferredRemove) {
			itemlist_priv.deferredRemove = FALSE;
			item = item_load(id);
			itemlist_remove_item(item);
		}
	}
}

// FIXME: is this an item set method?
static gboolean
itemlist_check_item (itemPtr item)
{
	/* use search folder rule list in case of a search folder */
	if (itemlist_priv.currentNode->type == NODE_TYPE_VFOLDER)		
		return rules_check_item (((vfolderPtr)itemlist_priv.currentNode->data)->rules, item);

	/* apply the item list filter if available */
	if (itemlist_priv.filter)
		return rules_check_item (itemlist_priv.filter, item);
	
	/* otherwise keep the item */
	return TRUE;
}

static void
itemlist_merge_item (itemPtr item) 
{
	if (itemlist_check_item (item)) {
		itemlist_priv.hasEnclosures |= item->hasEnclosure;
		itemview_add_item (item);
	}
}

/**
 * To be called whenever an itemset was updated. If it is the
 * displayed itemset it will be merged against the item list
 * tree view.
 */
void
itemlist_merge_itemset (itemSetPtr itemSet) 
{
	nodePtr		node;

	debug_enter ("itemlist_merge_itemset");
	
	debug_start_measurement (DEBUG_GUI);
	
	node = node_from_id(itemSet->nodeId);

	if(!itemlist_priv.currentNode)
		return; /* Nothing to do if nothing is displayed */
	
	if((NODE_TYPE_VFOLDER != itemlist_priv.currentNode->type) &&
	   (itemlist_priv.currentNode != node) && 
	   !node_is_ancestor (itemlist_priv.currentNode, node))
		return; /* Nothing to do if the item set does not belong to this node, or this is a search folder */

	if((NODE_TYPE_FOLDER == itemlist_priv.currentNode->type) && 
	   (0 == conf_get_int_value (FOLDER_DISPLAY_MODE)))
		return; /* Bail out if it is a folder without the recursive display preference set */
		
	debug1 (DEBUG_GUI, "reloading item list with node \"%s\"", node_get_title (node));

	/* merge items into item view */
	itemset_foreach (itemSet, itemlist_merge_item);
	
	if (itemlist_priv.hasEnclosures)
		ui_itemlist_enable_encicon_column (TRUE);

	itemview_update ();
	
	debug_end_measurement (DEBUG_GUI, "itemlist merge");

	debug_exit ("itemlist_merge_itemset");
}

/** 
 * To be called whenever a feed was selected and should
 * replace the current itemlist.
 */
void
itemlist_load (nodePtr node) 
{
	itemSetPtr	itemSet;
	GSList		*iter;

	debug_enter ("itemlist_load");

	g_return_if_fail (NULL != node);

	debug1 (DEBUG_GUI, "loading item list with node \"%s\"", node_get_title (node));

	/* 1. Filter check. Don't continue if folder is selected and 
	   no folder viewing is configured. If folder viewing is enabled
	   set up a "unread items only" rule depending on the prefences. */	
	   
	iter = itemlist_priv.filter;
	while (iter) {
		rule_free (iter->data);
		iter = g_slist_next (iter);
	}
	g_slist_free (itemlist_priv.filter);
	itemlist_priv.filter = NULL;
	
	itemlist_priv.hasEnclosures = FALSE;
	   
	/* for folders and other heirarchic nodes do filtering */
	if ((NODE_TYPE_FOLDER == node->type) || node->children) {
		if (0 == conf_get_int_value (FOLDER_DISPLAY_MODE))
			return;
	
		if (conf_get_bool_value (FOLDER_DISPLAY_HIDE_READ))
			itemlist_priv.filter = g_slist_append (NULL, rule_new (NULL, "unread", "", TRUE));
	}

	itemlist_priv.loading++;
	itemlist_priv.viewMode = node_get_view_mode (node);
	ui_mainwindow_set_layout (itemlist_priv.viewMode);

	/* Set the new displayed node... */
	itemlist_priv.currentNode = node;
	itemview_set_displayed_node (itemlist_priv.currentNode);

	if(NODE_VIEW_MODE_COMBINED != node_get_view_mode (node))
		itemview_set_mode (ITEMVIEW_NODE_INFO);
	else
		itemview_set_mode (ITEMVIEW_ALL_ITEMS);
	
	itemSet = node_get_itemset(itemlist_priv.currentNode);
	itemlist_merge_itemset(itemSet);
	itemset_free(itemSet);

	itemlist_priv.loading--;

	debug_exit("itemlist_load");
}

void
itemlist_unload (gboolean markRead) 
{
	if (itemlist_priv.currentNode) {
		itemview_clear ();
		itemview_set_displayed_node (NULL);
		
		/* 1. Postprocessing for previously selected node, this is necessary
		   to realize reliable read marking when using condensed mode. It's
		   important to do this only when the selection really changed. */
		if(markRead && (2 == node_get_view_mode (itemlist_priv.currentNode))) 
			itemlist_mark_all_read (itemlist_priv.currentNode->id);

		itemlist_check_for_deferred_action();
	}

	itemlist_set_selected(NULL);
	itemlist_priv.currentNode = NULL;
}

/* next unread selection logic */

static itemPtr
itemlist_find_unread_item (void) 
{
	itemPtr	result = NULL;
	
	if (!itemlist_priv.currentNode)
		return NULL;

	if (itemlist_priv.currentNode->children) {
		feedlist_find_unread_feed (itemlist_priv.currentNode);
		return NULL;
	}

	/* Note: to select in sorting order we need to do it in the GUI code
	   otherwise we would have to sort the item list here... */
	
	if (itemlist_priv.selectedId)
		result = ui_itemlist_find_unread_item (itemlist_priv.selectedId);
		
	if (!result)
		result = ui_itemlist_find_unread_item (0);

	return result;
}

void
itemlist_select_next_unread (void) 
{
	itemPtr	result = NULL;

	/* If we are in combined mode we have to mark everything
	   read or else we would never jump to the next feed,
	   because no item will be selected and marked read... */
	if (itemlist_priv.currentNode) {
		if (NODE_VIEW_MODE_COMBINED == node_get_view_mode (itemlist_priv.currentNode))
			itemlist_mark_all_read (itemlist_priv.currentNode->id);
	}

	itemlist_priv.loading++;	/* prevent unwanted selections */

	/* before scanning the feed list, we test if there is a unread 
	   item in the currently selected feed! */
	result = itemlist_find_unread_item ();
	
	/* If none is found we continue searching in the feed list */
	if (!result) {
		nodePtr	node;

		/* scan feed list and find first feed with unread items */
		node = feedlist_find_unread_feed (feedlist_get_root ());
		if (node) {
			/* load found feed */
			ui_feedlist_select (node);

			if (NODE_VIEW_MODE_COMBINED != node_get_view_mode (node))
				result = itemlist_find_unread_item ();	/* find first unread item */
		} else {
			/* if we don't find a feed with unread items do nothing */
			ui_mainwindow_set_status_bar (_("There are no unread items "));
		}
	}

	itemlist_priv.loading--;
	
	if (result)
		itemlist_selection_changed (result);
}

/* menu commands */

static void
itemlist_decrement_vfolder_count (nodePtr node)
{
	if (node->itemCount > 0)
		node->itemCount--;
	else
		node->itemCount = 0;
	ui_node_update (node->id);
}

static void
itemlist_increment_vfolder_count (nodePtr node)
{
	node->itemCount++;
	ui_node_update (node->id);
}

void
itemlist_set_flag (itemPtr item, gboolean newStatus) 
{	
	if (newStatus != item->flagStatus) {

		/* 1. No search folder propagation... */
					 
		/* 2. save state to DB */
		item->flagStatus = newStatus;
		db_item_update (item);

		/* 3. update item list GUI state */
		itemlist_update_item (item);

		/* 4. no update of feed list necessary... */

		/* 5. update notification statistics */
		feedlist_reset_new_item_count ();
		
		/* no duplicate state propagation to avoid copies 
		   in the "Important" search folder */
	}
}
	
void
itemlist_toggle_flag (itemPtr item) 
{
	itemlist_set_flag (item, !(item->flagStatus));
	itemview_update ();
}

static void
itemlist_decrement_vfolder_unread (nodePtr node)
{
	node->unreadCount--;
	node->itemCount--;
	ui_node_update (node->id);
}

static void
itemlist_increment_vfolder_unread (nodePtr node)
{
	node->unreadCount++;
	node->itemCount++;
	ui_node_update (node->id);
}

void
itemlist_set_read_status (itemPtr item, gboolean newStatus) 
{
	if (newStatus != item->readStatus) {
		debug_start_measurement (DEBUG_GUI);

		/* 1. remove propagate to vfolders (must happen before changing the item state) */
		if (newStatus)
			vfolder_foreach_with_item (item->id, itemlist_decrement_vfolder_unread);
				
		/* 2. save state to DB */
		item->readStatus = newStatus;
		db_item_update (item);
		
		/* 3. add propagate to vfolders (must happen after changing the item state) */
		if (!newStatus)
			vfolder_foreach_with_item (item->id, itemlist_increment_vfolder_unread);
			
		/* 4. update item list GUI state */
		itemlist_update_item (item);

		/* 5. updated feed list unread counters */
		node_update_counters (node_from_id (item->nodeId));
		
		/* 6. update notification statistics */
		feedlist_reset_new_item_count ();

		/* 7. duplicate state propagation */
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
					itemlist_set_read_status (duplicate, newStatus);
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
itemlist_toggle_read_status (itemPtr item) 
{
	itemlist_set_read_status (item, !(item->readStatus));
	itemview_update ();
}

void
itemlist_set_update_status (itemPtr item, const gboolean newStatus) 
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

/* function to remove items due to item list filtering */
static void
itemlist_hide_item (itemPtr item)
{
	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if (itemlist_priv.selectedId != item->id) {
		itemview_remove_item (item);
		ui_node_update (item->nodeId);
	} else {
		itemlist_priv.deferredFilter = TRUE;
		/* update the item to show new state that forces
		   later removal */
		itemview_update_item (item);
	}
}

/* functions to remove items on remove requests */

void
itemlist_remove_item (itemPtr item) 
{
	/* update search folder counters */
	if (!item->readStatus)
		vfolder_foreach_with_item (item->id, itemlist_decrement_vfolder_unread);

	if (itemlist_priv.selectedId == item->id) {
		itemlist_set_selected (NULL);
		itemlist_priv.deferredFilter = FALSE;
		itemlist_priv.deferredRemove = FALSE;
		itemview_select_item (NULL);
	}
	
	itemview_remove_item (item);
	itemview_update ();

	db_item_remove (item->id);
	
	/* update feed list */
	node_update_counters (node_from_id (item->nodeId));
	
	item_unload (item);
}

void
itemlist_request_remove_item (itemPtr item) 
{	
	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if (itemlist_priv.selectedId != item->id) {
		itemlist_remove_item (item);
	} else {
		itemlist_priv.deferredRemove = TRUE;
		/* update the item to show new state that forces
		   later removal */
		itemview_update_item (item);
	}
}

void
itemlist_remove_items (itemSetPtr itemSet, GList *items)
{
	GList		*iter = items;
	gboolean	unread = FALSE, flagged = FALSE;
	
	while (iter) {
		itemPtr item = (itemPtr) iter->data;
		unread |= !item->readStatus;
		flagged |= item->flagStatus;
		if (itemlist_priv.selectedId != item->id) {
			/* don't call itemlist_remove_item() here, because it's to slow */
			itemview_remove_item (item);
			db_item_remove (item->id);
		} else {
			/* go the normal and selection-safe way to avoid disturbing the user */
			itemlist_request_remove_item (item);
		}
		item_unload (item);
		iter = g_list_next (iter);
	}

	itemview_update ();
	node_update_counters (node_from_id (itemSet->nodeId));
	vfolder_foreach (vfolder_update_counters);
}

void
itemlist_remove_all_items (nodePtr node)
{	
	if (node == itemlist_priv.currentNode)
		itemview_clear ();
		
	db_itemset_remove_all (node->id);
	
	if (node == itemlist_priv.currentNode)
		itemview_update ();
	
	/* Search folders updating */
	if (node->unreadCount)
		vfolder_foreach_with_rule ("unread", vfolder_update_counters);
	vfolder_foreach_with_rule ("flagged", vfolder_update_counters);
	
	node_update_counters (node);
}

void
itemlist_update_item (itemPtr item)
{
	if (!itemlist_check_item (item)) {
		itemlist_hide_item (item);
		return;
	}
	
	itemview_update_item (item);
}

static void
itemlist_update_node_counters (gpointer key, gpointer value, gpointer user_data)
{
	nodePtr node = (nodePtr)key;

	g_return_if_fail(node != NULL);
	node_update_counters (node);
}

void
itemlist_mark_all_read (const gchar *nodeId) 
{
	GHashTable	*affectedNodes = NULL;
	nodePtr		node;
	itemSetPtr	itemSet;
	
	node = node_from_id (nodeId);	
	itemSet = node_get_itemset (node);
	
	if (!itemSet || !node || !node->unreadCount)
		return;
		
	affectedNodes = g_hash_table_new (g_direct_hash, g_direct_equal);
		
	GList *iter = itemSet->ids;
	while (iter) {
		gulong id = GPOINTER_TO_UINT (iter->data);
		itemPtr item = item_load (id);
		if (!item->readStatus) {
			db_item_mark_read (item);
			itemlist_update_item (item);
			g_hash_table_insert (affectedNodes, node_from_id (item->nodeId), NULL);
			// FIXME: better provide get_duplicate_nodes() method
			GSList *iter = db_item_get_duplicates (item->sourceId);
			while (iter) {
				itemPtr item = item_load (GPOINTER_TO_UINT (iter->data));
				g_hash_table_insert (affectedNodes, node_from_id (item->nodeId), NULL);
				iter = g_slist_next (iter);
				itemlist_update_item (item);
				item_unload (item);
			}
		}
		item_unload (item);
		iter = g_list_next (iter);
	}
		
	/* GUI list updating */
	itemview_update_all_items ();
	itemview_update ();

	g_hash_table_foreach (affectedNodes, itemlist_update_node_counters, NULL);
	g_hash_table_destroy (affectedNodes);	
	
	vfolder_foreach (vfolder_update_counters);
}

void
itemlist_mark_all_old (const gchar *nodeId)
{
	db_itemset_mark_all_old (nodeId);
	
	/* No GUI updating necessary... */
}

void
itemlist_mark_all_popup (const gchar *nodeId)
{
	db_itemset_mark_all_popup (nodeId);
	
	/* No GUI updating necessary... */
}

/* mouse/keyboard interaction callbacks */
void 
itemlist_selection_changed (itemPtr item)
{
	debug_enter ("itemlist_selection_changed");
	debug_start_measurement (DEBUG_GUI);

	if (0 == itemlist_priv.loading)	{
		/* folder&vfolder postprocessing to remove/filter unselected items no
		   more matching the display rules because they have changed state */
		itemlist_check_for_deferred_action ();
		
		itemlist_priv.selectedId = 0;

		debug1 (DEBUG_GUI, "item list selection changed to \"%s\"", item_get_title (item));
		
		itemlist_set_selected (item);
	
		/* set read and unset update status when selecting */
		if (item) {
			comments_refresh (item);

			itemlist_set_read_status (item, TRUE);
			itemlist_set_update_status (item, FALSE);

			if(node_load_link_preferred (node_from_id (item->nodeId))) {
				ui_htmlview_launch_URL (ui_mainwindow_get_active_htmlview (), 
				                        item_get_source (itemlist_get_selected ()), UI_HTMLVIEW_LAUNCH_INTERNAL);
			} else {
				itemview_set_mode (ITEMVIEW_SINGLE_ITEM);
				itemview_select_item (item);
				itemview_update ();
			}
		}

		ui_node_update (item->nodeId);

		feedlist_reset_new_item_count ();
	}

	debug_end_measurement (DEBUG_GUI, "itemlist selection");
	debug_exit ("itemlist_selection_changed");
}

/* viewing mode callbacks */

guint
itemlist_get_view_mode (void)
{
	return itemlist_priv.viewMode; 
}

void
itemlist_set_view_mode (guint newMode) 
{ 
	nodePtr		node;

	itemlist_priv.viewMode = newMode;
	
	node = itemlist_get_displayed_node ();
	if (node) {
		itemlist_unload (FALSE);
		
		node_set_view_mode (node, itemlist_priv.viewMode);
		ui_mainwindow_set_layout (itemlist_priv.viewMode);
		itemlist_load (node);
	}
}

void
on_view_activate (GtkRadioAction *action, GtkRadioAction *current, gpointer user_data)
{
	gint val = gtk_radio_action_get_current_value (current);
	itemlist_set_view_mode (val);
}

