/**
 * @file feedlist.c feedlist handling
 *
 * Copyright (C) 2005-2006 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2005-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <libxml/uri.h>
#include "support.h"
#include "feed.h"
#include "feedlist.h"
#include "newsbin.h"
#include "node.h"
#include "folder.h"
#include "vfolder.h"
#include "itemlist.h"
#include "update.h"
#include "conf.h"
#include "debug.h"
#include "callbacks.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_tray.h"
#include "ui/ui_feed.h"
#include "ui/ui_node.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/default_source.h"
#include "fl_sources/dummy_source.h"
#include "notification/notif_plugin.h"
#include "scripting/script.h"

static guint unreadCount = 0;
static guint newCount = 0;

static nodePtr	rootNode = NULL;

/* selectedNode matches the node selected in the feed list tree view, which
   is not necessarily the displayed one (e.g. folders without recursive
   display enabled) */
static nodePtr	selectedNode = NULL; 		

/* set when a feed list save is scheduled */
static guint feedlist_save_timer = 0;

/* this flag prevents the feed list being saved before it is completely loaded */
static gboolean feedlistLoading = TRUE;

nodePtr feedlist_get_root(void) { return rootNode; }

nodePtr feedlist_get_selected(void) { return selectedNode; }

nodePtr feedlist_get_insertion_point(void) { 

	g_assert(NULL != rootNode);

	if(!selectedNode)
		return rootNode;
	
	if(NODE_TYPE_FOLDER == selectedNode->type)
		return selectedNode;
	
	if(selectedNode->parent) 
		return selectedNode->parent;
	else
		return rootNode;
}

/* statistic handling methods */

int feedlist_get_unread_item_count(void) { return unreadCount; }
int feedlist_get_new_item_count(void) { return newCount; }

void feedlist_update_counters(gint unreadDiff, gint newDiff) {

	unreadCount += unreadDiff;
	newCount += newDiff;

	if((0 != newDiff) || (0 != unreadDiff)) {
		ui_tray_update();
		ui_mainwindow_update_feedsinfo();
	}
}

static void feedlist_unset_new_items(nodePtr node) {
	
	if(0 != node->newCount) {
		node_load(node);
		itemlist_mark_all_old(node->itemSet);
		node_unload(node);
	}
}

void feedlist_reset_new_item_count(void) {

	if(0 != newCount) {
		feedlist_foreach(feedlist_unset_new_items);
		newCount = 0;
		ui_tray_update();
		ui_mainwindow_update_feedsinfo();
	}
}

void feedlist_remove_node(nodePtr node) {

	debug_enter("feedlist_remove_node");

	if(node == selectedNode) {
		ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
		itemlist_unload(FALSE);
		ui_feedlist_select(NULL);
		selectedNode = NULL;
	}

	node_request_remove(node);

	debug_exit("feedlist_remove_node");
}

static gboolean feedlist_auto_update(void *data) {

	debug_enter("feedlist_auto_update");

	if(update_is_online())
		feedlist_foreach(node_request_auto_update);
	else
		debug0(DEBUG_UPDATE, "no update processing because we are offline!");
	
	debug_exit("feedlist_auto_update");

	return TRUE;
}

/* next unread scanning */

enum scanStateType {
  UNREAD_SCAN_INIT,            /* selected not yet passed */
  UNREAD_SCAN_FOUND_SELECTED,  /* selected passed */
  UNREAD_SCAN_SECOND_PASS      /* no unread items after selected feed */
};

static enum scanStateType scanState = UNREAD_SCAN_INIT;

/* This method tries to find a feed with unread items 
   in two passes. In the first pass it tries to find one
   after the currently selected feed (including the
   selected feed). If there are no such feeds the 
   search is restarted for all feeds. */
static nodePtr feedlist_unread_scan(nodePtr folder) {
	nodePtr		childNode, selectedNode;
	GSList		*selectedIter = NULL;

	if(NULL != (selectedNode = feedlist_get_selected()))
		selectedIter = g_slist_find(selectedNode->parent->children, selectedNode);
	else
		scanState = UNREAD_SCAN_SECOND_PASS;

	GSList *iter = folder->children;
	while(iter) {
		nodePtr node = iter->data;

		if(node == selectedNode)
			scanState = UNREAD_SCAN_FOUND_SELECTED;

		/* feed match if beyond the selected feed or in second pass... */
		if((scanState != UNREAD_SCAN_INIT) && (node->unreadCount > 0) &&
		   (NULL == node->children) && (NODE_TYPE_VFOLDER != node->type)) {
		       return node;
		}

		/* folder traversal if we are searching the selected feed
		   which might be a descendant of the folder and if we
		   are beyond the selected feed and the folder contains
		   feeds with unread items... */
		if(node->children &&
		   (((scanState != UNREAD_SCAN_INIT) && (node->unreadCount > 0)) ||
		    (selectedIter && (node_is_ancestor(node, selectedNode))))) {
		       if(NULL != (childNode = feedlist_unread_scan(node)))
				return childNode;
		}

		iter = g_slist_next(iter);
	}

	/* When we come here we didn't find anything from the selected
	   feed down to the end of the feed list. */
	if(folder == feedlist_get_root()) {
		if(0 == feedlist_get_unread_item_count()) {
			/* this may mean there is nothing more to find */
		} else {
			/* or that there are unread items above the selected feed */
			g_assert(scanState != UNREAD_SCAN_SECOND_PASS);
			scanState = UNREAD_SCAN_SECOND_PASS;
			return feedlist_unread_scan(feedlist_get_root());
		}
	}

	return NULL;
}

nodePtr feedlist_find_unread_feed(nodePtr folder) {

	scanState = UNREAD_SCAN_INIT;
	return feedlist_unread_scan(folder);
}

/* selection handling */

void feedlist_selection_changed(nodePtr node) {
	nodePtr	displayed_node;

	debug_enter("feedlist_selection_changed");

	debug1(DEBUG_GUI, "new selected node: %s", node?node_get_title(node):"none");
	if(node != selectedNode) {
		displayed_node = itemlist_get_displayed_node();

		/* When the user selects a feed in the feed list we
		   assume that he got notified of the new items or
		   isn't interested in the event anymore... */
		if(0 != newCount)
			feedlist_reset_new_item_count();

		script_run_for_hook(SCRIPT_HOOK_FEED_UNSELECT);

		/* Unload visible items. */
		itemlist_unload(TRUE);

		/* Unload previously displayed node. */
		if(displayed_node)
			node_unload(displayed_node);
			
		/* Load items of new selected node. */
		selectedNode = node;
		if(selectedNode) {
			node_load(selectedNode);
			itemlist_set_view_mode(node_get_view_mode(selectedNode));
			itemlist_load(selectedNode->itemSet);
		} else {
			ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
		}
		
		script_run_for_hook(SCRIPT_HOOK_FEED_SELECTED);
	}

	debug_exit("feedlist_selection_changed");
}

/* menu callbacks */

void on_menu_delete(GtkWidget *widget, gpointer user_data) {
	ui_feedlist_delete_prompt(selectedNode);
}

void on_menu_update(GtkWidget *widget, gpointer user_data) {

	if(!selectedNode) {
		ui_show_error_box(_("You have to select a feed entry"));
		return;
	}

	if(update_is_online()) 
		node_request_update(selectedNode, FEED_REQ_PRIORITY_HIGH);
	else
		ui_mainwindow_set_status_bar(_("Liferea is in offline mode. No update possible."));
}

void on_menu_update_all(GtkWidget *widget, gpointer user_data) { 

	if(update_is_online()) 
		// FIXME: int -> pointer
		feedlist_foreach_data(node_request_update, (gpointer)FEED_REQ_PRIORITY_HIGH);
	else
		ui_mainwindow_set_status_bar(_("Liferea is in offline mode. No update possible."));
}

void on_menu_allread(GtkWidget *widget, gpointer user_data) {
	
	if(selectedNode)
		node_mark_all_read(selectedNode);
}

void on_menu_allfeedsread(GtkWidget *widget, gpointer user_data) {
	node_foreach_child(feedlist_get_root(), node_mark_all_read);
}

/* feedlist saving */

static gboolean feedlist_schedule_save_cb(gpointer user_data) {

	/* step 1: request each node to save its state */
	feedlist_foreach(node_save);

	/* step 2: request saving for the root node and thereby
	   forcing the root plugin to save the feed list structure */
	NODE_SOURCE_TYPE(rootNode)->source_export(rootNode);
	
	feedlist_save_timer = 0;
	return FALSE;
}

void feedlist_schedule_save(void) {

	/* By waiting here 5s and checking feedlist_save_time
	   we hope to catch bulks of feed list changes and save 
	   less often */
	if(!feedlistLoading && !feedlist_save_timer) {
		debug0(DEBUG_CONF, "Scheduling feedlist save");
		feedlist_save_timer = g_timeout_add(5000, feedlist_schedule_save_cb, NULL);
	}
}

/* Executes immediate save */
void feedlist_save(void) {

	feedlist_schedule_save_cb(NULL);
}

/* This method is needed to update the vfolder count
   in the GUI after the initial feed loading is done. */
static void feedlist_update_vfolder_count(nodePtr node) {

	if(NODE_TYPE_VFOLDER == node->type) 
		ui_node_update(node);
}

/* This method is used to initially set the expansion
   state of all nodes in the feed list */
static void feedlist_expand_folder(nodePtr node) {

	if(node->expanded)
		ui_node_set_expansion(node, TRUE);
}

void feedlist_init(void) {

	debug_enter("feedlist_init");
	
	/* 1. Register standard node and source types */
	node_type_register(feed_get_node_type());
	node_type_register(root_get_node_type());
	node_type_register(folder_get_node_type());
	node_type_register(vfolder_get_node_type());
	node_type_register(node_source_get_node_type());
	node_type_register(newsbin_get_node_type());
	
	node_source_type_register(default_source_get_type());
	node_source_type_register(dummy_source_get_type());
	node_source_type_register(opml_source_get_type());

	/* 2. Set up a root node and import the feed list plugins structure. */
	rootNode = node_source_setup_root();

	/* 3. Sequentially load and unload all feeds and by doing so 
	   automatically load all vfolders */
	feedlist_foreach(node_initial_load);
	feedlist_foreach(feedlist_update_vfolder_count);
	feedlist_foreach(feedlist_expand_folder);
	
	notification_enable(getBooleanConfValue(SHOW_POPUP_WINDOWS));

	/* 4. Check if feeds do need updating. */
	switch(getNumericConfValue(STARTUP_FEED_ACTION)) {
		case 1: /* Update all feeds */
			debug0(DEBUG_UPDATE, "initial update: updating all feeds");
			// FIXME: int -> gpointer
			feedlist_foreach_data(node_request_update, 0);
			break;
		case 2:
			debug0(DEBUG_UPDATE, "initial update: resetting feed counter");
			feedlist_foreach(node_reset_update_counter);
			break;
		default:
			debug0(DEBUG_UPDATE, "initial update: using auto update");
			/* default, which is to use the lastPoll times, does not need any actions here. */;
	}

	/* 5. Start automatic updating */
 	(void)g_timeout_add(1000, feedlist_auto_update, NULL);

	/* 6. Finally save the new feed list state */
	feedlistLoading = FALSE;
	feedlist_schedule_save();
	
	debug_exit("feedlist_init");
}

