/**
 * @file itemlist.c itemlist handling
 *
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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
 
#include "itemlist.h"
#include "conf.h"
#include "debug.h"
#include "feed.h"
#include "ui_itemlist.h"
#include "ui_feedlist.h"
#include "ui_htmlview.h"
#include "ui_tray.h"
#include "ui_feed.h"
#include "ui_mainwindow.h"

/* controller implementation for itemlist handling, bypass only for read-only feed/item access! */

static nodePtr		displayed_node = NULL;
static itemPtr		displayed_item = NULL;

extern gboolean itemlist_loading;
static gint disableSortingSaving;		

static gboolean deferred_item_remove = FALSE;

static void itemlist_load_feed(feedPtr fp, gpointer data) {
	gboolean	merge = *(gboolean *)data;
	GSList		*item, *itemlist;
	itemPtr		ip;
	
	/* load model */
	feed_load(fp);
	
	/* update itemlist in view */	
	itemlist = feed_get_item_list(fp);
	itemlist = g_slist_copy(itemlist);
	itemlist = g_slist_reverse(itemlist);
	item = itemlist;
	while(NULL != item) {
		ip = item->data;
		g_assert(NULL != ip);
		ui_itemlist_add_item(ip, merge);
		item = g_slist_next(item);
	}
	g_slist_free(itemlist);	
	
	ui_itemlist_enable_favicon_column(FST_VFOLDER == feed_get_type(fp));
}

/* Loads or merges the passed feeds items into the itemlist. 
   If the selected feed is equal to the passed one we do 
   merging. Otherwise we can just clear the list and load
   the new items. */
void itemlist_load(nodePtr node) {
	GtkTreeModel	*model;
	gint		sortColumn;
	gboolean	isFeed;
	gboolean	merge = (node == displayed_node);

	/* postprocessing for previously selected node, this is necessary
	   to realize reliable read marking when using condensed mode */
	isFeed = ((displayed_node != NULL) && ((FST_FEED == displayed_node->type) || (FST_VFOLDER == displayed_node->type)));
	if(!merge && isFeed && (TRUE == feed_get_two_pane_mode((feedPtr)displayed_node)))
		itemlist_mark_all_read(displayed_node);

	displayed_node = node;

	isFeed = ((node != NULL) && ((FST_FEED == node->type) || (FST_VFOLDER == node->type)));
	if(!isFeed) {
		/* for now we do nothing for folders... (might be changed in future) */
		ui_itemlist_clear();
		displayed_node = NULL;
		displayed_item = NULL;
		return;
	}

	model = GTK_TREE_MODEL(ui_itemlist_get_tree_store());

	if(FALSE == merge) {
		ui_itemlist_clear();
		displayed_item = NULL;
		/* explicitly no ui_htmlview_clear() !!! */
	
		if(isFeed) {
			disableSortingSaving++;
			gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), ((feedPtr)node)->sortColumn, ((feedPtr)node)->sortReversed?GTK_SORT_DESCENDING:GTK_SORT_ASCENDING);
			disableSortingSaving--;
		}
		
		if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
			debug0(DEBUG_CACHE, "unloading everything...");
			ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, feed_unload);
		}
	}	

	/* Add the new items */
	if(TRUE == isFeed) {
		itemlist_load_feed((feedPtr)node, (gpointer)&merge);
	} else if(FST_FOLDER == node->type) {
		ui_feedlist_do_for_all_data(node, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, itemlist_load_feed, (gpointer)&merge);
	}

	ui_itemlist_display();
	
	if(FALSE == merge)
		ui_itemlist_prefocus();
}

/* updating methods */
void itemlist_update() {
	GSList	*iter;
	       
	if(NULL == displayed_node)
		return;
		
	if((FST_FEED != displayed_node->type) && (FST_VFOLDER != displayed_node->type))
		return;
		
	iter = feed_get_item_list((feedPtr)displayed_node);
	while(NULL != iter) {
		ui_itemlist_update_item((itemPtr)(iter->data));
		iter = g_slist_next(iter);
	}
}

void itemlist_update_vfolder(nodePtr vp) {

	if(displayed_node == vp)
		itemlist_load(vp);
	else
		ui_feed_update(vp);
}

void itemlist_reset_date_format(void) {

	ui_itemlist_reset_date_format();
	itemlist_update();
}

/* menu commands */
void itemlist_set_flag(itemPtr ip, gboolean newStatus) {
	itemPtr		sourceItem;
	feedPtr		sourceFeed;
	
	if(newStatus != item_get_flag(ip)) {
		/* if this item belongs to a vfolder update the source feed */
		if(ip->sourceFeed != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceFeed = ip->sourceFeed;	/* keep feed pointer because ip might be free'd */
			feed_load(sourceFeed);
			if(NULL != (sourceItem = feed_lookup_item(sourceFeed, ip->nr)))
				itemlist_set_flag(sourceItem, newStatus);
			feed_unload(sourceFeed);
		} else {
			item_set_flag(ip, newStatus);
			if(displayed_node == (nodePtr)ip->fp)
				ui_itemlist_update_item(ip);	
				
			vfolder_update_item(ip);	/* there might be vfolders using this item */
			vfolder_check_item(ip);		/* and check if now a rule matches */
		}
		ui_feedlist_update();
	}
}
	
void itemlist_toggle_flag(itemPtr ip) {

	if(TRUE == item_get_flag(ip))
		itemlist_set_flag(ip, FALSE);
	else
		itemlist_set_flag(ip, TRUE);
}

void itemlist_set_read_status(itemPtr ip, gboolean newStatus) {
	itemPtr		sourceItem;
	feedPtr		sourceFeed;

	if(newStatus != item_get_read_status(ip)) {		
		/* if this item belongs to a vfolder update the source feed */
		if(ip->sourceFeed != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceFeed = ip->sourceFeed;	/* keep feed pointer because ip might be free'd */
			feed_load(sourceFeed);
			if(NULL != (sourceItem = feed_lookup_item(sourceFeed, ip->nr)))
				itemlist_set_read_status(sourceItem, newStatus);
			feed_unload(sourceFeed);
		} else {
			item_set_read_status(ip, newStatus);
			if(displayed_node == (nodePtr)ip->fp)
				ui_itemlist_update_item(ip);
			
			vfolder_update_item(ip);	/* there might be vfolders using this item */
			vfolder_check_item(ip);		/* and check if now a rule matches */
		}
		ui_feedlist_update();
		
		if(TRUE == newStatus)
			ui_tray_zero_new();		/* reset tray icon */
	}
}

void itemlist_toggle_read_status(itemPtr ip) {

	if(TRUE == item_get_read_status(ip))
		itemlist_set_read_status(ip, FALSE);
	else
		itemlist_set_read_status(ip, TRUE);
}

void itemlist_set_update_status(itemPtr ip, const gboolean newStatus) { 
	itemPtr		sourceItem;
	feedPtr		sourceFeed;
	
	if(newStatus != item_get_update_status(ip)) {	
		/* if this item belongs to a vfolder update the source feed */
		if(ip->sourceFeed != NULL) {
			/* propagate change to source feed, this indirectly updates us... */
			sourceFeed = ip->sourceFeed;	/* keep feed pointer because ip might be free'd */
			feed_load(sourceFeed);
			if(NULL != (sourceItem = feed_lookup_item(sourceFeed, ip->nr)))
				itemlist_set_update_status(sourceItem, newStatus);
			feed_unload(sourceFeed);
		} else {
			item_set_update_status(ip, newStatus);
			if(displayed_node == (nodePtr)ip->fp)
				ui_itemlist_update_item(ip);

			vfolder_update_item(ip);	/* there might be vfolders using this item */
			vfolder_check_item(ip);		/* and check if now a rule matches */
		}
		ui_feedlist_update();
	}
}

void itemlist_mark_all_read(nodePtr np) {
	GSList	*item, *items;

	if(FST_FOLDER == np->type) {
		/* if we have selected a folder we mark all item of all feeds as read */
		ui_feedlist_do_for_all(np, ACTION_FILTER_FEED, (nodeActionFunc)itemlist_mark_all_read);
	} else {
		/* if not we mark all items of the item list as read */
		
		feed_load((feedPtr)np);

		/* two loops on list copies because the itemlist_set_* 
		   methods may modify the feeds original item list */

		items = g_slist_copy(((feedPtr)np)->items);
		item = items;
		while(NULL != item) {
			itemlist_set_read_status((itemPtr)item->data, TRUE);
			item = g_slist_next(item);
		}
		g_slist_free(items);

		items = g_slist_copy(((feedPtr)np)->items);
		item = items;
		while(NULL != item) {	
			itemlist_set_update_status((itemPtr)item->data, FALSE);
			item = g_slist_next(item);
		}
		g_slist_free(items);

		feed_unload((feedPtr)np);
	}
}

void itemlist_update_item(itemPtr ip) {
	
	if(displayed_node == (nodePtr)ip->fp)
		ui_itemlist_update_item(ip);
}

void itemlist_remove_item(itemPtr ip) {

	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if(displayed_item != ip) {
		if(displayed_node == (nodePtr)(ip->fp))
			ui_itemlist_remove_item(ip);
		feed_remove_item(ip->fp, ip);
		ui_feedlist_update();
	} else {
		deferred_item_remove = TRUE;
		/* update the item to show new state that forces
		   later removal */
		ui_itemlist_update_item(ip);
	}
}

void itemlist_remove_items(feedPtr fp) {

	g_assert(displayed_node == (nodePtr)fp);
	ui_itemlist_clear();
	ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
	feed_remove_items(fp);
	ui_feedlist_update();
}

/* mouse/keyboard interaction callbacks */
void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data) {
	GtkTreeIter 	iter;
	GtkTreeModel	*model;
	itemPtr 	ip;
	
	if(!itemlist_loading && (FALSE == ui_itemlist_get_two_pane_mode())) {
		/* vfolder postprocessing to remove unselected items not
		   more matching the rules because they have changed state */
		if(NULL != displayed_item) {
			ip = displayed_item;
			displayed_item = NULL;
			if(TRUE == deferred_item_remove) {
				deferred_item_remove = FALSE;
				itemlist_remove_item(ip);
			}
		}
	
		if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
			gtk_tree_model_get(model, &iter, IS_PTR, &ip, -1);			
			displayed_item = ip;
			item_display(ip);
			/* set read and unset update status done when unselecting */
			itemlist_set_read_status(ip, TRUE);
			itemlist_set_update_status(ip, FALSE);
			ui_feedlist_update();
		} else {
			displayed_item = NULL;
		}
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
	if((FST_FEED == np->type) || (FST_VFOLDER == np->type))
		feed_set_sort_column((feedPtr)np, sortColumn, sortType == GTK_SORT_DESCENDING);
}

/* two/three pane mode callbacks */
void itemlist_set_two_pane_mode(gboolean new_mode) {
	gboolean	old_mode;
	nodePtr		np;

	old_mode = ui_itemlist_get_two_pane_mode();
	
	if((NULL != (np = ui_feedlist_get_selected())) &&
	   ((FST_FEED == np->type) || (FST_VFOLDER == np->type))) 
		feed_set_two_pane_mode((feedPtr)np, new_mode);

	ui_itemlist_set_two_pane_mode(new_mode);
}
