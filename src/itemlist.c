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
 
#include "itemlist.h"
#include "conf.h"
#include "debug.h"
#include "feed.h"
#include "support.h"
#include "ui_itemlist.h"
#include "ui_feedlist.h"

/* controller implementation for itemlist handling, bypass only for read-only feed/item access! */

extern nodePtr		displayed_node;
extern itemPtr		displayed_item;

extern gboolean itemlist_loading;
static gint disableSortingSaving;		

static gboolean deferred_item_remove = FALSE;

static void itemlist_load_feed(feedPtr fp, gpointer data) {
	gboolean	merge = GPOINTER_TO_INT(data);
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
}

static void itemlist_check_if_child(feedPtr fp, gpointer data) {

	if((nodePtr)fp == (nodePtr)data)
		itemlist_load_feed(fp, (gpointer)TRUE);
}

/**
 * To be called whenever a feed was updated. If it is a somehow
 * displayed feed it is loaded this method decides if the
 * and how the item list GUI needs to be updated.
 */
void itemlist_reload(nodePtr node) {
	gboolean	isFeed;
	gboolean	isFolder;

	/* determine what node type is actually selected */
	isFeed = ((displayed_node != NULL) && ((FST_FEED == displayed_node->type) || (FST_VFOLDER == displayed_node->type)));
	isFolder = ((displayed_node != NULL) && (FST_FOLDER == displayed_node->type));
	g_assert(isFeed || isFolder);

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
			ui_feedlist_do_for_all_data(displayed_node, ACTION_FILTER_FEED, itemlist_load_feed, GINT_TO_POINTER(TRUE));
		}
	}

	if(TRUE == isFeed) {
		if(node != displayed_node)
			return;
		itemlist_load_feed((feedPtr)node, (gpointer)TRUE);
	}

	ui_itemlist_display();
}

/** 
 * To be called whenever a feed was selected and should
 * replace the current itemlist.
 */
void itemlist_load(nodePtr node) {
	GtkTreeModel	*model;
	gint		sortColumn;
	gboolean	isFeed;
	gboolean	isFolder;

	/* Postprocessing for previously selected node, this is necessary
	   to realize reliable read marking when using condensed mode. It's
	   important to do this only when the selection really changed. */
	isFeed = ((displayed_node != NULL) && ((FST_FEED == displayed_node->type) || (FST_VFOLDER == displayed_node->type)));
	if(isFeed && (node != displayed_node) && (TRUE == feed_get_two_pane_mode((feedPtr)displayed_node)))
		itemlist_mark_all_read(displayed_node);

	/* preparation done, we can select it... */
	displayed_node = node;

	/* determine what node type is now selected */
	isFeed = ((node != NULL) && ((FST_FEED == node->type) || (FST_VFOLDER == node->type)));
	isFolder = ((node != NULL) && (FST_FOLDER == node->type));
	
	if(!isFeed && !isFolder) {
		ui_itemlist_clear();
		displayed_node = NULL;
		displayed_item = NULL;
		return;
	}
	
	if(isFolder && (1 != getNumericConfValue(FOLDER_DISPLAY_MODE)))
		return;

	model = GTK_TREE_MODEL(ui_itemlist_get_tree_store());
		
	if(isFeed) {
		disableSortingSaving++;
		gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), ((feedPtr)node)->sortColumn, ((feedPtr)node)->sortReversed?GTK_SORT_DESCENDING:GTK_SORT_ASCENDING);
		disableSortingSaving--;
	}
	
	if(isFolder)
		ui_itemlist_set_two_pane_mode(FALSE);
		
	ui_itemlist_clear();
	/* explicitly no ui_htmlview_clear() !!! */
	displayed_item = NULL;

	if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
		debug0(DEBUG_CACHE, "unloading everything...");
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, feed_unload);
	}
	
	itemlist_reload(node);
	ui_itemlist_enable_favicon_column((FST_VFOLDER == node->type) || (FST_FOLDER == node->type));
	ui_itemlist_prefocus();
}

void itemlist_update_vfolder(nodePtr vp) {

	if(displayed_node == vp)
		/* maybe itemlist_load(vp) would be faster, but
		   it unloads all feeds and therefore must not be 
		   called from here! */		
		itemlist_reload(vp);
	else
		ui_feed_update(vp);
}

void itemlist_reset_date_format(void) {
	
	ui_itemlist_reset_date_format();
	if (!ui_itemlist_get_two_pane_mode())
		ui_itemlist_update();
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
			ui_itemlist_update_item(ip);	
				
			vfolder_update_item(ip);	/* there might be vfolders using this item */
			vfolder_check_item(ip);		/* and check if now a rule matches */
		}
		ui_feedlist_update();
	}
}
	
void itemlist_toggle_flag(itemPtr ip) {

	itemlist_set_flag(ip, !item_get_flag(ip));
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

	itemlist_set_read_status(ip, !item_get_read_status(ip));
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
	
	ui_itemlist_update_item(ip);
}

void itemlist_remove_item(itemPtr ip) {

	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if(displayed_item != ip) {
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
	   ((FST_FEED == np->type) || (FST_VFOLDER == np->type))) {
		feed_set_two_pane_mode((feedPtr)np, new_mode);
		ui_itemlist_set_two_pane_mode(new_mode);
	}
}
