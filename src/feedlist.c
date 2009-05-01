/**
 * @file feedlist.c  subscriptions as an hierarchic tree
 *
 * Copyright (C) 2005-2009 Lars Lindner <lars.lindner@gmail.com>
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
#include "db.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "itemlist.h"
#include "net_monitor.h"
#include "node.h"
#include "script.h"
#include "update.h"
#include "vfolder.h"
#include "ui/itemview.h"
#include "ui/liferea_shell.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_node.h"
#include "ui/ui_subscription.h"
#include "ui/ui_tray.h"
#include "fl_sources/node_source.h"
#include "notification/notification.h"

static void feedlist_class_init	(FeedListClass *klass);
static void feedlist_init	(FeedList *fl);
static void feedlist_save	(void);

#define FEEDLIST_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), FEEDLIST_TYPE, FeedListPrivate))

struct FeedListPrivate {
	guint		newCount;	/**< overall new item count */

	nodePtr		rootNode;	/**< the feed list root node */
	nodePtr		selectedNode;	/**< matches the node selected in the feed list tree view, which
					     is not necessarily the displayed one (e.g. folders without recursive
					     display enabled) */

	guint		saveTimer;	/**< timer id for delayed feed list saving */
	guint		autoUpdateTimer; /**< timer id for auto update */

	gboolean	loading;	/**< this flag prevents the feed list being saved before it is completely loaded */
};

#define ROOTNODE feedlist->priv->rootNode
#define SELECTED feedlist->priv->selectedNode

#define IS_SELECTED(node) (feedlist->priv->selectedNode == node)

static GObjectClass *parent_class = NULL;
static FeedList *feedlist = NULL;

GType
feedlist_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (FeedListClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) feedlist_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (FeedList),
			0, /* n_preallocs */
			(GInstanceInitFunc) feedlist_init
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "FeedList",
					       &our_info, 0);
	}

	return type;
}

static void
feedlist_free_node (nodePtr node)
{
	if (node->children)
		node_foreach_child (node, feedlist_free_node);
	
	node->parent->children = g_slist_remove (node->parent->children, node);
	node_free (node); 
}

static void
feedlist_finalize (GObject *object)
{
	/* Stop all timer based activity */
	if (feedlist->priv->autoUpdateTimer)
		g_source_remove (feedlist->priv->autoUpdateTimer);
	if (feedlist->priv->saveTimer)
		g_source_remove (feedlist->priv->saveTimer);

	/* Enforce synchronous save upon exit */
	feedlist_save ();		
	
	/* And destroy everything */
	feedlist_foreach (feedlist_free_node);
	node_free (ROOTNODE);
	ROOTNODE = NULL;

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
feedlist_class_init (FeedListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = feedlist_finalize;

	g_type_class_add_private (object_class, sizeof(FeedListPrivate));
}

static gboolean
feedlist_auto_update (void *data)
{
	debug_enter ("feedlist_auto_update");

	if (network_monitor_is_online ())
		node_auto_update_subscription (ROOTNODE);
	else
		debug0 (DEBUG_UPDATE, "no update processing because we are offline!");
	
	debug_exit ("feedlist_auto_update");

	return TRUE;
}

/* This method is used to initialize the node states in the feed list */
static void
feedlist_init_node (nodePtr node) 
{
	if (node->expanded)
		ui_node_set_expansion (node, TRUE);
	
	if (node->subscription)
		db_subscription_load (node->subscription);
		
	node_update_counters (node);
	ui_node_update (node->id);	/* Necessary to initially set folder unread counters */
	
	node_foreach_child (node, feedlist_init_node);
}

static void
feedlist_init (FeedList *fl)
{
	debug_enter ("feedlist_init");
	
	/* 1. Prepare globally accessible singleton */
	g_assert (NULL == feedlist);
	feedlist = fl;
	
	feedlist->priv = FEEDLIST_GET_PRIVATE (fl);
	feedlist->priv->loading = TRUE;
	
	/* 2. Set up a root node and import the feed list source structure. */
	debug0 (DEBUG_CACHE, "Setting up root node");
	ROOTNODE = node_source_setup_root ();

	/* 3. Ensure folder expansion and unread count*/
	debug0 (DEBUG_CACHE, "Initializing node state");
	feedlist_foreach (feedlist_init_node);

	debug0 (DEBUG_GUI, "Notification setup");	
	notification_enable (conf_get_bool_value (SHOW_POPUP_WINDOWS));
	ui_tray_update ();

	/* 4. Check if feeds do need updating. */
	debug0 (DEBUG_UPDATE, "Performing initial feed update");
	switch (conf_get_int_value (STARTUP_FEED_ACTION)) {
		case 1: /* Update all feeds */
			debug0 (DEBUG_UPDATE, "initial update: updating all feeds");
			node_update_subscription (feedlist_get_root (), GUINT_TO_POINTER (0));
			break;
		case 2:
			debug0 (DEBUG_UPDATE, "initial update: resetting feed counter");
			feedlist_reset_update_counters (NULL);
			break;
		default:
			debug0 (DEBUG_UPDATE, "initial update: using auto update");
			/* default, which is to use the lastPoll times, does not need any actions here. */;
	}

	/* 5. Start automatic updating */
	feedlist->priv->autoUpdateTimer = g_timeout_add_seconds (10, feedlist_auto_update, NULL);

	/* 6. Finally save the new feed list state */
	feedlist->priv->loading = FALSE;
	feedlist_schedule_save ();
	
	debug_exit ("feedlist_init");	
}

static void feedlist_unselect(void);

nodePtr
feedlist_get_root (void)
{
	return ROOTNODE;
}

nodePtr
feedlist_get_selected (void)
{
	return SELECTED;
}

static nodePtr
feedlist_get_parent_node (void)
{ 

	g_assert (NULL != ROOTNODE);

	if (!SELECTED)
		return ROOTNODE;
	
	if (IS_FOLDER (SELECTED))
		return SELECTED;
	
	if (SELECTED->parent) 
		return SELECTED->parent;

	return ROOTNODE;
}

gboolean
feedlist_is_writable (void)
{
	nodePtr node;
	
	node = feedlist_get_parent_node ();
	
	return (0 != (NODE_TYPE (node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS));
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
	if (!node)
		return;

	feedlist_reset_new_item_count ();

	if (node != ROOTNODE)
		node_mark_all_read (node);
	else 
		node_foreach_child (ROOTNODE, node_mark_all_read);
		
	feedlist_foreach (feedlist_update_node_counters);
	itemview_update_all_items ();
	itemview_update ();
}

/* statistic handling methods */

guint
feedlist_get_unread_item_count (void)
{
	if (!feedlist)
		return 0;
		
	return (ROOTNODE->unreadCount > 0)?ROOTNODE->unreadCount:0;
}

guint
feedlist_get_new_item_count (void)
{
	if (!feedlist)
		return 0;
		
	return (feedlist->priv->newCount > 0)?feedlist->priv->newCount:0;
}

void
feedlist_update_new_item_count (guint addValue)
{
	feedlist->priv->newCount += addValue;
	
	/* On subsequent feed updates with cache drops
	   more new items can be reported than effectively
	   were merged. The simplest way to catch this case
	   is by checking for new count > unread count here. */
	if (feedlist->priv->newCount > ROOTNODE->unreadCount)
		feedlist->priv->newCount = ROOTNODE->unreadCount;
		
	ui_tray_update ();
	liferea_shell_update_unread_stats ();
}

void
feedlist_reset_new_item_count (void)
{
	if (feedlist->priv->newCount) {
		feedlist->priv->newCount = 0;
		ui_tray_update ();
		liferea_shell_update_unread_stats ();
	}
}

void
feedlist_node_was_updated (nodePtr node, guint newCount)
{
	vfolder_foreach (vfolder_update_counters);			
	node_update_counters (node);
	feedlist_update_new_item_count (newCount);
}

void
feedlist_add_folder (const gchar *title)
{
	nodePtr		parent;
	
	g_assert (NULL != title);

	parent = feedlist_get_parent_node ();

	if(0 == (NODE_TYPE (parent->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
		return;	

	node_source_add_folder (parent->source->root, title);
}

void
feedlist_add_subscription (const gchar *source, const gchar *filter, updateOptionsPtr options, gint flags)
{
	nodePtr		parent;

	g_assert (NULL != source);

	parent = feedlist_get_parent_node ();

	if (0 == (NODE_TYPE (parent->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS)) {
		g_warning ("feedlist_add_subscription: this should never happen!");
		return;
	}

	node_source_add_subscription (parent->source->root, subscription_new (source, filter, options));
}

void
feedlist_node_imported (nodePtr node)
{
	ui_node_add (node);	
	feedlist_schedule_save ();
}

void
feedlist_node_added (nodePtr node)
{
	gint	position = -1;
	
	g_assert (NULL == node->parent);
	
	if (SELECTED && !IS_FOLDER(SELECTED)) {
		position = g_slist_index (SELECTED->parent->children, SELECTED);
		if (position > -1)
			position++;	/* insert after selected child index */
	}

	node_set_parent (node, feedlist_get_parent_node (), position);
	
	if (node->subscription)
		db_subscription_update (node->subscription);
	
	db_node_update(node);

	feedlist_node_imported (node);
	
	ui_feedlist_select (node);
	
	if (node->subscription)
		script_run_for_hook (SCRIPT_HOOK_NEW_SUBSCRIPTION);
}

void
feedlist_remove_node (nodePtr node)
{
	if (node->source->root != node)
		node_source_remove_node (node->source->root, node);
	else
		feedlist_node_removed (node);
}

void
feedlist_node_removed (nodePtr node)
{
	if (IS_SELECTED (node))
		feedlist_unselect ();
		
	/* First remove all children */
	node_foreach_child (node, feedlist_node_removed);

	node_remove (node);

	ui_node_remove_node (node);

	node->parent->children = g_slist_remove (node->parent->children, node);

	node_free (node);
	
	feedlist_schedule_save ();
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
static nodePtr
feedlist_unread_scan (nodePtr folder)
{
	nodePtr		childNode;
	GSList		*selectedIter = NULL;

	if (SELECTED)
		selectedIter = g_slist_find(SELECTED->parent->children, SELECTED);
	else
		scanState = UNREAD_SCAN_SECOND_PASS;

	GSList *iter = folder->children;
	while (iter) {
		nodePtr node = iter->data;

		if (node == SELECTED)
			scanState = UNREAD_SCAN_FOUND_SELECTED;

		/* feed match if beyond the selected feed or in second pass... */
		if ((scanState != UNREAD_SCAN_INIT) && (node->unreadCount > 0) &&
		    (NULL == node->children) && !IS_VFOLDER(node)) {
		       return node;
		}

		/* folder traversal if we are searching the selected feed
		   which might be a descendant of the folder and if we
		   are beyond the selected feed and the folder contains
		   feeds with unread items... */
		if (node->children &&
		    (((scanState != UNREAD_SCAN_INIT) && (node->unreadCount > 0)) ||
		     (selectedIter && (node_is_ancestor (node, SELECTED))))) {
			childNode = feedlist_unread_scan (node);
			if (childNode)
				return childNode;
		}

		iter = g_slist_next (iter);
	}

	/* When we come here we didn't find anything from the selected
	   feed down to the end of the feed list. */
	if (folder == ROOTNODE) {
		if (0 == ROOTNODE->unreadCount) {
			/* this may mean there is nothing more to find */
		} else {
			/* or that there are unread items above the selected feed */
			g_assert (scanState != UNREAD_SCAN_SECOND_PASS);
			scanState = UNREAD_SCAN_SECOND_PASS;
			return feedlist_unread_scan (ROOTNODE);
		}
	}

	return NULL;
}

nodePtr
feedlist_find_unread_feed (nodePtr folder)
{
	scanState = UNREAD_SCAN_INIT;
	return feedlist_unread_scan (folder);
}

/* selection handling */

static void
feedlist_unselect (void)
{
	SELECTED = NULL;

	itemview_set_displayed_node (NULL);
	itemview_update ();
		
	itemlist_unload (FALSE /* mark all read */);
	ui_feedlist_select (NULL);
	liferea_shell_update_feed_menu (TRUE, FALSE, FALSE);
	liferea_shell_update_allitems_actions (FALSE, FALSE);
}

void
feedlist_selection_changed (nodePtr node)
{
	debug_enter ("feedlist_selection_changed");

	debug1 (DEBUG_GUI, "new selected node: %s", node?node_get_title (node):"none");
	if (node != SELECTED) {

		/* When the user selects a feed in the feed list we
		   assume that he got notified of the new items or
		   isn't interested in the event anymore... */
		if (0 != feedlist->priv->newCount)
			feedlist_reset_new_item_count ();

		script_run_for_hook (SCRIPT_HOOK_FEED_UNSELECT);

		/* Unload visible items. */
		itemlist_unload (TRUE);
	
		/* Load items of new selected node. */
		SELECTED = node;
		if (SELECTED) {
			itemlist_set_view_mode (node_get_view_mode (SELECTED));		
			itemlist_load (SELECTED);
		} else {
			itemview_clear ();
		}
		
		if (SELECTED)
			script_run_for_hook (SCRIPT_HOOK_FEED_SELECTED);
	}

	debug_exit ("feedlist_selection_changed");
}

static gboolean
feedlist_schedule_save_cb (gpointer user_data)
{
	/* step 1: request each node to save its state */
	feedlist_foreach (node_save);

	/* step 2: request saving for the root node and thereby
	   forcing the root plugin to save the feed list structure */
	NODE_SOURCE_TYPE (ROOTNODE)->source_export (ROOTNODE);
	
	feedlist->priv->saveTimer = 0;
	
	return FALSE;
}

void
feedlist_schedule_save (void)
{
	if (feedlist->priv->loading || feedlist->priv->saveTimer)
		return;
		
	debug0 (DEBUG_CONF, "Scheduling feedlist save");

	/* By waiting here 5s and checking feedlist_save_time
	   we hope to catch bulks of feed list changes and save 
	   less often */
	feedlist->priv->saveTimer = g_timeout_add_seconds (5, feedlist_schedule_save_cb, NULL);
}

/* This method is only to be used when exiting the program! */
static void
feedlist_save (void)
{
	debug0 (DEBUG_CONF, "Forced feed list save");
	feedlist_schedule_save_cb (NULL);
}

void
feedlist_reset_update_counters (nodePtr node) 
{
	GTimeVal now;
	
	if (!node)
		node = feedlist_get_root ();	
	
	g_get_current_time (&now);
	node_foreach_child_data (node, node_reset_update_counter, &now);
}

FeedList *
feedlist_create (void)
{
	return FEEDLIST (g_object_new (FEEDLIST_TYPE, NULL));
}
