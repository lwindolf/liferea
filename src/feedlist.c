/*
 * @file feedlist.c  subscriptions as an hierarchic tree
 *
 * Copyright (C) 2005-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "comments.h"
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
#include "update.h"
#include "vfolder.h"
#include "ui/feed_list_view.h"
#include "ui/itemview.h"
#include "ui/liferea_shell.h"
#include "ui/feed_list_view.h"
#include "fl_sources/node_source.h"

static void feedlist_save	(void);

struct _FeedList {
	GObject		parentInstance;

	guint		newCount;		/*<< overall new item count */

	nodePtr		rootNode;		/*<< the feed list root node */
	nodePtr		selectedNode;	/*<< matches the node selected in the feed list tree view, which
				                     is not necessarily the displayed one (e.g. folders without recursive
				                     display enabled) */

	guint		saveTimer;		/*<< timer id for delayed feed list saving */
	guint		autoUpdateTimer; /*<< timer id for auto update */

	gboolean	loading;		/*<< prevents the feed list being saved before it is completely loaded */
};

enum {
	NEW_ITEMS,		/*<< node has new items after update */
	NODE_UPDATED,	/*<< node display info (title, unread count) has changed */
	ITEMS_UPDATED,	/*<< all items were updated (i.e. marked all read) */
	LAST_SIGNAL
};

#define ROOTNODE feedlist->rootNode
#define SELECTED feedlist->selectedNode

static guint feedlist_signals[LAST_SIGNAL] = { 0 };

FeedList *feedlist = NULL;	// singleton

G_DEFINE_TYPE (FeedList, feedlist, G_TYPE_OBJECT);

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
	if (feedlist->autoUpdateTimer) {
		g_source_remove (feedlist->autoUpdateTimer);
		feedlist->autoUpdateTimer = 0;
	}
	if (feedlist->saveTimer) {
		g_source_remove (feedlist->saveTimer);
		feedlist->saveTimer = 0;
	}

	/* Enforce synchronous save upon exit */
	feedlist_save ();

	/* Save last selection for next start */
	if (feedlist->selectedNode)
		conf_set_str_value (LAST_NODE_SELECTED, feedlist->selectedNode->id);

	/* And destroy everything */
	feedlist_foreach (feedlist_free_node);
	node_free (ROOTNODE);
	ROOTNODE = NULL;

	/* This might also be a good place to use for some other cleanup */
	comments_deinit ();
}

static void
feedlist_class_init (FeedListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = feedlist_finalize;

	feedlist_signals[NEW_ITEMS] =
		g_signal_new ("new-items",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE,
		1,
		G_TYPE_POINTER);

	feedlist_signals[NODE_UPDATED] =
		g_signal_new ("node-updated",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING);

	feedlist_signals[ITEMS_UPDATED] =
		g_signal_new ("items-updated",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__POINTER,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING);
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

static void
on_network_status_changed (gpointer instance, gboolean online, gpointer data)
{
	if (online) feedlist_auto_update (NULL);
}

/* This method is used to initialize the node states in the feed list */
static void
feedlist_init_node (nodePtr node)
{
	if (node->expanded)
		feed_list_view_set_expansion (node, TRUE);

	if (node->subscription)
		db_subscription_load (node->subscription);

	node_update_counters (node);
	feed_list_view_update_node (node->id);	/* Necessary to initially set folder unread counters */

	node_foreach_child (node, feedlist_init_node);
}

static void
feedlist_init (FeedList *fl)
{
	gint	startup_feed_action;

	debug_enter ("feedlist_init");

	/* 1. Prepare globally accessible singleton */
	g_assert (NULL == feedlist);
	feedlist = fl;
	feedlist->loading = TRUE;

	/* 2. Set up a root node and import the feed list source structure. */
	debug0 (DEBUG_CACHE, "Setting up root node");
	ROOTNODE = node_source_setup_root ();

	/* 3. Ensure folder expansion and unread count*/
	debug0 (DEBUG_CACHE, "Initializing node state");
	feedlist_foreach (feedlist_init_node);

	/* 4. Check if feeds do need updating. */
	debug0 (DEBUG_UPDATE, "Performing initial feed update");
	conf_get_int_value (STARTUP_FEED_ACTION, &startup_feed_action);
	if (0 == startup_feed_action) {
		/* Update all feeds */
		if (network_monitor_is_online ()) {
			debug0 (DEBUG_UPDATE, "initial update: updating all feeds");
			node_auto_update_subscription (feedlist_get_root ());
		} else {
			debug0 (DEBUG_UPDATE, "initial update: prevented because we are offline");
		}
	} else {
		debug0 (DEBUG_UPDATE, "initial update: resetting feed counter");
		feedlist_reset_update_counters (NULL);
	}

	/* 5. Purge old nodes from the database */
	db_node_cleanup (feedlist_get_root ());

	/* 6. Start automatic updating */
	feedlist->autoUpdateTimer = g_timeout_add_seconds (10, feedlist_auto_update, NULL);
	g_signal_connect (network_monitor_get (), "online-status-changed", G_CALLBACK (on_network_status_changed), NULL);

	/* 7. Finally save the new feed list state */
	feedlist->loading = FALSE;
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

nodePtr
feedlist_find_node (nodePtr parent, feedListFindType type, const gchar *str)
{
	GSList	*iter;

	g_assert (str);

	iter = parent->children;
	while (iter) {
		gboolean found = FALSE;
		nodePtr result, node = (nodePtr)iter->data;

		/* Check child node */
		switch (type) {
			case NODE_BY_URL:
				if (node->subscription)
					found = g_str_equal (str, subscription_get_source (node->subscription));
				break;
			case NODE_BY_ID:
				found = g_str_equal (str, node->id);
				break;
			case FOLDER_BY_TITLE:
				if (IS_FOLDER (node))
					found = g_str_equal (str, node->title);
				break;

			default:
				break;
		}
		if (found)
			return node;

		/* And recurse */
		result = feedlist_find_node (node, type, str);
		if (result)
			return result;

		iter = g_slist_next (iter);
	}

	return NULL;
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
	node_update_counters (node);	/* update with parent propagation */

	if (node->needsUpdate)
		feed_list_view_update_node (node->id);
	if (node->children)
		node_foreach_child (node, feedlist_update_node_counters);
}

void
feedlist_mark_all_read (nodePtr node)
{
	if (!node)
		return;

	feedlist_reset_new_item_count ();

	if (!IS_FEED (node))
		itemview_select_item (NULL);

	if (node != ROOTNODE)
		node_mark_all_read (node);
	else
		node_foreach_child (ROOTNODE, node_mark_all_read);

	feedlist_foreach (feedlist_update_node_counters);
	itemview_update_all_items ();
	itemview_update ();

	g_signal_emit_by_name (feedlist, "items-updated", node->id);
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

	return (feedlist->newCount > 0)?feedlist->newCount:0;
}

void
feedlist_reset_new_item_count (void)
{
	if (feedlist->newCount)
		feedlist->newCount = 0;

	feedlist_new_items (0);
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
feedlist_add_subscription_check_duplicate(const gchar *source, const gchar *filter, updateOptionsPtr options, gint flags)
{
	nodePtr duplicateNode = NULL;

	duplicateNode = feedlist_find_node (feedlist_get_root (), NODE_BY_URL, source);
	if (!duplicateNode) {
		feedlist_add_subscription (source, filter, options, FEED_REQ_PRIORITY_HIGH);
	} else {
		feed_list_view_add_duplicate_url_subscription (subscription_new (source, filter, options), duplicateNode);
	}
}

void
feedlist_node_imported (nodePtr node)
{
	feed_list_view_add_node (node);

	feedlist_schedule_save ();
}

void
feedlist_node_added (nodePtr node)
{
	gint	position = -1;

	g_assert (NULL == node->parent);

	if (SELECTED && !IS_FOLDER (SELECTED)) {
		position = g_slist_index (SELECTED->parent->children, SELECTED);
		if (position > -1)
			position++;	/* insert after selected child index */
	}

	node_set_parent (node, feedlist_get_parent_node (), position);

	if (node->subscription)
		db_subscription_update (node->subscription);

	db_node_update (node);

	feedlist_node_imported (node);

	feed_list_view_select (node);
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
	if (node == SELECTED)
		feedlist_unselect ();

	/* First remove all children */
	node_foreach_child (node, feedlist_node_removed);

	node_remove (node);

	feed_list_view_remove_node (node);

	node->parent->children = g_slist_remove (node->parent->children, node);

	node_free (node);

	feedlist_schedule_save ();
}

/* Checks if the given node is a subscription node and
   has at least one unread item or is selected, if yes it
   is added to the list ref passed as user_data */
static void
feedlist_collect_unread (nodePtr node, gpointer user_data)
{
	GSList	**list = (GSList **)user_data;

	if (node->children) {
		node_foreach_child_data (node, feedlist_collect_unread, user_data);
		return;
	}
	if (!node->subscription)
		return;
	if (!node->unreadCount && SELECTED && !g_str_equal (node->id, SELECTED->id))
		return;

	*list = g_slist_append (*list, g_strdup (node->id));
}

nodePtr
feedlist_find_unread_feed (nodePtr folder)
{
	GSList	*list = NULL;
	nodePtr	result = NULL;

	feedlist_foreach_data (feedlist_collect_unread, &list);

	if (list) {
		// Pass 1: try after selected node in list
		if (SELECTED) {
			GSList *s = g_slist_find_custom (list, SELECTED->id, (GCompareFunc)g_strcmp0);
			if (s)
				s = g_slist_next (s);
			if (s)
				result = node_from_id (s->data);
		}

		// Pass 2: just return first node in list
		if (!result)
			result = node_from_id (list->data);

		g_slist_free_full (list, g_free);
	}
	return result;
}

/* selection handling */

static void
feedlist_unselect (void)
{
	SELECTED = NULL;

	itemview_set_displayed_node (NULL);
	itemview_update ();

	itemlist_unload ();
	feed_list_view_select (NULL);
}

static void
feedlist_selection_changed (gpointer obj, gchar * nodeId, gpointer data)
{
	debug_enter ("feedlist_selection_changed");

	nodePtr node = node_from_id (nodeId);
	if (node) {
		if (node != SELECTED) {
			debug1 (DEBUG_GUI, "new selected node: %s", node?node_get_title (node):"none");

			/* When the user selects a feed in the feed list we
			   assume that he got notified of the new items or
			   isn't interested in the event anymore... */
			if (0 != feedlist->newCount)
				feedlist_reset_new_item_count ();

			/* Unload visible items. */
			itemlist_unload ();

			/* Load items of new selected node. */
			SELECTED = node;
			if (SELECTED)
				itemlist_load (SELECTED);
			else
				itemview_clear ();
		} else {
			debug1 (DEBUG_GUI, "selected node stayed: %s", node?node_get_title (node):"none");
		}
	} else {
		debug1 (DEBUG_GUI, "failed to resolve node id: %s", nodeId);
	}

	debug_exit ("feedlist_selection_changed");
}

static gboolean
feedlist_schedule_save_cb (gpointer user_data)
{
	if (!feedlist || !ROOTNODE)
		return FALSE;

	/* step 1: request each node to save its state, that is
	   mostly needed for nodes that are node sources */
	feedlist_foreach (node_save);

	/* step 2: request saving for the root node and thereby
	   forcing the default source to write an OPML file */
	NODE_SOURCE_TYPE (ROOTNODE)->source_export (ROOTNODE);

	feedlist->saveTimer = 0;

	return FALSE;
}

void
feedlist_schedule_save (void)
{
	if (feedlist->loading || feedlist->saveTimer)
		return;

	debug0 (DEBUG_CONF, "Scheduling feedlist save");

	/* By waiting here 5s and checking feedlist_save_time
	   we hope to catch bulks of feed list changes and save
	   less often */
	feedlist->saveTimer = g_timeout_add_seconds (5, feedlist_schedule_save_cb, NULL);
}

/* Handling updates */

void
feedlist_new_items (guint newCount)
{
	feedlist->newCount += newCount;

	/* On subsequent feed updates with cache drops
	   more new items can be reported than effectively
	   were merged. The simplest way to catch this case
	   is by checking for new count > unread count here. */
	if (feedlist->newCount > ROOTNODE->unreadCount)
		feedlist->newCount = ROOTNODE->unreadCount;

	g_signal_emit_by_name (feedlist, "new-items", feedlist->newCount);
}

void
feedlist_node_was_updated (nodePtr node)
{
	node_update_counters (node);
	feedlist_schedule_save ();

	g_signal_emit_by_name (feedlist, "node-updated", node->title);
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
	guint64 now;

	if (!node)
		node = feedlist_get_root ();

	now = g_get_real_time();
	node_foreach_child_data (node, node_reset_update_counter, &now);
}

FeedList *
feedlist_create (gpointer flv)
{
	FeedList *fl = FEED_LIST (g_object_new (FEED_LIST_TYPE, NULL));

	g_signal_connect (flv, "selection-changed", G_CALLBACK (feedlist_selection_changed), fl);

	return fl;
}
