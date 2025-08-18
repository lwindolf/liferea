/*
 * @file itemlist.c  item list controller
 *
 * Copyright (C) 2004-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include <string.h>
#include <glib.h>

#include "browser.h"
#include "comments.h"
#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "node_providers/folder.h"
#include "item_history.h"
#include "item_state.h"
#include "itemlist.h"
#include "itemset.h"
#include "metadata.h"
#include "node.h"
#include "node_providers/vfolder.h"
#include "rule.h"
#include "ui/liferea_shell.h"

/* The 'item list' is a controller for 'item view' and database backend.
   It manages the currently displayed 'node', realizes filtering
   by node and 'item set' rules, also duplicate elimination and provides
   synchronisation for backend and GUI access to the current itemset.

   The 'item list' provides methods to add/remove items from the 'item
   view' synchronously and allows registering 'item loaders' for
   asynchronous adding of item batches.

   The 'item list' does not handle item list rendering variants, which
   is done by the 'item view'.
 */

#define ITEMLIST_GET_PRIVATE itemlist_get_instance_private

struct ItemListPrivate
{
	GHashTable	*guids;			/*<< list of GUID to avoid having duplicates in currently loaded list */
	itemSetPtr	filter;			/*<< currently active filter rules */
	Node		*currentNode;		/*<< the node whose own or its child items are currently displayed */
	gulong		selectedId;		/*<< the currently selected (and displayed) item id */

	guint 		loading;		/*<< if >0 prevents selection effects when loading the item list */
	itemPtr		invalidSelection;	/*<< if set then the next selection might need to do an unselect first */

	gboolean 	deferredRemove;		/*<< TRUE if selected item needs to be removed from cache on unselecting */
};

enum {
	ITEM_ADDED,		/*<< a new item has been added to the list */
	ITEM_UPDATED,		/*<< state of a currently visible item has changed */
	ITEM_REMOVED,		/*<< an item has been removed from the list */
	ALL_ITEMS_REMOVED,	/*<< all items have been removed from the list */
	ITEM_SELECTED,		/*<< the currently selected item has changed */
	ITEM_BATCH_START,	/*<< item list has been reset and new batch load starts */
	ITEM_BATCH_END,		/*<< batch load finised */
	LAST_SIGNAL
};

static guint itemlist_signals[LAST_SIGNAL] = { 0 };

static GObjectClass *parent_class = NULL;
static ItemList *itemlist = NULL;

G_DEFINE_TYPE_WITH_CODE (ItemList, itemlist, G_TYPE_OBJECT, G_ADD_PRIVATE (ItemList));

static void
itemlist_init (ItemList *il)
{
	/* 1. Prepare globally accessible singleton */
	g_assert (NULL == itemlist);
	itemlist = il;
	itemlist->priv = ITEMLIST_GET_PRIVATE (il);
	itemlist->priv->guids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void
itemlist_duplicate_list_remove_item (itemPtr item)
{
	if (!itemlist || !item->validGuid)
		return;

	g_hash_table_remove (itemlist->priv->guids, item->sourceId);
}

static void
itemlist_duplicate_list_add_item (itemPtr item)
{
	if (!itemlist || !item->validGuid)
		return;
	
	g_hash_table_insert (itemlist->priv->guids, g_strdup (item->sourceId), GUINT_TO_POINTER (item->id));
}

static gboolean
itemlist_duplicate_list_check_item (itemPtr item)
{
	if (!itemlist || !item->validGuid)
		return TRUE;

	return (NULL == g_hash_table_lookup (itemlist->priv->guids, item->sourceId));
}

static void
itemlist_finalize (GObject *object)
{
	itemset_free (itemlist->priv->filter);
	g_hash_table_destroy (itemlist->priv->guids);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
itemlist_class_init (ItemListClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = itemlist_finalize;

	itemlist_signals[ITEM_UPDATED] =
		g_signal_new ("item-updated",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE,
		1,
		G_TYPE_INT);

	itemlist_signals[ITEM_ADDED] =
		g_signal_new ("item-added",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE,
		1,
		G_TYPE_INT);

	itemlist_signals[ITEM_REMOVED] =
		g_signal_new ("item-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE,
		1,
		G_TYPE_INT);

	itemlist_signals[ALL_ITEMS_REMOVED] =
		g_signal_new ("all-items-removed",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0,
		NULL);

	itemlist_signals[ITEM_SELECTED] =
		g_signal_new ("item-selected",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE,
		1,
		G_TYPE_INT);

	itemlist_signals[ITEM_BATCH_START] =
		g_signal_new ("item-batch-start",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0);

	itemlist_signals[ITEM_BATCH_END] =
		g_signal_new ("item-batch-end",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_INT,
		0);
}

/* member wrappers */

itemPtr
itemlist_get_selected (void)
{
	return item_load (itemlist->priv->selectedId);
}

gulong
itemlist_get_selected_id (void)
{
	return itemlist->priv->selectedId;
}

void
itemlist_set_selected (itemPtr item)
{
	itemlist->priv->selectedId = item?item->id:0;
	g_signal_emit_by_name (itemlist, "item-selected", item?item->id:0);
}

Node *
itemlist_get_displayed_node (void)
{
	return itemlist->priv->currentNode;
}

static gboolean
itemlist_filter_check_item (itemPtr item)
{
	if (itemlist->priv->currentNode && IS_VFOLDER (itemlist->priv->currentNode)) {
		vfolderPtr vfolder = (vfolderPtr)itemlist->priv->currentNode->data;

		/* Use hide read for search folders */
		if (vfolder->unreadOnly && item->readStatus)
			return FALSE;

		/* Use search folder rule list in case of a search folder */
		return itemset_check_item (vfolder->itemset, item);
	}

	/* apply the item list filter if available */
	if (itemlist->priv->filter)
		return itemset_check_item (itemlist->priv->filter, item);

	/* otherwise keep the item */
	return TRUE;
}

/* called when unselecting the item or unloading the item list */
static void
itemlist_check_for_deferred_action (void)
{
	itemPtr item;

	if (!itemlist_get_selected_id ())
		return;

	item = itemlist_get_selected ();
	itemlist_set_selected (NULL);

	/* check for item hiding caused by itemlist filter rule (i.e. folder hide read items) */
	if (!itemlist_filter_check_item (item)) {
		gboolean keep_for_search_folder;
		conf_get_bool_value (DEFER_DELETE_MODE, &keep_for_search_folder);
		if (keep_for_search_folder) {
			item->isHidden = TRUE;
			g_signal_emit_by_name (itemlist, "item-updated", item->id);
		} else {
			g_signal_emit_by_name (itemlist, "item-removed", item->id);
		}
	}

	/* check for item unloading caused by search folder rules (i.e. unread items only) */
	if (itemlist->priv->deferredRemove) {
		itemlist->priv->deferredRemove = FALSE;
		itemlist_remove_item (item);
	} else {
		item_unload (item);
	}
}

static void
itemlist_merge_item (itemPtr item)
{
	if (!itemlist_duplicate_list_check_item (item))
		return;

	if (!itemlist_filter_check_item (item))
		return;

	itemlist_duplicate_list_add_item (item);
	g_signal_emit_by_name (itemlist, "item-added", item->id);
}

static void
itemlist_merge_item_callback (itemPtr item, gpointer _unused)
{
	itemlist_merge_item (item);
}

/* Helper method checking if the passed item set is relevant
   for the currently item list content. */
static gboolean
itemlist_itemset_is_valid (itemSetPtr itemSet)
{
	gint	folder_display_mode;
	Node	*node;

	node = node_from_id (itemSet->nodeId);

	if (!itemlist->priv->currentNode)
		return FALSE; /* Nothing to do if nothing is displayed */

	if (!IS_VFOLDER (itemlist->priv->currentNode) &&
	    (itemlist->priv->currentNode != node) &&
	    !node_is_ancestor (itemlist->priv->currentNode, node))
		return FALSE; /* Nothing to do if the item set does not belong to this node, or this is a search folder */

	conf_get_int_value (FOLDER_DISPLAY_MODE, &folder_display_mode);
	if (IS_FOLDER (itemlist->priv->currentNode) && !folder_display_mode)
		return FALSE; /* Bail out if it is a folder without the recursive display preference set */

	debug (DEBUG_GUI, "reloading item list with node \"%s\"", node_get_title (node));

	return TRUE;
}

void
itemlist_merge_itemset (itemSetPtr itemSet)
{
	if (itemlist_itemset_is_valid (itemSet))
		itemset_foreach (itemSet, itemlist_merge_item_callback, NULL);
}

void
itemlist_load (Node *node)
{
	itemSetPtr	itemSet;
	gint		folder_display_mode;
	gboolean	display_hide_read = FALSE;

	g_return_if_fail (NULL != node);

	debug (DEBUG_GUI, "loading item list with node \"%s\"", node_get_title (node));

	g_assert (!itemlist->priv->filter);
g_print("itemlist_load called %s\n",node->title);
	g_signal_emit_by_name (itemlist, "item-batch-start", 0);

	/* 1. Filter check. Don't continue if folder is selected and
	   no folder viewing is configured. If folder viewing is enabled
	   set up a "unread items only" rule depending on the prefences. */

	/* for folders and other hierarchic nodes do preference based filtering */
	if (IS_FOLDER (node) || node->children) {
		conf_get_int_value (FOLDER_DISPLAY_MODE, &folder_display_mode);
		if (!folder_display_mode)
			return;

		conf_get_bool_value (FOLDER_DISPLAY_HIDE_READ, &display_hide_read);
	}

	/* for search folders do properties based filtering */
	if (IS_VFOLDER (node))
		display_hide_read = ((vfolderPtr)node)->unreadOnly;

	if (display_hide_read) {
		itemlist->priv->filter = g_new0(struct itemSet, 1);
		itemlist->priv->filter->anyMatch = TRUE;
		itemset_add_rule (itemlist->priv->filter, "unread", "", TRUE);
	}

	itemlist->priv->loading++;
	itemlist->priv->currentNode = node;

	itemSet = node_get_itemset (itemlist->priv->currentNode);
	itemlist_merge_itemset (itemSet);
	if (!IS_VFOLDER (node))			/* FIXME: this is ugly! */
		itemset_free (itemSet);

	itemlist->priv->loading--;

	g_signal_emit_by_name (itemlist, "item-batch-end", 0);
}

void
itemlist_unload (void)
{
	g_print("itemlist_unload called\n");
	if (itemlist->priv->currentNode)
		itemlist_check_for_deferred_action ();

	itemlist_set_selected (NULL);
	g_hash_table_steal_all (itemlist->priv->guids);
	itemlist->priv->currentNode = NULL;

	itemset_free (itemlist->priv->filter);
	itemlist->priv->filter = NULL;
}

void
itemlist_select_next_unread (void)
{
	itemPtr	result = NULL;

	itemlist->priv->loading++;	/* prevent unwanted selections */
g_warning("FIXME itemlist_select_next_unread");
	/* before scanning the feed list, we test if there is a unread
	   item in the currently selected feed! */
	//result = itemview_find_unread_item (itemlist->priv->selectedId);

	/* If none is found we continue searching in the feed list */
	//if (!result) {
		/* scan feed list and find first feed with unread items */
	//	Node *node = feedlist_find_unread_feed (feedlist_get_root ());
	//	if (node) {
	//		/* load found feed */
	//		feed_list_view_select (node);
	//		result = itemview_find_unread_item (0);	/* find first unread item */
	//	} else {
	//		/* if we don't find a feed with unread items do nothing */
	//		liferea_shell_set_status_bar (_("There are no unread items"));
	//	}
	//}

	itemlist->priv->loading--;

	if (result)
		itemlist_set_selected (result);
}

/* menu commands */

void
itemlist_toggle_flag (itemPtr item)
{
	item_set_flag_state (item, !(item->flagStatus));
	g_signal_emit_by_name (itemlist, "item-updated", item->id);
}

void
itemlist_toggle_read_status (itemPtr item)
{
	item_set_read_state (item, !(item->readStatus));
	g_signal_emit_by_name (itemlist, "item-updated", item->id);
}

/* function to remove items due to item list filtering */
static void
itemlist_hide_item (itemPtr item)
{
	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if (itemlist->priv->selectedId != item->id) {
		g_signal_emit_by_name (itemlist, "item-removed", item->id);
	} else {
		/* update the item to show new state that forces
		   later removal */
		g_signal_emit_by_name (itemlist, "item-updated", item->id);
	}
}

/* function to cancel deferred filtering of selected item */
static void
itemlist_unhide_item (itemPtr item)
{
        item->isHidden = FALSE;
}

/* functions to remove items on remove requests */

/* hard unconditional item remove */
void
itemlist_remove_item (itemPtr item)
{
	if (itemlist->priv->selectedId == item->id) {
		itemlist_set_selected (NULL);
		itemlist->priv->deferredRemove = FALSE;
	}

	itemlist_duplicate_list_remove_item (item);

	db_item_remove (item->id);

	/* update feed list counters*/
	vfolder_foreach (node_update_counters);
	node_update_counters (node_from_id (item->nodeId));

	g_signal_emit_by_name (itemlist, "item-removed", item->id);
	item_unload (item);
}

/* soft possibly delayed item remove */
static void
itemlist_request_remove_item (itemPtr item)
{
	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if (itemlist->priv->selectedId != item->id) {
		g_signal_emit_by_name (itemlist, "item-removed", item->id);
	} else {
		itemlist->priv->deferredRemove = TRUE;
		/* update the item to show new state that forces
		   later removal */
		g_signal_emit_by_name (itemlist, "item-updated", item->id);
	}
}

void
itemlist_remove_items (itemSetPtr itemSet, GList *items)
{
	GList		*iter = items;

	while (iter) {
		itemPtr item = (itemPtr) iter->data;
		if (itemlist->priv->selectedId != item->id) {
			g_signal_emit_by_name (itemlist, "item-removed", item->id);
			db_item_remove(item->id);
		} else {
			itemlist_request_remove_item(item);
		}
		g_object_unref (item);

		iter = g_list_next (iter);
	}

	vfolder_foreach (node_update_counters);
	node_update_counters (node_from_id (itemSet->nodeId));
}

void
itemlist_remove_all_items (Node *node)
{
	if (node == itemlist->priv->currentNode)
		itemlist_set_selected (NULL);

	db_itemset_remove_all (node->id);

	if (node == itemlist->priv->currentNode)
		g_hash_table_steal_all (itemlist->priv->guids);

	vfolder_foreach (node_update_counters);
	node_update_counters (node);

	g_signal_emit_by_name (itemlist, "all-items-removed");
}

void
itemlist_update_item (itemPtr item)
{
	if (!itemlist_filter_check_item (item)) {
		itemlist_hide_item (item);
		return;
	} else {
		itemlist_unhide_item (item);
	}

	g_signal_emit_by_name (itemlist, "item-updated", item->id);
}

/* mouse/keyboard interaction callbacks */

void
itemlist_selection_changed (ItemList *ilv, gulong itemId, gpointer unused)
{
	itemPtr item = item_load (itemId);

	if (!item)
		return;

	if (itemlist->priv->selectedId == itemId)
		return;

	if (0 == itemlist->priv->loading) {
		/* folder&vfolder postprocessing to remove/filter unselected items no
		more matching the display rules because they have changed state */
		itemlist_check_for_deferred_action ();

			debug (DEBUG_GUI, "item list selection changed to \"%s\"", item?item_get_title (item):"(null)");

			itemlist_set_selected (item);

			/* set read and unset update status when selecting */
			if (item) {
				gchar	*link = NULL;
				Node	*node = node_from_id (item->nodeId);

			item_set_read_state (item, TRUE);

			if (IS_FEED (node) && node->data && ((feedPtr)node->data)->loadItemLink && (link = item_make_link (item))) {
				browser_launch_URL (link, TRUE /* force internal */);
				g_free (link);
			}
		}

		feedlist_reset_new_item_count ();

		g_signal_emit_by_name (itemlist, "item-selected", NULL);
	}
	g_object_unref (item);
}

static void
itemlist_select_from_history (gboolean back)
{
	itemPtr item;
	Node *node;

	if (back)
		item = item_history_get_previous ();
	else
		item = item_history_get_next ();

	if (!item)
		return;

	node = node_from_id (item->parentNodeId);
	if (!node)
		return;

	feedlist_set_selected (node);
	itemlist_set_selected (item);
	item_unload (item);
}

void
on_prev_read_item_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemlist_select_from_history (TRUE);
}

void
on_next_read_item_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemlist_select_from_history (FALSE);
}

/* item loader methods */

static void
itemlist_item_batch_fetched_cb (ItemLoader *il, GSList *items, gpointer user_data)
{
	GSList		*iter;

	if (item_loader_get_node (il) != itemlist->priv->currentNode)
		return;	/* Bail on loader not matching selection */

	debug (DEBUG_CACHE, "itemlist_item_batch_fetched_cb()");

	iter = items;
	while (iter) {
		itemPtr item = (itemPtr)iter->data;

		itemlist_merge_item (item);
		item_unload (item);

		iter= g_slist_next (iter);
	}

	g_slist_free (items);
}

static void
itemlist_add_loader (ItemLoader *loader)
{
	g_signal_connect (G_OBJECT (loader), "item-batch-fetched", G_CALLBACK (itemlist_item_batch_fetched_cb), NULL);

	item_loader_start (loader);
}

void
itemlist_add_search_result (ItemLoader *loader)
{
	itemlist_unload ();

	/* Set current node to search result dummy node so that
	   we except only items from the respective loader for
	   the item view. */
	itemlist->priv->currentNode = item_loader_get_node (loader);

	itemlist_add_loader (loader);
}
