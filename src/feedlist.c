/**
 * @file feedlist.c feedlist handling
 *
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include "item.h"
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
static nodePtr	selectedNode = NULL;
extern nodePtr	displayed_node;

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

static void feedlist_unset_new_items(nodePtr np) {
	
	node_load(np);
	itemset_mark_all_old(np->itemSet);
	node_unload(np);
}

void feedlist_reset_new_item_count(void) {

	if(0 != newCount) {
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, feedlist_unset_new_items);
		newCount = 0;
		ui_tray_update();
	}
}

void feedlist_add_node(nodePtr parent, nodePtr np, gint position) {

	ui_feedlist_add(parent, np, position);	// FIXME: should be ui_node_add_child()
	if(NULL != parent)
		ui_node_update(parent);
}

void feedlist_update_node(nodePtr np) {

	if(download_is_online()) 
		node_request_update(np, FEED_REQ_PRIORITY_HIGH);
	else
		ui_mainwindow_set_status_bar(_("Liferea is in offline mode. No update possible."));
}

static void feedlist_remove_node_(nodePtr np) { 
	
	debug_enter("feedlist_remove_node_");

	ui_notification_remove_feed(np);	/* removes an existing notification for this feed */
	ui_node_remove_node(np);
	node_remove(np);

	debug_exit("feedlist_remove_node_");
}

static void feedlist_remove_folder(nodePtr np) {

	debug_enter("feedlist_remove_folder");

	ui_feedlist_do_for_all(np, ACTION_FILTER_CHILDREN | ACTION_FILTER_ANY, feedlist_remove_node_);
	ui_node_remove_node(np);
	node_remove(np);	

	debug_exit("feedlist_remove_folder");
}

void feedlist_remove_node(nodePtr np) {

	debug_enter("feedlist_remove_node");

	if(np == selectedNode) {
		ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
		itemlist_load(NULL);
		selectedNode = NULL;
	}

	if(FST_FOLDER != np->type)
		feedlist_remove_node_(np);
	else
		feedlist_remove_folder(np);

	debug_exit("feedlist_remove_node");
}

/* This callback is used to compute the itemset of folder nodes */
static void feedlist_merge_itemset_cb(nodePtr np, gpointer userdata) {
	itemSetPtr	sp = (itemSetPtr)userdata;

	debug1(DEBUG_GUI, "merging items of node \"%s\"", node_get_title(np));

	switch(np->type) {
		case FST_FOLDER:
			return; /* a sub folder has no own itemset to add */
			break;
		case FST_FEED:
		case FST_PLUGIN:
			node_load(np);
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
	sp->items = g_list_concat(sp->items, np->itemSet->items);
	debug1(DEBUG_GUI, "  post merge item set: %d items", g_list_length(sp->items));
}

void feedlist_load_node(nodePtr np) {

	if(FST_FOLDER == np->type) {
		g_assert(NULL != np->itemSet);
		ui_feedlist_do_for_all_data(np, ACTION_FILTER_FEED, feedlist_merge_itemset_cb, (gpointer)np->itemSet);
	} else {
		node_load(np);
	}
}

void feedlist_unload_node(nodePtr np) {

	if(FST_FOLDER == np->type) {
g_print(">>>>>>>< freeing folder \n");
		g_list_free(np->itemSet->items);
		np->itemSet->items = NULL;
		ui_feedlist_do_for_all(np, ACTION_FILTER_FEED, node_unload);
	} else {
		node_unload(np);
	}
}

static gboolean feedlist_auto_update(void *data) {

	debug_enter("feedlist_auto_update");

	if(download_is_online())
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (gpointer)node_request_auto_update);
	else
		debug0(DEBUG_UPDATE, "no update processing because we are offline!");
	
	debug_exit("feedlist_auto_update");

	return TRUE;
}

/* direct user callbacks */

void feedlist_selection_changed(nodePtr np) {

	if(np != selectedNode) {
		selectedNode = np;
	
		/* when the user selects a feed in the feed list we
		   assume that he got notified of the new items or
		   isn't interested in the event anymore... */
		if(0 != newCount)
			feedlist_reset_new_item_count();
	}
}

void on_menu_delete(GtkMenuItem *menuitem, gpointer user_data) {

	ui_feedlist_delete_prompt(selectedNode);
}

void on_popup_refresh_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	nodePtr np = (nodePtr)callback_data;

	if(np == NULL) {
		ui_show_error_box(_("You have to select a feed entry"));
		return;
	}

	ui_feedlist_do_for_all(np, ACTION_FILTER_FEED, feedlist_update_node);
}

void on_refreshbtn_clicked(GtkButton *button, gpointer user_data) { 

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, feedlist_update_node);
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

static void feedlist_mark_all_read(nodePtr np) {

	if(0 == np->unreadCount)
		return;

	node_load(np);
	itemlist_mark_all_read(np);
	ui_node_update(np);
	node_unload(np);
}

void on_popup_allunread_selected(void) {
	nodePtr	np;
	
	if(NULL != selectedNode) {
		if(FST_FOLDER == selectedNode->type) {
			/* if we have selected a folder we mark all item of all feeds as read */
			ui_feedlist_do_for_all(selectedNode, ACTION_FILTER_FEED, (nodeActionFunc)feedlist_mark_all_read);
		} else {
			feedlist_mark_all_read(selectedNode);
		}
	}
}

void on_popup_allfeedsunread_selected(void) {

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, feedlist_mark_all_read);
}

void on_popup_mark_as_read(gpointer callback_data, guint callback_action, GtkWidget *widget) {

	on_popup_allunread_selected();
}

void on_popup_delete(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	
	ui_feedlist_delete_prompt((nodePtr)callback_data);
}

static gboolean feedlist_schedule_save_cb(gpointer user_data) {

	/* step 1: request each feed list plugin to save its state */
	ui_feedlist_do_for_all(NULL, ACTION_FILTER_PLUGIN, node_save);

	/* step 2: request saving for the root plugin and thereby
	   saving the feed list structure */
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
	
	feedlist_load_node(np);
	ui_node_update(np);
	feedlist_unload_node(np);
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
	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, feedlist_initial_load);

	/* 4. Check if feeds do need updating. */
	switch(getNumericConfValue(STARTUP_FEED_ACTION)) {
		case 1: /* Update all feeds */
			debug0(DEBUG_UPDATE, "initial update: updating all feeds");
			ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (gpointer)feedlist_initial_update);
			break;
		case 2:
			debug0(DEBUG_UPDATE, "initial update: resetting feed counter");
			ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (gpointer)feed_reset_update_counter);
			break;
		default:
			debug0(DEBUG_UPDATE, "initial update: using auto update");
			/* default, which is to use the lastPoll times, does not need any actions here. */;
	}

	/* 5. Start automatic updating */
 	//(void)g_timeout_add(1000, feedlist_auto_update, NULL);
	
	debug_exit("feedlist_init");
}
