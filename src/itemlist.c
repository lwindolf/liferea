/**
 * @file itemlist.c itemlist handling
 *
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
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
static gboolean itemlistLoading;	/* if TRUE prevents selection effects when loading the item list */
gint disableSortingSaving;		/* set in ui_itemlist.c to disable sort-changed callback */

static gboolean deferred_item_remove = FALSE;	/* TRUE if selected item needs to be removed from cache on unselecting */
static gboolean deferred_item_filter = FALSE;	/* TRUE if selected item needs to be filtered on unselecting */

itemPtr itemlist_get_selected(void) {

	return displayed_item;
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
		displayed_item = NULL;

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
	
	if(displayed_itemSet == NULL)
		return; /* Nothing to do if nothing is displayed */
	
	if((displayed_itemSet != itemSet) && !node_is_ancestor(displayed_itemSet->node, itemSet->node))
		return;

	debug1(DEBUG_GUI, "reloading item list with node \"%s\"", node_get_title(itemSet->node));

	if((ITEMSET_TYPE_FOLDER == displayed_itemSet->type) && 
	   (0 == getNumericConfValue(FOLDER_DISPLAY_MODE)))
			return;

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
	GtkTreeModel	*model;

	debug_enter("itemlist_load");

	g_assert(NULL != itemSet);

	debug1(DEBUG_GUI, "loading item list with node \"%s\"\n", node_get_title(itemSet->node));

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

	itemlistLoading = 1;
	viewMode = node_get_view_mode(itemSet->node);
	ui_mainwindow_set_layout(viewMode);
	
	// FIXME: move ui_itemlist setup to itemview.c

	/* 2. Clear item list and disable sorting for performance reasons */

	/* Free the old itemstore and create a new one; this is the only way to disable sorting */
	ui_itemlist_reset_tree_store();	 /* this also clears the itemlist. */
	model = GTK_TREE_MODEL(ui_itemlist_get_tree_store());

	ui_itemlist_enable_encicon_column(FALSE);

	switch(itemSet->type) {
		case ITEMSET_TYPE_FEED:
			ui_itemlist_enable_favicon_column(FALSE);
			break;
		case ITEMSET_TYPE_VFOLDER:
		case ITEMSET_TYPE_FOLDER:
			ui_itemlist_enable_favicon_column(TRUE);
			break;
	}

	/* 3. Set sorting again... */
	disableSortingSaving++;
	gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), 
	                                     itemSet->node->sortColumn, 
	                                     itemSet->node->sortReversed?GTK_SORT_DESCENDING:GTK_SORT_ASCENDING);
	disableSortingSaving--;

	/* 4. Load the new one... */
	displayed_itemSet = itemSet;
	itemview_set_itemset(itemSet);

	if(NODE_VIEW_MODE_COMBINED != node_get_view_mode(itemSet->node))
		itemview_set_mode(ITEMVIEW_NODE_INFO);
	else
		itemview_set_mode(ITEMVIEW_ALL_ITEMS);
	
	itemlist_merge_itemset(itemSet);

	itemlistLoading = 0;

	debug_exit("itemlist_load");
}

void itemlist_unload(gboolean markRead) {

	if(displayed_itemSet) {
		itemview_clear();
		
		/* 1. Postprocessing for previously selected node, this is necessary
		   to realize reliable read marking when using condensed mode. It's
		   important to do this only when the selection really changed. */
		if(markRead && (2 == node_get_view_mode(displayed_itemSet->node))) 
			itemlist_mark_all_read(displayed_itemSet);

		itemlist_check_for_deferred_action();
	}

	displayed_item = NULL;
	displayed_itemSet = NULL;
}

void itemlist_update_vfolder(vfolderPtr vp) {

	if(displayed_itemSet == vp->node->itemSet)
		/* maybe itemlist_load(vp) would be faster, but
		   it unloads all feeds and therefore must not be 
		   called from here! */		
		itemlist_merge_itemset(displayed_itemSet);
	else
		ui_node_update(vp->node);
}

void itemlist_reset_date_format(void) {
	
	ui_itemlist_reset_date_format();
	itemview_update();
}

/* next unread selection logic */

static gboolean itemlist_find_unread_item(void) {
	
	if(!displayed_itemSet)
		return FALSE;
		
	if(displayed_itemSet->node->children) {
		feedlist_find_unread_feed(displayed_itemSet->node);
		return FALSE;
	}

	/* Note: to select in sorting order we need to do it in the GUI code
	   otherwise we would have to sort the item list here... */
	
	if(!displayed_item || !ui_itemlist_find_unread_item(displayed_item))
		return ui_itemlist_find_unread_item(NULL);
	else
		return TRUE;
	
	return FALSE;
}

void itemlist_select_next_unread(void) {
	nodePtr		node;
	
	/* before scanning the feed list, we test if there is a unread 
	   item in the currently selected feed! */
	if(itemlist_find_unread_item())
		return;
	
	/* scan feed list and find first feed with unread items */
	node = feedlist_find_unread_feed(feedlist_get_root());
	if(node) {
		
		/* load found feed */
		ui_feedlist_select(node);

		/* find first unread item */
		itemlist_find_unread_item();
	} else {
		/* if we don't find a feed with unread items do nothing */
		ui_mainwindow_set_status_bar(_("There are no unread items "));
	}
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
	}
}
	
void itemlist_toggle_flag(itemPtr item) {

	itemlist_set_flag(item, !(item->flagStatus));
	itemview_update();
}

void itemlist_set_read_status(itemPtr item, gboolean newStatus) {

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

	g_assert(NULL != itemset_lookup_item(item->itemSet, item->itemSet->node, item->nr));
	
	if(displayed_item == item)
		itemview_select_item(NULL);
	itemview_remove_item(item);
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

void itemlist_remove_items(itemSetPtr itemSet) {

	itemview_clear();
	itemset_remove_items(itemSet);
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

	/* loop on list copy because the itemlist_set_* 
	   methods may modify the original item list */

	GList *items = g_list_copy(itemSet->items);
	GList *iter = items;
	while(iter) {
		itemPtr item = (itemPtr)iter->data;
		if(item->newStatus) {
			itemset_set_item_new_status(itemSet, item, FALSE);
			itemlist_update_item(item);
		}
		iter = g_list_next(iter);
	}
	g_list_free(items);
	
	/* No GUI updating necessary... */
}

/* mouse/keyboard interaction callbacks */
void itemlist_selection_changed(itemPtr item) {

	debug_enter("itemlist_selection_changed");
	
	if(!itemlistLoading) {
		/* folder&vfolder postprocessing to remove/filter unselected items no
		   more matching the display rules because they have changed state */
		itemlist_check_for_deferred_action();

		if(displayed_item)
			script_run_for_hook(SCRIPT_HOOK_ITEM_UNSELECT);
	
		debug1(DEBUG_GUI, "item list selection changed to \"%s\"", item_get_title(item));

		displayed_item = item;
		
		/* set read and unset update status when selecting */
		if(item) {
			itemlist_set_read_status(item, TRUE);
			itemlist_set_update_status(item, FALSE);
			
			if(itemset_load_link_preferred(displayed_itemSet)) {
				ui_htmlview_launch_URL(ui_mainwindow_get_active_htmlview(), 
				                       item_get_source(itemlist_get_selected()), 2);
			} else {
				itemview_set_mode(ITEMVIEW_SINGLE_ITEM);
				itemview_update();
			}
		}

		ui_node_update(displayed_itemSet->node);

		feedlist_reset_new_item_count();
	
		if(displayed_item)
			script_run_for_hook(SCRIPT_HOOK_ITEM_SELECTED);
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
