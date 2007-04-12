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
 
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "itemlist.h"
#include "itemset.h"
#include "itemview.h"
#include "node.h"
#include "support.h"
#include "rule.h"
#include "vfolder.h"
#include "itemset.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"
#include "scripting/script.h"

/* This is a simple controller implementation for itemlist handling. 
   It manages the currently displayed itemset, realizes filtering
   and provides synchronisation for backend and GUI access to this 
   itemset.  

   Bypass only for read-only item access!   

   For the data structure the item list is assigned an item set by
   itemlist_load(), which can be updated with itemlist_merge_itemset().
   All items are filtered using the item list filter.
 */

static rulePtr itemlist_filter = NULL;		/* currently active filter rule */

static nodePtr	currentNode = NULL;		/* the node whose own our child items are currently displayed */
static gulong	selectedId = 0;			/* the currently selected (and displayed) item id */

static guint viewMode = 0;			/* current viewing mode */
static guint itemlistLoading = 0;		/* if >0 prevents selection effects when loading the item list */

static gboolean deferred_item_remove = FALSE;	/* TRUE if selected item needs to be removed from cache on unselecting */
static gboolean deferred_item_filter = FALSE;	/* TRUE if selected item needs to be filtered on unselecting */

itemPtr itemlist_get_selected(void) {

	return item_load(selectedId);
}

static void itemlist_set_selected(itemPtr item) {

	if(selectedId)
		script_run_for_hook(SCRIPT_HOOK_ITEM_UNSELECT);

	selectedId = item?item->id:0;
		
	if(selectedId)
		script_run_for_hook(SCRIPT_HOOK_ITEM_SELECTED);
}

nodePtr itemlist_get_displayed_node(void) { return currentNode; }

/* called when unselecting the item or unloading the item list */
static void itemlist_check_for_deferred_action(void) {
	itemPtr	item;
	
	if(selectedId) {
		gulong id = selectedId;
		itemlist_set_selected(NULL);

		/* check for removals caused by itemlist filter rule */
		if(deferred_item_filter) {
			deferred_item_filter = FALSE;
			item = item_load(id);
			itemview_remove_item(item);
			ui_node_update(item->nodeId);
			item_unload(item);
		}

		/* check for removals caused by vfolder rules */
		if(deferred_item_remove) {
			deferred_item_remove = FALSE;
			item = item_load(id);
			itemlist_remove_item(item);
			item_unload(item);
		}
	}
}

/**
 * To be called whenever an itemset was updated. If it is the
 * displayed itemset it will be merged against the item list
 * tree view.
 */
void itemlist_merge_itemset(itemSetPtr itemSet) {
	GList		*iter;
	gboolean	hasEnclosures = FALSE;
	nodePtr		node;

	debug_enter("itemlist_merge_itemset");
	
	debug_start_measurement (DEBUG_GUI);
	
	node = node_from_id(itemSet->nodeId);

	if(!currentNode)
		return; /* Nothing to do if nothing is displayed */
	
	if((currentNode != node) && !node_is_ancestor(currentNode, node))
		return; /* Nothing to do if the item set does not belong to this node */

	if((NODE_TYPE_FOLDER == currentNode->type) && 
	   (0 == getNumericConfValue(FOLDER_DISPLAY_MODE)))
		return; /* Bail out if it is a folder without the recursive display preference set */
		
	debug1(DEBUG_GUI, "reloading item list with node \"%s\"", node_get_title(node));

	/* update item list tree view */	
	iter = g_list_last(itemSet->ids);
	while(iter) {
		itemPtr item = item_load(GPOINTER_TO_UINT(iter->data));
		if(!(itemlist_filter && !rule_check_item(itemlist_filter, item))) {
			hasEnclosures |= item->hasEnclosure;
			itemview_add_item(item);
		}
		item_unload(item);
		iter = g_list_previous(iter);
	}
	
	if(hasEnclosures)
		ui_itemlist_enable_encicon_column(TRUE);

	itemview_update();
	
	debug_end_measurement (DEBUG_GUI, "itemlist merge");

	debug_exit("itemlist_merge_itemset");
}

/** 
 * To be called whenever a feed was selected and should
 * replace the current itemlist.
 */
void itemlist_load(nodePtr node) {
	itemSetPtr	itemSet;

	debug_enter("itemlist_load");

	g_assert(NULL != node);

	debug1(DEBUG_GUI, "loading item list with node \"%s\"", node_get_title(node));

	/* 1. Filter check. Don't continue if folder is selected and 
	   no folder viewing is configured. If folder viewing is enabled
	   set up a "unread items only" rule depending on the prefences. */	
	   
	g_free(itemlist_filter);
	itemlist_filter = NULL;
	   
	if(NODE_TYPE_FOLDER == node->type) {
		if(0 == getNumericConfValue(FOLDER_DISPLAY_MODE))
			return;
	
		if(getBooleanConfValue(FOLDER_DISPLAY_HIDE_READ))
			itemlist_filter = rule_new(NULL, "unread", "", TRUE);		
	}

	itemlistLoading++;
	viewMode = node_get_view_mode(node);
	ui_mainwindow_set_layout(viewMode);

	/* Set the new displayed node... */
	currentNode = node;
	itemview_set_displayed_node(currentNode);

	if(NODE_VIEW_MODE_COMBINED != node_get_view_mode(node))
		itemview_set_mode(ITEMVIEW_NODE_INFO);
	else
		itemview_set_mode(ITEMVIEW_ALL_ITEMS);
	
	itemSet = node_get_itemset(currentNode);
	itemlist_merge_itemset(itemSet);
	itemset_free(itemSet);

	itemlistLoading--;

	debug_exit("itemlist_load");
}

void itemlist_unload(gboolean markRead) {

	if(currentNode) {
		itemview_clear();
		itemview_set_displayed_node(NULL);
		
		/* 1. Postprocessing for previously selected node, this is necessary
		   to realize reliable read marking when using condensed mode. It's
		   important to do this only when the selection really changed. */
		if(markRead && (2 == node_get_view_mode(currentNode))) 
			itemlist_mark_all_read(currentNode->id);

		itemlist_check_for_deferred_action();
	}

	itemlist_set_selected(NULL);
	currentNode = NULL;
}

/* next unread selection logic */

static itemPtr itemlist_find_unread_item(void) {
	itemPtr	result = NULL;
	
	if(!currentNode)
		return NULL;

	if(currentNode->children) {
		feedlist_find_unread_feed(currentNode);
		return NULL;
	}

	/* Note: to select in sorting order we need to do it in the GUI code
	   otherwise we would have to sort the item list here... */
	
	if(selectedId)
		result = ui_itemlist_find_unread_item(selectedId);
		
	if(!result)
		result = ui_itemlist_find_unread_item(0);

	return result;
}

void itemlist_select_next_unread(void) {
	itemPtr	result = NULL;

	/* If we are in combined mode we have to mark everything
	   read or else we would never jump to the next feed,
	   because no item will be selected and marked read... */
	if(currentNode) {
		if(NODE_VIEW_MODE_COMBINED == node_get_view_mode(currentNode))
			itemlist_mark_all_read(currentNode->id);
	}

	itemlistLoading++;	/* prevent unwanted selections */

	/* before scanning the feed list, we test if there is a unread 
	   item in the currently selected feed! */
	result = itemlist_find_unread_item();
	
	/* If none is found we continue searching in the feed list */
	if(!result) {
		nodePtr	node;
		
		/* scan feed list and find first feed with unread items */
		node = feedlist_find_unread_feed(feedlist_get_root());

		if(node) {		
			/* load found feed */
			ui_feedlist_select(node);

			if(NODE_VIEW_MODE_COMBINED != node_get_view_mode(node))
				result = itemlist_find_unread_item();	/* find first unread item */
		} else {
			/* if we don't find a feed with unread items do nothing */
			ui_mainwindow_set_status_bar(_("There are no unread items "));
		}
	}

	itemlistLoading--;
	
	if(result)
		itemlist_selection_changed(result);
}

/* menu commands */
void itemlist_set_flag(itemPtr item, gboolean newStatus) {
	
	if(newStatus != item->flagStatus) {

		/* 1. save state to DB */
		item->flagStatus = newStatus;
		db_item_update(item);

		/* 2. update item list GUI state */
		itemlist_update_item(item);

		/* 3. no update of feed list necessary... */

		/* 4. update notification statistics */
		feedlist_reset_new_item_count();		
		
		/* no duplicate state propagation to avoid copies 
		   in the "Important" search folder */
	}
}
	
void itemlist_toggle_flag(itemPtr item) {

	itemlist_set_flag(item, !(item->flagStatus));
	itemview_update();
}

void itemlist_set_read_status(itemPtr item, gboolean newStatus) {

	if(newStatus != item->readStatus) {		
		debug_start_measurement (DEBUG_GUI);
		
		/* 1. save state to DB */
		item->readStatus = newStatus;
		db_item_update(item);

		/* 2. update item list GUI state */
		itemlist_update_item(item);

		/* 3. updated feed list unread counters */
		node_update_counters(node_from_id(item->nodeId));
		ui_node_update(item->nodeId);

		/* 4. update notification statistics */
		feedlist_reset_new_item_count();

		/* 5. duplicate state propagation */
		// FIXME!
		
		debug_end_measurement (DEBUG_GUI, "set read status");
	}
}

void itemlist_toggle_read_status(itemPtr item) {

	itemlist_set_read_status(item, !(item->readStatus));
	itemview_update();
}

void itemlist_set_update_status(itemPtr item, const gboolean newStatus) { 
	
	if(newStatus != item->updateStatus) {	

		/* 1. save state to DB */
		item->updateStatus = newStatus;
		db_item_update(item);

		/* 2. update item list state */
		itemlist_update_item(item);	

		/* 3. update notification statistics */
		feedlist_reset_new_item_count();
	
		/* no duplicate state propagation necessary */
	}
}

/* function to remove items due to item list filtering */
static void itemlist_hide_item(itemPtr item) {

	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if(selectedId != item->id) {
		itemview_remove_item(item);
		ui_node_update(item->nodeId);
	} else {
		deferred_item_filter = TRUE;
		/* update the item to show new state that forces
		   later removal */
		itemview_update_item(item);
	}
}

/* functions to remove items on remove requests */

void itemlist_remove_item(itemPtr item) {

	if(selectedId == item->id) {
		itemlist_set_selected(NULL);
		deferred_item_filter = FALSE;
		deferred_item_remove = FALSE;
		itemview_select_item(NULL);
	}
	
	itemview_remove_item(item);
	itemview_update();
	
	db_item_remove(item->id);
	
	node_update_counters(node_from_id(item->nodeId));
	ui_node_update(item->nodeId);
	
	item_unload(item);
}

void itemlist_request_remove_item(itemPtr item) {
	
	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if(selectedId != item->id) {
		itemlist_remove_item(item);
	} else {
		deferred_item_remove = TRUE;
		/* update the item to show new state that forces
		   later removal */
		itemview_update_item(item);
	}
}

void itemlist_remove_items(itemSetPtr itemSet, GList *items) {
	GList	*iter = items;
	
	while(iter) {
		itemview_remove_item(iter->data);
		iter = g_list_next(iter);
	}

	itemview_update();
	ui_node_update(itemSet->nodeId);
}

void itemlist_remove_all_items(nodePtr node) {

	itemview_clear();
	db_itemset_remove_all(node->id);
	itemview_update();
	ui_node_update(node->id);
}

void itemlist_update_item(itemPtr item) {

	if(itemlist_filter && !rule_check_item(itemlist_filter, item)) {
		itemlist_hide_item(item);
		return;
	}
	
	itemview_update_item(item);
}

void itemlist_mark_all_read(const gchar *nodeId) {

	db_itemset_mark_all_read(nodeId);
	
	/* GUI updating */	
	itemview_update_all_items();
	itemview_update();
	node_update_counters(node_from_id(nodeId));
	ui_node_update(nodeId);
}

void itemlist_mark_all_old(const gchar *nodeId) {

	db_itemset_mark_all_old(nodeId);
	
	/* No GUI updating necessary... */
}

void itemlist_mark_all_popup(const gchar *nodeId) {

	db_itemset_mark_all_popup(nodeId);
	
	/* No GUI updating necessary... */
}

/* mouse/keyboard interaction callbacks */
void 
itemlist_selection_changed (itemPtr item) {

	debug_enter ("itemlist_selection_changed");
	debug_start_measurement (DEBUG_GUI);

	if (0 == itemlistLoading)
	{
		/* folder&vfolder postprocessing to remove/filter unselected items no
		   more matching the display rules because they have changed state */
		itemlist_check_for_deferred_action ();
		
		selectedId = 0;

		debug1 (DEBUG_GUI, "item list selection changed to \"%s\"", item_get_title (item));
		
		itemlist_set_selected (item);
	
		/* set read and unset update status when selecting */
		if (item)
		{
			item_comments_refresh (item);

			itemlist_set_read_status (item, TRUE);
			itemlist_set_update_status (item, FALSE);
			
			if(node_load_link_preferred (node_from_id (item->nodeId))) 
			{
				ui_htmlview_launch_URL (ui_mainwindow_get_active_htmlview (), 
				                        item_get_source (itemlist_get_selected ()), UI_HTMLVIEW_LAUNCH_INTERNAL);
			} 
			else 
			{
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

guint itemlist_get_view_mode(void) { return viewMode; }

void itemlist_set_view_mode(guint newMode) { 
	nodePtr		node;

	viewMode = newMode;
	
	node = itemlist_get_displayed_node();
	if(node) {
		itemlist_unload(FALSE);
		
		node_set_view_mode(node, viewMode);
		ui_mainwindow_set_layout(viewMode);
		itemlist_load(node);
	}
}

void on_normal_view_activate(GtkToggleAction *menuitem, gpointer user_data) {
	itemlist_set_view_mode(0);
}

void on_wide_view_activate(GtkToggleAction *menuitem, gpointer user_data) {
	itemlist_set_view_mode(1);
}

void on_combined_view_activate(GtkToggleAction *menuitem, gpointer user_data) {
	itemlist_set_view_mode(2);
}
