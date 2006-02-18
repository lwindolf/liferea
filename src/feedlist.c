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
#include "node.h"
#include "itemlist.h"
#include "update.h"
#include "conf.h"
#include "debug.h"
#include "callbacks.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_notification.h"
#include "ui/ui_tray.h"
#include "ui/ui_feed.h"
#include "ui/ui_node.h"
#include "fl_providers/fl_plugin.h"

static guint unreadCount = 0;
static guint newCount = 0;

static nodePtr	rootNode = NULL;

/* selectedNode matches the node selected in the feed list tree view, which
   is not necessarily the displayed one (e.g. folders without recursive
   display enabled) */
static nodePtr	selectedNode = NULL; 		

static guint feedlist_save_timer = 0;
static gboolean feedlistLoading = TRUE;

typedef enum {
	NEW_ITEM_COUNT,
	UNREAD_ITEM_COUNT
} countType;

nodePtr feedlist_get_root(void) { return rootNode; }

nodePtr feedlist_get_selected(void) { return selectedNode; }

nodePtr feedlist_get_selected_parent(void) { 

	g_assert(NULL != rootNode);

	if(NULL == selectedNode)
		return rootNode;
	
	if(NULL == selectedNode->parent) 
		return rootNode;
	else
		return selectedNode->parent;
}

/* statistic handling methods */

int feedlist_get_unread_item_count(void) { return unreadCount; }
int feedlist_get_new_item_count(void) { return newCount; }

void feedlist_update_counters(gint unreadDiff, gint newDiff) {

	unreadCount += unreadDiff;
	newCount += newDiff;

	if((0 != newDiff) || (0 != unreadDiff))
		ui_tray_update();
}

static void feedlist_unset_new_items(nodePtr node) {
	
	if(0 != node->newCount) {
		node_load(node);
		itemset_mark_all_old(node->itemSet);
		node_unload(node);
	}
}

void feedlist_reset_new_item_count(void) {

	if(0 != newCount) {
		feedlist_foreach(feedlist_unset_new_items);
		newCount = 0;
		ui_tray_update();
	}
}

void feedlist_add_node(nodePtr parent, nodePtr node, gint position) {

	parent->children = g_slist_append(parent->children, node);
	ui_node_add(parent, node, position);	
	ui_node_update(node);
}

void feedlist_remove_node(nodePtr np) {

	debug_enter("feedlist_remove_node");

	if(np == selectedNode) {
		ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
		itemlist_unload();
		ui_feedlist_select(NULL);
	}

	node_remove(np);

	debug_exit("feedlist_remove_node");
}

/* This callback is used to compute the itemset of folder nodes */
static void feedlist_merge_itemset_cb(nodePtr node, gpointer userdata) {
	itemSetPtr	sp = (itemSetPtr)userdata;

	debug1(DEBUG_GUI, "merging items of node \"%s\"", node_get_title(node));

	switch(node->type) {
		case FST_FOLDER:
			node_foreach_child_data(node, feedlist_merge_itemset_cb, userdata);
			break;
		case FST_FEED:
		case FST_PLUGIN:
			node_load(node);
			break;
		case FST_VFOLDER:
			/* Do not merge vfolders because this might
			   cause duplicate items and very large itemsets very. */
			return;
			break;
		default:
			g_warning("internal error: unknown node type!");
			return;
			break;
	}

	debug1(DEBUG_GUI, "   pre merge item set: %d items", g_list_length(sp->items));
	/* Don't use a direct g_list_concat() here it will freeze the program! (Lars) */
	// FIXME: is this a memleak?
	sp->items = g_list_concat(sp->items, g_list_copy(node->itemSet->items));
	debug1(DEBUG_GUI, "  post merge item set: %d items", g_list_length(sp->items));
}

static gboolean feedlist_auto_update(void *data) {

	debug_enter("feedlist_auto_update");

	if(download_is_online())
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
	nodePtr			np, childNode, selectedNode;
	GSList			*iter, *selectedIter = NULL;

	if(NULL != (selectedNode = feedlist_get_selected())) {
		selectedIter = g_slist_find(selectedNode->parent->children, selectedNode);
	} else {
		scanState = UNREAD_SCAN_SECOND_PASS;
	}

	iter = folder->children;
	while(iter) {
		np = iter->data;

		if(np == selectedNode)
			scanState = UNREAD_SCAN_FOUND_SELECTED;

		/* feed match if beyond the selected feed or in second pass... */
		if((scanState != UNREAD_SCAN_INIT) && (np->unreadCount > 0) &&
		   ((FST_FEED == np->type) || (FST_PLUGIN == np->type))) {
		       return np;
		}

		/* folder traversal if we are searching the selected feed
		   which might be a descendant of the folder and if we
		   are beyond the selected feed and the folder contains
		   feeds with unread items... */
		if((FST_FOLDER == np->type) &&
		   (((scanState != UNREAD_SCAN_INIT) && (np->unreadCount > 0)) ||
		    ((NULL != selectedIter) && (node_is_ancestor(np, selectedNode))))) {
		       if(NULL != (childNode = feedlist_unread_scan(np)))
				return childNode;
		} /* Directories are never checked */

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
			childNode = feedlist_unread_scan(feedlist_get_root());
			return childNode;
		}
	}

	return NULL;
}

nodePtr feedlist_find_unread_feed(nodePtr folder) {

	scanState = UNREAD_SCAN_INIT;
	return feedlist_unread_scan(folder);
}

/* selection handling */

void feedlist_selection_changed(nodePtr np) {
	nodePtr	displayed_node;

	debug_enter("feedlist_selection_changed");

	debug1(DEBUG_GUI, "new selected node: %s", (NULL == np)?"none":node_get_title(np));
	if(np != selectedNode) {
		displayed_node = itemlist_get_displayed_node();

		/* When the user selects a feed in the feed list we
		   assume that he got notified of the new items or
		   isn't interested in the event anymore... */
		if(0 != newCount)
			feedlist_reset_new_item_count();

		/* Unload visible items. */
		itemlist_unload();

		/* Unload previously displayed node. */
		if(NULL != displayed_node)
			node_unload(displayed_node);

		/* Load items of new selected node. */
		selectedNode = np;

		node_load(selectedNode);
		itemlist_load(selectedNode->itemSet);
	}

	debug_exit("feedlist_selection_changed");
}

void on_menu_delete(GtkMenuItem *menuitem, gpointer user_data) {

	ui_feedlist_delete_prompt(selectedNode);
}

void on_popup_refresh_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	nodePtr node = (nodePtr)callback_data;

	if(!node) {
		ui_show_error_box(_("You have to select a feed entry"));
		return;
	}

	if(download_is_online()) 
		node_request_update(node, FEED_REQ_PRIORITY_HIGH);
	else
		ui_mainwindow_set_status_bar(_("Liferea is in offline mode. No update possible."));
}

void on_refreshbtn_clicked(GtkButton *button, gpointer user_data) { 

	if(download_is_online()) 
		node_request_update(feedlist_get_root(), FEED_REQ_PRIORITY_HIGH);
	else
		ui_mainwindow_set_status_bar(_("Liferea is in offline mode. No update possible."));
}

void on_menu_feed_update(GtkMenuItem *menuitem, gpointer user_data) {

	on_popup_refresh_selected((gpointer)selectedNode, 0, NULL);
}

void on_menu_update(GtkMenuItem *menuitem, gpointer user_data) {
	
	if(selectedNode != NULL)
		on_popup_refresh_selected((gpointer)selectedNode, 0, NULL);
	else
		g_warning("You have found a bug in Liferea. You must select a node in the feedlist to do what you just did.");
}

void on_popup_allread_selected(void) {
	
	if(selectedNode)
		node_mark_all_read(selectedNode);
}

void on_popup_allfeedsread_selected(void) {
g_print("all feeds read selected\n");
	node_foreach_child(feedlist_get_root(), node_mark_all_read);
}

void on_popup_mark_as_read(gpointer callback_data, guint callback_action, GtkWidget *widget) {

	on_popup_allread_selected();
}

void on_popup_delete(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	
	ui_feedlist_delete_prompt((nodePtr)callback_data);
}

static gboolean feedlist_schedule_save_cb(gpointer user_data) {

	/* step 1: request each node to save its state */
	feedlist_foreach(node_save);

	/* step 2: request saving for the root node and thereby
	   forcing the root plugin to save the feed list structure */
	FL_PLUGIN(rootNode)->node_save(rootNode);
	
	feedlist_save_timer = 0;
	return FALSE;
}

void feedlist_schedule_save(void) {

	if(!feedlistLoading && !feedlist_save_timer) {
		debug0(DEBUG_CONF, "Scheduling feedlist save");
		feedlist_save_timer = g_timeout_add(5000, feedlist_schedule_save_cb, NULL);
	}
}

void feedlist_save(void) {

	feedlist_schedule_save_cb(NULL);
}

/**
 * Used to process feeds directly after feed list loading.
 * Loads the given feed or requests a download. During feed
 * loading its items are automatically checked against all 
 * vfolder rules.
 */
static void feedlist_initial_load(nodePtr np) {
	
	if(FST_VFOLDER != np->type) {
		node_load(np);
		vfolder_check_node(np);		/* copy items to matching vfolders */
		node_unload(np);
	}
}

static void feedlist_initial_update(nodePtr np) {

	node_request_update(np, 0);
}

void feedlist_init(void) {
	flPluginInfo	*rootPlugin;

	debug_enter("feedlist_init");

	/* 1. Set up a root node */
	rootNode = node_new();
	rootNode->isRoot = TRUE;
	rootNode->title = g_strdup("root");

	/* 2. Initialize list of plugins and find root provider
	   plugin. Creating an instance of this plugin. This 
	   will load the feed list and all attached plugin 
	   handlers. */
	rootPlugin = fl_plugins_get_root();
	rootPlugin->node_load(rootNode);

	/* 3. Sequentially load and unload all feeds and by doing so 
	   automatically load all vfolders */
	feedlist_foreach(feedlist_initial_load);
	feedlist_foreach(ui_node_update);

	/* 4. Check if feeds do need updating. */
	switch(getNumericConfValue(STARTUP_FEED_ACTION)) {
		case 1: /* Update all feeds */
			debug0(DEBUG_UPDATE, "initial update: updating all feeds");
			feedlist_foreach(feedlist_initial_update);
			break;
		case 2:
			debug0(DEBUG_UPDATE, "initial update: resetting feed counter");
			feedlist_foreach(feed_reset_update_counter);
			break;
		default:
			debug0(DEBUG_UPDATE, "initial update: using auto update");
			/* default, which is to use the lastPoll times, does not need any actions here. */;
	}

	/* 5. Start automatic updating */
 	(void)g_timeout_add(1000, feedlist_auto_update, NULL);
	
	debug_exit("feedlist_init");
}

void feedlist_foreach_full(nodePtr node, gpointer func, gint params, gpointer user_data) {
	
	if(!node)
		node = feedlist_get_root();

	GSList *iter = node->children;
	while(iter) {
		nodePtr childNode = (nodePtr)iter->data;
		
		/* Apply the method to the child */
		if(0 == params)
			((nodeActionFunc)func)(childNode);
		else 
			((nodeActionDataFunc)func)(childNode, user_data);
			
		/* if the iter has children descend */
		if(childNode->children)
			feedlist_foreach_full(childNode, func, params, user_data);

		iter = g_slist_next(iter);
	}
}
