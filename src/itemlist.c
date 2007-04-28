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

itemSetPtr	displayed_itemSet = NULL;

/* internal item list states */

static rulePtr itemlist_filter = NULL;	/* currently active filter rule */

static itemPtr	displayed_item = NULL;	/* displayed item = selected item */

static guint viewMode = 0;		/* current viewing mode */
static guint itemlistLoading = 0;	/* if >0 prevents selection effects when loading the item list */

static gboolean deferred_item_remove = FALSE;	/* TRUE if selected item needs to be removed from cache on unselecting */
static gboolean deferred_item_filter = FALSE;	/* TRUE if selected item needs to be filtered on unselecting */

itemPtr itemlist_get_selected(void) {

	return displayed_item;
}

static void itemlist_set_selected(itemPtr item) {

	if(displayed_item)
		script_run_for_hook(SCRIPT_HOOK_ITEM_UNSELECT);

	displayed_item = item;
		
	if(displayed_item)
		script_run_for_hook(SCRIPT_HOOK_ITEM_SELECTED);
}

nodePtr itemlist_get_displayed_node(void) {

	if(displayed_itemSet)
		return displayed_itemSet->node;
	else
		return NULL;
}

itemSetPtr itemlist_get_displayed_itemset(void) { return displayed_itemSet; }

/* called when unselecting the item or unloading the item list */
static void itemlist_check_for_deferred_action(void) {
	itemPtr item;

	if(displayed_item) {
		item = displayed_item;
		itemlist_set_selected(NULL);

		/* check for removals caused by itemlist filter rule */
		if(deferred_item_filter) {
			deferred_item_filter = FALSE;
			itemview_remove_item(item);
			ui_node_update(item->sourceNode);
		}

		/* check for removals caused by vfolder rules */
		if(deferred_item_remove) {
			deferred_item_remove = FALSE;
			itemlist_remove_item(item);
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

	debug_enter("itemlist_merge_itemset");

	if(!displayed_itemSet)
		return; /* Nothing to do if nothing is displayed */
	
	if((displayed_itemSet != itemSet) && !node_is_ancestor(displayed_itemSet->node, itemSet->node))
		return; /* Nothing to do if the item set does not belong to this node */

	if((ITEMSET_TYPE_FOLDER == displayed_itemSet->type) && 
	   (0 == getNumericConfValue(FOLDER_DISPLAY_MODE)))
		return; /* Bail out if it is a folder without the recursive display preference set */
		
	debug1(DEBUG_GUI, "reloading item list with node \"%s\"", node_get_title(itemSet->node));

	/* update item list tree view */	
	iter = g_list_last(itemSet->items);
	while(iter) {
		itemPtr item = iter->data;
		if(!(itemlist_filter && !rule_check_item(itemlist_filter, item))) {
			hasEnclosures |= item->hasEnclosure;
			itemview_add_item(item);
		}

		iter = g_list_previous(iter);
	}
	
	if(hasEnclosures)
		ui_itemlist_enable_encicon_column(TRUE);

	itemview_update();

	debug_exit("itemlist_merge_itemset");
}

/** 
 * To be called whenever a feed was selected and should
 * replace the current itemlist.
 */
void itemlist_load(itemSetPtr itemSet) {

	debug_enter("itemlist_load");

	g_return_if_fail(NULL != itemSet);

	debug1(DEBUG_GUI, "loading item list with node \"%s\"", node_get_title(itemSet->node));

	/* 1. Filter check. Don't continue if folder is selected and 
	   no folder viewing is configured. If folder viewing is enabled
	   set up a "unread items only" rule depending on the prefences. */	
	   
	g_free(itemlist_filter);
	itemlist_filter = NULL;
	   
	if(ITEMSET_TYPE_FOLDER == itemSet->type) {
		if(0 == getNumericConfValue(FOLDER_DISPLAY_MODE))
			return;
	
		if(getBooleanConfValue(FOLDER_DISPLAY_HIDE_READ))
			itemlist_filter = rule_new(NULL, "unread", "", TRUE);		
	}

	itemlistLoading++;
	viewMode = node_get_view_mode(itemSet->node);
	ui_mainwindow_set_layout(viewMode);

	/* Set the new item set... */
	displayed_itemSet = itemSet;
	itemview_set_itemset(itemSet);

	if(NODE_VIEW_MODE_COMBINED != node_get_view_mode(itemSet->node))
		itemview_set_mode(ITEMVIEW_NODE_INFO);
	else
		itemview_set_mode(ITEMVIEW_ALL_ITEMS);
	
	itemlist_merge_itemset(itemSet);

	itemlistLoading--;

	debug_exit("itemlist_load");
}

void itemlist_unload(gboolean markRead) {

	if(displayed_itemSet) {
		itemview_clear();
		itemview_set_itemset(NULL);
		
		/* 1. Postprocessing for previously selected node, this is necessary
		   to realize reliable read marking when using condensed mode. It's
		   important to do this only when the selection really changed. */
		if(markRead && (2 == node_get_view_mode(displayed_itemSet->node))) 
			itemlist_mark_all_read(displayed_itemSet);

		itemlist_check_for_deferred_action();
	}

	itemlist_set_selected(NULL);
	displayed_itemSet = NULL;
}

void itemlist_update_vfolder(vfolderPtr vfolder) {

	if(displayed_itemSet == vfolder->node->itemSet)
		/* maybe itemlist_load(vfolder) would be faster, but
		   it unloads all feeds and therefore must not be 
		   called from here! */		
		itemlist_merge_itemset(displayed_itemSet);
	else
		ui_node_update(vfolder->node);
}

/* next unread selection logic */

static itemPtr itemlist_find_unread_item(void) {
	itemPtr	result = NULL;
	
	if(!displayed_itemSet)
		return NULL;

	if(displayed_itemSet->node->children) {
		feedlist_find_unread_feed(displayed_itemSet->node);
		return NULL;
	}

	/* Note: to select in sorting order we need to do it in the GUI code
	   otherwise we would have to sort the item list here... */
	
	if(displayed_item)
		result = ui_itemlist_find_unread_item(displayed_item);
		
	if(!result)
		result = ui_itemlist_find_unread_item(NULL);

	return result;
}

void itemlist_select_next_unread(void) {
	itemPtr	result = NULL;

	/* If we are in combined mode we have to mark everything
	   read or else we would never jump to the next feed,
	   because no item will be selected and marked read... */
	if(displayed_itemSet) {
		if(NODE_VIEW_MODE_COMBINED == node_get_view_mode(displayed_itemSet->node))
			itemlist_mark_all_read(displayed_itemSet);
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
		item->itemSet->node->needsCacheSave = TRUE;

		/* 1. propagate to model for recursion */
		itemset_set_item_flag(item->itemSet, item, newStatus);

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
	GSList	*iter;

	if(newStatus != item->readStatus) {		
		item->itemSet->node->needsCacheSave = TRUE;

		/* 1. propagate to model for recursion */
		itemset_set_item_read_status(item->itemSet, item, newStatus);

		/* 2. update item list GUI state */
		itemlist_update_item(item);

		/* 3. updated feed list unread counters */
		node_update_counters(item->itemSet->node);
		ui_node_update(item->itemSet->node);
		if(item->sourceNode != item->itemSet->node)
			ui_node_update(item->sourceNode);

		/* 4. update notification statistics */
		feedlist_reset_new_item_count();

		/* 5. duplicate state propagation */
		iter = item_guid_list_get_duplicates_for_id(item);
		while(iter) {
			GList *dupIter;
			nodePtr dupNode = (nodePtr)iter->data;
			
			if(dupNode != item->sourceNode) {
				debug2(DEBUG_UPDATE, "marking duplicate in \"%s\" as %s...", node_get_title(dupNode), newStatus?"read":"unread");
				node_load(dupNode);

				dupIter = dupNode->itemSet->items;
				while(dupIter) {
					itemPtr duplicate = (itemPtr)dupIter->data;
					if(duplicate->id && item->id && !strcmp(duplicate->id, item->id) &&
					   (newStatus != duplicate->readStatus)) {
						/* don't call ourselves with duplicate item to avoid cascading, just repeat 1),2) and 3)... */
						dupNode->needsCacheSave = TRUE;
						itemset_set_item_read_status(dupNode->itemSet, duplicate, newStatus);
						itemlist_update_item(duplicate);
						node_update_counters(dupNode);
						ui_node_update(dupNode);
						debug0(DEBUG_UPDATE, " duplicate sync'ed...\n");
					}
					dupIter = g_list_next(dupIter);
				}

				node_unload(dupNode);
			}
			iter = g_slist_next(iter);
		}
	}
}

void itemlist_toggle_read_status(itemPtr item) {

	itemlist_set_read_status(item, !(item->readStatus));
	itemview_update();
}

void itemlist_set_update_status(itemPtr item, const gboolean newStatus) { 
	
	if(newStatus != item->updateStatus) {	
		item->itemSet->node->needsCacheSave = TRUE;

		/* 1. propagate to model for recursion */
		itemset_set_item_update_status(item->itemSet, item, newStatus);

		/* 2. update item list GUI state */
		itemlist_update_item(item);	

		/* 3. no update of feed list necessary... */
		node_update_counters(item->itemSet->node);

		/* 4. update notification statistics */
		feedlist_reset_new_item_count();
	
		/* no duplicate state propagation necessary */
	}
}

/* function to remove items due to item list filtering */
static void itemlist_hide_item(itemPtr item) {

	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if(displayed_item != item) {
		itemview_remove_item(item);
		ui_node_update(item->itemSet->node);
	} else {
		deferred_item_filter = TRUE;
		/* update the item to show new state that forces
		   later removal */
		itemview_update_item(item);
	}
}

/* functions to remove items on remove requests */

void itemlist_remove_item(itemPtr item) {

	/* Normally the item should exist when removing it, but 
	   when removing with the keyboard this won't be the case.
	   FIXME: An assertion here would be better! */
	if(NULL == itemset_lookup_item(item->itemSet, item->sourceNode, item->sourceNr))
		return;

	if(displayed_item == item) {
		itemlist_set_selected(NULL);
		deferred_item_filter = FALSE;
		deferred_item_remove = FALSE;
		itemview_select_item(NULL);
	}

	itemview_remove_item(item);
	itemview_update();
	itemset_remove_item(item->itemSet, item);
	ui_node_update(item->itemSet->node);
}

void itemlist_request_remove_item(itemPtr item) {
	
	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if(displayed_item != item) {
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
		itemset_remove_item(itemSet, iter->data);
		item_free(iter->data);
		iter = g_list_next(iter);
	}

	itemview_update();
	ui_node_update(itemSet->node);
}

void itemlist_remove_all_items(itemSetPtr itemSet) {

	itemview_clear();
	itemset_remove_all_items(itemSet);
	itemview_update();
	ui_node_update(itemSet->node);
}

void itemlist_update_item(itemPtr item) {

	if(itemlist_filter && !rule_check_item(itemlist_filter, item)) {
		itemlist_hide_item(item);
		return;
	}
	
	itemview_update_item(item);
}

void itemlist_mark_all_read(itemSetPtr itemSet) {
	GList	*iter, *items;
	itemPtr	item;

	/* two loops on list copies because the itemlist_set_* 
	   methods may modify the original item list (e.g.
	   if the displayed item set is a vfolder) */

	items = g_list_copy(itemSet->items);
	iter = items;
	while(iter) {
		item = (itemPtr)iter->data;
		if(!item->readStatus) {
			itemlist_set_read_status(item, TRUE);
			itemlist_update_item(item);
		}
		iter = g_list_next(iter);
	}
	g_list_free(items);

	items = g_list_copy(itemSet->items);
	iter = items;
	while(iter) {	
		item = (itemPtr)iter->data;
		if(item->updateStatus) {
			itemlist_set_update_status(item, FALSE);
			itemlist_update_item(item);
		}
		iter = g_list_next(iter);
	}
	g_list_free(items);
	
	itemSet->node->needsCacheSave = TRUE;
	
	/* GUI updating */	
	itemview_update();
	ui_node_update(itemSet->node);
}

void itemlist_mark_all_old(itemSetPtr itemSet) {

	/* no loop on list copy because the itemset_set_* 
	   methods MUST NOT modify the original item list */

	GList *iter = itemSet->items;
	while(iter) {
		itemPtr item = (itemPtr)iter->data;
		if(item->newStatus) {
			itemset_set_item_new_status(itemSet, item, FALSE);
			itemlist_update_item(item);
		}
		iter = g_list_next(iter);
	}
	
	itemSet->node->needsCacheSave = TRUE;
	
	/* No GUI updating necessary... */
}

void itemlist_mark_all_popup(itemSetPtr itemSet) {

	/* no loop on list copy because the itemset_set_* 
	   methods MUST NOT modify the original item list */

	GList *iter = itemSet->items;
	while(iter) {
		itemPtr item = (itemPtr)iter->data;
		if(item->popupStatus) {
			itemset_set_item_popup_status(itemSet, item, FALSE);
			itemlist_update_item(item);
		}
		iter = g_list_next(iter);
	}
	
	/* No GUI updating necessary... */
}

/* mouse/keyboard interaction callbacks */
void itemlist_selection_changed(itemPtr item) {

	debug_enter("itemlist_selection_changed");
	
	if(0 == itemlistLoading) {
		/* folder&vfolder postprocessing to remove/filter unselected items no
		   more matching the display rules because they have changed state */
		itemlist_check_for_deferred_action();

		debug1(DEBUG_GUI, "item list selection changed to \"%s\"", item?item_get_title(item):"(null)");
		
		itemlist_set_selected(item);
	
		/* set read and unset update status when selecting */
		if(item) {
			itemlist_set_read_status(item, TRUE);
			itemlist_set_update_status(item, FALSE);
			
			if(itemset_load_link_preferred(displayed_itemSet)) {
				ui_htmlview_launch_URL(ui_mainwindow_get_active_htmlview(), 
				                       item_get_source(itemlist_get_selected()), UI_HTMLVIEW_LAUNCH_INTERNAL);
			} else {
				itemview_set_mode(ITEMVIEW_SINGLE_ITEM);
				itemview_select_item(item);
				itemview_update();
			}
		}

		ui_node_update(displayed_itemSet->node);

		feedlist_reset_new_item_count();
	}

	debug_exit("itemlist_selection_changed");
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
		itemlist_load(node->itemSet);
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
