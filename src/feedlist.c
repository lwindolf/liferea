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

static nodePtr	selectedNode = NULL;
extern nodePtr	displayed_node;

static flPluginInfo * rootPlugin = NULL;
static GSList * plugins = NULL;

static guint feedlist_save_timer = 0;
static gboolean feedlistLoading = TRUE;

/* helper functions */

typedef enum {
	NEW_ITEM_COUNT,
	UNREAD_ITEM_COUNT
} countType;

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
	
	if(0 == np->itemSet->newCount)
		return;
		
	node_load(np);
	itemset_mark_all_old(np->itemSet);
	node_unload(np);
}

void feedlist_reset_new_item_count(void) {

	if(0 != newCount) {
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, feedlist_unset_new_items);
		newCount = 0;
		ui_tray_update();
	}
}

void feedlist_add_node(nodePtr parent, nodePtr np, gint position) {

	ui_feedlist_add(parent, np, position);
	ui_feedlist_update();
}

void feedlist_update_node(nodePtr np) {

	if(download_is_online()) 
		node_request_update(np, FEED_REQ_PRIORITY_HIGH);
	else
		ui_mainwindow_set_status_bar(_("Liferea is in offline mode. No update possible."));
}

static void feedlist_remove_node_(nodePtr np) { 
	
	// FIXME: feedPtr!!!
	//ui_notification_remove_feed((feedPtr)np);	/* removes an existing notification for this feed */
	ui_node_remove_node(np);
	ui_feedlist_update();
	
	node_load(np);
	node_remove(np);	
}

static void feedlist_remove_folder(nodePtr np) {

	ui_feedlist_do_for_all(np, ACTION_FILTER_CHILDREN | ACTION_FILTER_ANY, feedlist_remove_node_);
	ui_feedlist_update();
	node_remove(np);	
}

void feedlist_remove_node(nodePtr np) {

	if(np == displayed_node) {
		itemlist_load(NULL);
		ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
	}

	if(FST_FOLDER != np->type)
		feedlist_remove_node_(np);
	else
		feedlist_remove_folder(np);
}

static void feedlist_merge_itemset_cb(nodePtr np, gpointer userdata) {
	itemSetPtr sp = (itemSetPtr)userdata;

	switch(np->type) {
		case FST_FOLDER:
			return; /* a sub folder has no own itemset to add */
			break;
		case FST_FEED:
		case FST_PLUGIN:
			if(NULL != FL_PLUGIN(np)->node_load)
				FL_PLUGIN(np)->node_load(np);
			break;
		case FST_VFOLDER:
			/* FIXME */		
			return;
			break;
		default:
			g_warning("internal error: unknown node type!");
			return;
			break;
	}

	sp->items = g_list_concat(sp->items, np->itemSet->items);
	sp->newCount += np->itemSet->newCount;
	sp->unreadCount += np->itemSet->unreadCount;
}

void feedlist_load_node(nodePtr np) {

	node_load(np);

	if(FST_FOLDER == np->type)
		ui_feedlist_do_for_all_data(np, ACTION_FILTER_FEED, feedlist_merge_itemset_cb, (gpointer)&(np->itemSet));
}

void feedlist_unload_node(nodePtr np) {

	node_unload(np);

	if(FST_FOLDER == np->type)
		ui_feedlist_do_for_all(np, ACTION_FILTER_FEED, node_unload);
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
		feedlist_reset_new_item_count();
	}
}

void on_menu_delete(GtkMenuItem *menuitem, gpointer user_data) {

	ui_feedlist_delete_prompt((nodePtr)ui_feedlist_get_selected());
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

	on_popup_refresh_selected((gpointer)ui_feedlist_get_selected(), 0, NULL);
}

void on_menu_update(GtkMenuItem *menuitem, gpointer user_data) {
	gpointer np = (gpointer)ui_feedlist_get_selected();
	
	if(np != NULL)
		on_popup_refresh_selected((gpointer)np, 0, NULL);
	else
		g_warning("You have found a bug in Liferea. You must select a node in the feedlist to do what you just did.");
}

static void feedlist_mark_all_read(nodePtr np) {

	if(0 == np->itemSet->unreadCount)
		return;

	node_load(np);
	itemset_mark_all_read(np->itemSet);
	node_unload(np);
}

void on_popup_allunread_selected(void) {
	nodePtr	np;
	
	if(NULL != (np = ui_feedlist_get_selected())) {
		if(FST_FOLDER == np->type) {
			/* if we have selected a folder we mark all item of all feeds as read */
			ui_feedlist_do_for_all(np, ACTION_FILTER_FEED, (nodeActionFunc)feedlist_mark_all_read);
		} else {
			feedlist_mark_all_read(np);
		}
		ui_feedlist_update();
	}
}

static void feedlist_mark_all_read_cb(nodePtr np) {

	itemset_mark_all_read(np->itemSet);
}

void on_popup_allfeedsunread_selected(void) {

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, feedlist_mark_all_read_cb);
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
	rootPlugin->node_save(NULL);
	
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

void feedlist_init(void) {

	debug_enter("feedlist_init");

	/* Initialize list of plugins and find root provider
	   plugin. Creating an instance of this plugin. This 
	   will load the feed list and all attached plugin 
	   handlers. */
	plugins = plugin_mgmt_get_list();
	rootPlugin = fl_plugins_get_root(plugins);
	rootPlugin->handler_new(NULL);

	/* initial update of feed list */
	feedlist_auto_update(NULL);

	/* setup one minute timer for automatic updating */
 	(void)g_timeout_add(60*1000, feedlist_auto_update, NULL);

	debug_exit("feedlist_init");
}
