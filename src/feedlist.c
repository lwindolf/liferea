/**
 * @file feedlist.c feedlist handling
 *
 * Copyright (C) 2005-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "itemlist.h"
#include "itemview.h"
#include "newsbin.h"
#include "node.h"
#include "script.h"
#include "update.h"
#include "vfolder.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"
#include "ui/ui_subscription.h"
#include "ui/ui_tray.h"
#include "fl_sources/bloglines_source.h"
#include "fl_sources/default_source.h"
#include "fl_sources/dummy_source.h"
#include "fl_sources/google_source.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"
#include "notification/notif_plugin.h"

static guint newCount = 0;

static nodePtr	rootNode = NULL;

/* selectedNode matches the node selected in the feed list tree view, which
   is not necessarily the displayed one (e.g. folders without recursive
   display enabled) */
static nodePtr	selectedNode = NULL; 		

/** set when a feed list save is scheduled */
static guint feedlist_save_timer = 0;

/** this flag prevents the feed list being saved before it is completely loaded */
static gboolean feedlistLoading = TRUE;

/** flag is set when any cache migration was done on startup */
gboolean cacheMigrated = FALSE;

static void feedlist_unselect(void);

nodePtr feedlist_get_root(void) { return rootNode; }

nodePtr feedlist_get_selected(void) { return selectedNode; }

nodePtr
feedlist_get_insertion_point (void)
{ 

	g_assert (NULL != rootNode);

	if (!selectedNode)
		return rootNode;
	
	if (IS_FOLDER (selectedNode))
		return selectedNode;
	
	if (selectedNode->parent) 
		return selectedNode->parent;
	else
		return rootNode;
}

static void
feedlist_update_node_counters (nodePtr node)
{
	if (node->needsRecount) {
		if (IS_VFOLDER (node))
			vfolder_update_counters (node);	/* simple vfolder only update */
		else
			node_update_counters (node);	/* update with parent propagation */
	}
	if (node->needsUpdate)
		ui_node_update (node->id);
	if (node->children)
		node_foreach_child (node, feedlist_update_node_counters);
}

void
feedlist_mark_all_read (nodePtr node)
{
	feedlist_reset_new_item_count ();

	if (node != feedlist_get_root ())
		node_mark_all_read (node);
	else 
		node_foreach_child (feedlist_get_root (), node_mark_all_read);
		
	feedlist_foreach (feedlist_update_node_counters);
	itemview_update_all_items ();
	itemview_update ();
}

/* statistic handling methods */

guint
feedlist_get_unread_item_count (void)
{
	if (!rootNode)
		return 0;
		
	return (rootNode->unreadCount > 0)?rootNode->unreadCount:0;
}

guint
feedlist_get_new_item_count (void)
{
	return (newCount > 0)?newCount:0;
}

static void
feedlist_unset_new_items (nodePtr node)
{	
	if (0 != node->newCount)
		item_state_set_all_old (node->id);
	
	node_foreach_child (node, feedlist_unset_new_items);
}

static void
feedlist_update_new_item_count (guint addValue)
{
	newCount += addValue;
	ui_tray_update ();
	ui_mainwindow_update_feedsinfo ();
}

void
feedlist_reset_new_item_count (void)
{
	if (newCount) {
		feedlist_foreach (feedlist_unset_new_items);
		newCount = 0;
		ui_tray_update ();
		ui_mainwindow_update_feedsinfo ();
	}
}

void
feedlist_node_was_updated (nodePtr node, guint newCount)
{
	vfolder_foreach (vfolder_update_counters);			
	node_update_counters (node);
	feedlist_update_new_item_count (newCount);
}

void feedlist_remove_node(nodePtr node) {

	debug_enter("feedlist_remove_node");

	if(node == selectedNode)
		feedlist_unselect();

	node_request_remove(node);

	debug_exit("feedlist_remove_node");
}

static gboolean
feedlist_auto_update (void *data)
{
	GTimeVal now;
	
	debug_enter ("feedlist_auto_update");

	if (update_is_online ())
		node_auto_update_subscription (feedlist_get_root ());
	else
		debug0 (DEBUG_UPDATE, "no update processing because we are offline!");
	
	debug_exit ("feedlist_auto_update");

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
		   (NULL == node->children) && !IS_VFOLDER(node)) {
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

static void
feedlist_unselect (void)
{
	selectedNode = NULL;

	itemview_set_displayed_node (NULL);
	itemview_update ();
		
	itemlist_unload (FALSE /* mark all read */);
	ui_feedlist_select (NULL);
	ui_mainwindow_update_feed_menu (FALSE, FALSE);
}

void
feedlist_selection_changed (nodePtr node)
{
	debug_enter ("feedlist_selection_changed");

	debug1 (DEBUG_GUI, "new selected node: %s", node?node_get_title (node):"none");
	if (node != selectedNode) {

		/* When the user selects a feed in the feed list we
		   assume that he got notified of the new items or
		   isn't interested in the event anymore... */
		if (0 != newCount)
			feedlist_reset_new_item_count ();

		script_run_for_hook (SCRIPT_HOOK_FEED_UNSELECT);

		/* Unload visible items. */
		itemlist_unload (TRUE);
	
		/* Load items of new selected node. */
		selectedNode = node;
		if (selectedNode) {
			itemlist_set_view_mode (node_get_view_mode (selectedNode));		
			itemlist_load (selectedNode);
		} else {
			liferea_htmlview_clear (ui_mainwindow_get_active_htmlview ());
		}
		
		if (selectedNode)
			script_run_for_hook (SCRIPT_HOOK_FEED_SELECTED);
	}

	debug_exit ("feedlist_selection_changed");
}

/* menu callbacks */

void on_menu_delete(GtkWidget *widget, gpointer user_data) {
	ui_feedlist_delete_prompt(selectedNode);
}

void
on_menu_update(GtkWidget *widget, gpointer user_data)
{
	if (!selectedNode) {
		ui_show_error_box(_("You have to select a feed entry"));
		return;
	}

	if (update_is_online ()) 
		node_update_subscription (selectedNode, GUINT_TO_POINTER (FEED_REQ_PRIORITY_HIGH));
	else
		ui_mainwindow_set_status_bar (_("Liferea is in offline mode. No update possible."));
}

void
on_menu_update_all(GtkWidget *widget, gpointer user_data)
{ 
	if (update_is_online ()) 
		node_update_subscription (feedlist_get_root(), GUINT_TO_POINTER (FEED_REQ_PRIORITY_HIGH));
	else
		ui_mainwindow_set_status_bar (_("Liferea is in offline mode. No update possible."));
}

void
on_menu_allread (GtkWidget *widget, gpointer user_data)
{	
	feedlist_mark_all_read (selectedNode);
}

void
on_menu_allfeedsread (GtkWidget *widget, gpointer user_data)
{
	feedlist_mark_all_read (feedlist_get_root ());
}

/* Feedlist saving. Do not call directly to avoid threading 
   problems. Use feedlist_schedule_save() instead! */
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

/* This method is only to be used when exiting the program! */
void feedlist_save(void) {

	feedlist_schedule_save_cb(NULL);
}

void
feedlist_reset_update_counters (nodePtr node) 
{
	GTimeVal now;
	
	if (NULL == node)
		node = feedlist_get_root ();	
	
	g_get_current_time (&now);
	node_foreach_child_data (node, node_reset_update_counter, &now);
}

/* This method is used to initially the node states in the feed list */
static void
feedlist_init_node (nodePtr node) 
{
	if (node->expanded)
		ui_node_set_expansion (node, TRUE);
	
	if (node->subscription)
		db_subscription_load (node->subscription);
		
	node_update_counters (node);
		
	node_foreach_child (node, feedlist_init_node);
}

void feedlist_init(void) {

	debug_enter("feedlist_init");
	
	/* 1. Register standard node and source types */
	node_type_register (feed_get_node_type ());
	node_type_register (root_get_node_type ());
	node_type_register (folder_get_node_type ());
	node_type_register (vfolder_get_node_type ());
	node_type_register (node_source_get_node_type ());
	node_type_register (newsbin_get_node_type ());
	
	node_source_type_register (default_source_get_type ());
	node_source_type_register (dummy_source_get_type ());
	node_source_type_register (opml_source_get_type ());
	node_source_type_register (bloglines_source_get_type ());
	node_source_type_register (google_source_get_type ());

	/* 2. Set up a root node and import the feed list plugins structure. */
	debug0(DEBUG_CACHE, "Setting up root node");
	rootNode = node_source_setup_root();

	/* 3. Ensure folder expansion and unread count*/
	debug0(DEBUG_CACHE, "Initializing node state");
	feedlist_foreach (feedlist_init_node);

	debug0 (DEBUG_GUI, "Notification setup");	
	notification_enable (conf_get_bool_value (SHOW_POPUP_WINDOWS));
	ui_tray_update ();

	/* 4. Check if feeds do need updating. */
	debug0(DEBUG_UPDATE, "Performing initial feed update");
	switch(getNumericConfValue(STARTUP_FEED_ACTION)) {
		case 1: /* Update all feeds */
			debug0(DEBUG_UPDATE, "initial update: updating all feeds");
			node_update_subscription (feedlist_get_root (), GUINT_TO_POINTER (0));
			break;
		case 2:
			debug0(DEBUG_UPDATE, "initial update: resetting feed counter");
			feedlist_reset_update_counters (NULL);
			break;
		default:
			debug0(DEBUG_UPDATE, "initial update: using auto update");
			/* default, which is to use the lastPoll times, does not need any actions here. */;
	}

	/* 5. Start automatic updating */
 	(void)g_timeout_add(10000, feedlist_auto_update, NULL);

	/* 6. Finally save the new feed list state */
	feedlistLoading = FALSE;
	feedlist_schedule_save();
	
	if(cacheMigrated)
		ui_show_info_box(_("This version of Liferea uses a new cache format and has migrated your "
		                   "feed cache. The cache content of v1.2 in ~/.liferea_1.2 was "
		                   "not deleted automatically. Please remove this directory "
		                   "manually once you are sure migration was successful!"));
	
	debug_exit("feedlist_init");
}

static void
feedlist_free_node (nodePtr node)
{
	if (node->children)
		node_foreach_child (node, feedlist_free_node);
	
	node->parent->children = g_slist_remove (node->parent->children, node);
	node_free (node); 
}

void
feedlist_free (void)
{
	feedlist_foreach (feedlist_free_node);
	node_free (rootNode);
	rootNode = NULL;
	
	feedlistLoading = TRUE;	/* prevent further feed list saving */
}
