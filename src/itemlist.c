/**
 * @file itemlist.c itemlist handling
 *
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
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
 
#include "itemlist.h"
#include "conf.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "support.h"
#include "vfolder.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_feed.h"
#include "ui/ui_tray.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"

/* controller implementation for itemlist handling, bypass only for read-only item access! */

extern nodePtr		displayed_node;
extern itemPtr		displayed_item;

extern gboolean itemlist_loading;
static gint disableSortingSaving;		

static gboolean deferred_item_remove = FALSE;

static void itemlist_check_for_deferred_removal(void) {
	itemPtr ip;

	if(NULL != displayed_item) {
		ip = displayed_item;
		displayed_item = NULL;
		if(TRUE == deferred_item_remove) {
			deferred_item_remove = FALSE;
			itemlist_remove_item(ip);
		}
	}
}

static void itemlist_load_node(nodePtr np, gboolean merge) {
	gboolean	loadReadItems = TRUE;
	GList		*item;
	itemPtr		ip;

	debug_enter("itemlist_load_node");
	
	debug2(DEBUG_GUI, "loading item list with node \"%s\" (merge=%s)", node_get_title(np), merge?"TRUE":"FALSE");
	
	if(FST_FOLDER == displayed_node->type)
		loadReadItems = !getBooleanConfValue(FOLDER_DISPLAY_HIDE_READ);

	/* load model */
	feedlist_load_node(np);
	/* update itemlist in view */	
	item = g_list_last(np->itemSet->items);
	while(NULL != item) {
		ip = item->data;
		g_assert(NULL != ip);

		if((FALSE == ip->readStatus) || (TRUE == loadReadItems))
			ui_itemlist_add_item(ip, merge);

		item = g_list_previous(item);
	}

	debug_exit("itemlist_load_node");
}

static void itemlist_check_if_child(nodePtr np, gpointer data) {

	if(np == (nodePtr)data)
		itemlist_load_node(np, TRUE);
}

/**
 * To be called whenever a feed was updated. If it is a somehow
 * displayed feed it is loaded this method decides if the
 * and how the item list GUI needs to be updated.
 */
void itemlist_reload(nodePtr node) {
	gboolean	isFeed;
	gboolean	isFolder;

	debug_enter("itemlist_reload");
	
	if(displayed_node == NULL)
		return; /* Nothing to do if nothing is displayed */
	
	/* determine what node type is actually selected */
	isFeed = (FST_FEED == displayed_node->type) || (FST_VFOLDER == displayed_node->type);
	isFolder = FST_FOLDER == displayed_node->type;
	
	if((TRUE == isFolder) && (1 == getNumericConfValue(FOLDER_DISPLAY_MODE))) {
		if((FST_FOLDER != node->type) && (node != displayed_node)) {
			/* There are two cases: the updated feed is a child of
			   the displayed folder or not. If it is we want to update the
			   item list of this folder. */
			ui_feedlist_do_for_all_data(displayed_node, ACTION_FILTER_FEED, itemlist_check_if_child, (gpointer)node);
			ui_itemlist_display();
			return;
		} else {
			/* and the user might get click directly on a folder, then we
			   can unconditionally load all child feeds into the itemlist */
			itemlist_load_node(node, TRUE);
		}
	}

	if(TRUE == isFeed) {
		if(node != displayed_node)
			return;
		itemlist_load_node(node, TRUE);
	}

	ui_itemlist_display();

	debug_exit("itemlist_reload");
}

/** 
 * To be called whenever a feed was selected and should
 * replace the current itemlist.
 */
void itemlist_load(nodePtr node) {
	GtkTreeModel	*model;
	gboolean	isFeed;
	gboolean	isFolder;
	gint		sortColumn;
	GtkSortType	sortType;
	gboolean	sorted;

	if(NULL != node) {
		/* 1. Postprocessing for previously selected node, this is necessary
		   to realize reliable read marking when using condensed mode. It's
		   important to do this only when the selection really changed. */
		switch(node->type) {
			case FST_FEED:
			case FST_VFOLDER:
				if((node != displayed_node) && (NULL != displayed_node) &&
				   (TRUE == node_get_two_pane_mode(displayed_node)))
					itemset_mark_all_read(displayed_node->itemSet);
				break;
			case FST_FOLDER:
				// Nothing to do?
				break;
		}
	}

	/* 2. Don't continue if folder is selected and no
	   folder viewing is configured. */
	if((NULL != node) && (FST_FOLDER == node->type) && 
	   (0 == getNumericConfValue(FOLDER_DISPLAY_MODE)))
		return;

	itemlist_check_for_deferred_removal();
	
	/* 3. Load the selected feed. */
	if(NULL != node) {

		/* Disable sorting for performance reasons */
		model = GTK_TREE_MODEL(ui_itemlist_get_tree_store());
		sorted = gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(model), &sortColumn, &sortType);
	
		/* Free the old itemstore and create a new one; this is the only way to disable sorting */
		ui_itemlist_reset_tree_store();	 /* this also clears the itemlist. */
		model = GTK_TREE_MODEL(ui_itemlist_get_tree_store());

		switch(node->type) {
			case FST_FEED:
				ui_itemlist_enable_favicon_column(FALSE);
				break;
			case FST_VFOLDER:
				ui_itemlist_enable_favicon_column(TRUE);
				break;
			case FST_FOLDER:
				ui_itemlist_enable_favicon_column(TRUE);
				itemlist_set_two_pane_mode(FALSE);
				break;
		}

		/* Enable sorting again */
		disableSortingSaving++;
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), node->sortColumn, node->sortReversed?GTK_SORT_DESCENDING:GTK_SORT_ASCENDING);
		disableSortingSaving--;

		/* Unload old selected node */
		if(NULL != displayed_node)
			feedlist_unload_node(displayed_node);
	
		/* Load the new one... */
		ui_itemlist_prefocus();
		displayed_node = node;
		itemlist_reload(node);
	} else {
		ui_itemlist_clear();
		displayed_node = NULL;
		displayed_item = NULL;
	}

}

void itemlist_update_vfolder(vfolderPtr vp) {

	if(displayed_node == vp->node)
		/* maybe itemlist_load(vp) would be faster, but
		   it unloads all feeds and therefore must not be 
		   called from here! */		
		itemlist_reload(vp->node);
	else
		ui_node_update(vp->node);
}

void itemlist_reset_date_format(void) {
	
	ui_itemlist_reset_date_format();
	if(!ui_itemlist_get_two_pane_mode())
		ui_itemlist_update();
}

/* menu commands */
void itemlist_set_flag(itemPtr ip, gboolean newStatus) {
	itemPtr		sourceItem;
	feedPtr		sourceFeed;
	
	if(newStatus != ip->flagStatus) {
		displayed_node->needsCacheSave = TRUE;

		/* 1. propagate to model for recursion */
		itemset_set_item_flag(displayed_node->itemSet, ip, newStatus);

		/* 2. update item list GUI state */
		ui_itemlist_update_item(ip);

		/* 3. updated feed list unread counters */
		ui_node_update(displayed_node);

		/* 4. update notification statistics */
		feedlist_reset_new_item_count();		
	}
}
	
void itemlist_toggle_flag(itemPtr ip) {

	itemlist_set_flag(ip, !(ip->flagStatus));
}

void itemlist_set_read_status(itemPtr ip, gboolean newStatus) {
	itemPtr		sourceItem;
	feedPtr		sourceFeed;

	if(newStatus != ip->readStatus) {		
		displayed_node->needsCacheSave = TRUE;

		/* 1. propagate to model for recursion */
		itemset_set_item_read_status(displayed_node->itemSet, ip, newStatus);

		/* 2. update item list GUI state */
		ui_itemlist_update_item(ip);

		/* 3. updated feed list unread counters */
		ui_node_update(displayed_node);

		/* 4. update notification statistics */
		feedlist_reset_new_item_count();
	}
}

void itemlist_toggle_read_status(itemPtr ip) {

	itemlist_set_read_status(ip, !(ip->readStatus));
}

void itemlist_set_update_status(itemPtr ip, const gboolean newStatus) { 
	itemPtr		sourceItem;
	feedPtr		sourceFeed;
	
	if(newStatus != ip->updateStatus) {	
		displayed_node->needsCacheSave = TRUE;

		/* 1. propagate to model for recursion */
		itemset_set_item_update_status(displayed_node->itemSet, ip, newStatus);

		/* 2. update item list GUI state */
		ui_itemlist_update_item(ip);	

		/* 3. updated feed list unread counters */
		ui_node_update(displayed_node);

		/* 4. update notification statistics */
		feedlist_reset_new_item_count();
	}
}

void itemlist_update_item(itemPtr ip) {
	
	ui_itemlist_update_item(ip);
}

void itemlist_remove_item(itemPtr ip) {
	
	/* FIXME: is the following check really necessary? */
	if(NULL != itemset_lookup_item(ip->itemSet, ip->itemSet->node, ip->nr)) {
		/* if the currently selected item should be removed we
		   don't do it and set a flag to do it when unselecting */
		if(displayed_item != ip) {
			ui_itemlist_remove_item(ip);
			itemset_remove_item(ip->itemSet, ip);
			ui_node_update(ip->itemSet->node);
		} else {
			deferred_item_remove = TRUE;
			/* update the item to show new state that forces
			   later removal */
			ui_itemlist_update_item(ip);
		}
	}
}

void itemlist_remove_items(nodePtr np) {

	ui_itemlist_clear();
	ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
	itemset_remove_items(np->itemSet);
	ui_node_update(np);
}

void itemlist_mark_all_read(nodePtr np) {

	itemset_mark_all_read(np->itemSet);
	ui_itemlist_update();
	ui_node_update(np);
}

/* mouse/keyboard interaction callbacks */
void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data) {
	GtkTreeIter 	iter;
	GtkTreeModel	*model;
	itemPtr 	ip;
	
	if(!itemlist_loading && (FALSE == ui_itemlist_get_two_pane_mode())) {
		/* vfolder postprocessing to remove unselected items not
		   more matching the rules because they have changed state */
		itemlist_check_for_deferred_removal();
	
		if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
			displayed_item = ip = ui_itemlist_get_item_from_iter(&iter);
			debug1(DEBUG_GUI, "item list selection changed to \"%s\"", item_get_title(ip));
			itemset_render_item(ip->itemSet, ip);
			/* set read and unset update status done when unselecting */
			itemlist_set_read_status(ip, TRUE);
			itemlist_set_update_status(ip, FALSE);
			ui_node_update(displayed_node);
		} else {
			displayed_item = NULL;
		}

		feedlist_reset_new_item_count();
	}
}

void itemlist_sort_column_changed_cb(GtkTreeSortable *treesortable, gpointer user_data) {
	gint		sortColumn;
	GtkSortType	sortType;
	gboolean	sorted;
	nodePtr		np;
	
	np = ui_feedlist_get_selected();	
	if(np == NULL || disableSortingSaving != 0)
		return;
	
	sorted = gtk_tree_sortable_get_sort_column_id(treesortable, &sortColumn, &sortType);
	node_set_sort_column(np, sortColumn, sortType == GTK_SORT_DESCENDING);
}

/* two/three pane mode callbacks */
void itemlist_set_two_pane_mode(gboolean new_mode) {
	gboolean	old_mode;
	nodePtr		np;

	np = ui_feedlist_get_selected();	
	if(NULL == np)
		return;

	old_mode = ui_itemlist_get_two_pane_mode();

	/* for now we disallow folders in two pane mode ... */
	if((FST_FEED == np->type) || (FST_VFOLDER == np->type)) 
		node_set_two_pane_mode(np, new_mode);
	
	ui_itemlist_set_two_pane_mode(new_mode);
}
