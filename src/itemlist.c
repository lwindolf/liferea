/*
 * @file itemlist.c  item list handling
 *
 * Copyright (C) 2004-2023 Lars Windolf <lars.windolf@gmx.de>
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

#include "comments.h"
#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "item_history.h"
#include "item_state.h"
#include "itemlist.h"
#include "itemset.h"
#include "metadata.h"
#include "node.h"
#include "rule.h"
#include "vfolder.h"
#include "ui/item_list_view.h"
#include "ui/itemview.h"
#include "ui/liferea_shell.h"
#include "ui/feed_list_view.h"

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
	nodePtr		currentNode;		/*<< the node whose own or its child items are currently displayed */
	gulong		selectedId;		/*<< the currently selected (and displayed) item id */

	guint 		loading;		/*<< if >0 prevents selection effects when loading the item list */
	itemPtr		invalidSelection;	/*<< if set then the next selection might need to do an unselect first */

	gboolean 	deferredRemove;		/*<< TRUE if selected item needs to be removed from cache on unselecting */
};

enum {
	ITEM_UPDATED,	/*<< state of a currently visible item has changed */
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
}

static void
itemlist_duplicate_list_remove_item (itemPtr item)
{
	if (!item->validGuid)
		return;
	if (!itemlist->priv->guids)
		return;
	g_hash_table_remove (itemlist->priv->guids, item->sourceId);
}

static void
itemlist_duplicate_list_add_item (itemPtr item)
{
	if (!item->validGuid)
		return;
	if (!itemlist->priv->guids)
		itemlist->priv->guids = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	g_hash_table_insert (itemlist->priv->guids, g_strdup (item->sourceId), GUINT_TO_POINTER (item->id));
}

static gboolean
itemlist_duplicate_list_check_item (itemPtr item)
{
	if (!itemlist->priv->guids || !item->validGuid)
		return TRUE;

	return (NULL == g_hash_table_lookup (itemlist->priv->guids, item->sourceId));
}

static void
itemlist_duplicate_list_free (void)
{
	if (itemlist->priv->guids) {
		g_hash_table_destroy (itemlist->priv->guids);
		itemlist->priv->guids = NULL;
	}
}

static void
itemlist_finalize (GObject *object)
{
	itemset_free (itemlist->priv->filter);
	itemlist_duplicate_list_free ();

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
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING);
}

/* member wrappers */

itemPtr
itemlist_get_selected (void)
{
	return item_load(itemlist->priv->selectedId);
}

gulong
itemlist_get_selected_id (void)
{
	return itemlist->priv->selectedId;
}

static void
itemlist_set_selected (itemPtr item)
{
	itemlist->priv->selectedId = item?item->id:0;
}

nodePtr
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
			itemview_update_item (item);
		} else {
			itemview_remove_item (item);
		}
		feed_list_view_update_node (item->nodeId);
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
	itemview_add_item (item);
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
	nodePtr node;

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

	debug1 (DEBUG_GUI, "reloading item list with node \"%s\"", node_get_title (node));

	return TRUE;
}

void
itemlist_merge_itemset (itemSetPtr itemSet)
{
	debug_enter ("itemlist_merge_itemset");

	if (itemlist_itemset_is_valid (itemSet)) {
		debug_start_measurement (DEBUG_GUI);
		itemset_foreach (itemSet, itemlist_merge_item_callback, NULL);
		itemview_update ();
		debug_end_measurement (DEBUG_GUI, "itemlist merge");
	}

	debug_exit ("itemlist_merge_itemset");
}

void
itemlist_load (nodePtr node)
{
	itemSetPtr	itemSet;
	gint		folder_display_mode;
	gboolean	display_hide_read = FALSE;

	debug_enter ("itemlist_load");

	g_return_if_fail (NULL != node);

	debug1 (DEBUG_GUI, "loading item list with node \"%s\"", node_get_title (node));

	g_assert (!itemlist->priv->guids);
	g_assert (!itemlist->priv->filter);

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

	/* Set the new displayed node... */
	itemlist->priv->currentNode = node;
	itemview_set_displayed_node (itemlist->priv->currentNode);

	itemview_set_mode (ITEMVIEW_NODE_INFO);

	itemSet = node_get_itemset (itemlist->priv->currentNode);
	itemlist_merge_itemset (itemSet);
	if (!IS_VFOLDER (node))			/* FIXME: this is ugly! */
		itemset_free (itemSet);

	itemlist->priv->loading--;

	debug_exit("itemlist_load");
}

void
itemlist_unload (void)
{
	/* Always clear to ensure clearing on search */
	itemview_clear ();

	if (itemlist->priv->currentNode) {
		itemview_set_displayed_node (NULL);
		itemlist_check_for_deferred_action ();
	}

	itemlist_set_selected (NULL);
	itemlist_duplicate_list_free ();
	itemlist->priv->currentNode = NULL;

	itemset_free (itemlist->priv->filter);
	itemlist->priv->filter = NULL;
}

void
itemlist_select_next_unread (void)
{
	itemPtr	result = NULL;

	itemlist->priv->loading++;	/* prevent unwanted selections */

	/* before scanning the feed list, we test if there is a unread
	   item in the currently selected feed! */
	result = itemview_find_unread_item (itemlist->priv->selectedId);

	/* If none is found we continue searching in the feed list */
	if (!result) {
		nodePtr	node;

		/* scan feed list and find first feed with unread items */
		node = feedlist_find_unread_feed (feedlist_get_root ());
		if (node) {
			/* load found feed */
			feed_list_view_select (node);
			result = itemview_find_unread_item (0);	/* find first unread item */
		} else {
			/* if we don't find a feed with unread items do nothing */
			liferea_shell_set_status_bar (_("There are no unread items"));
		}
	}

	itemlist->priv->loading--;

	if (result)
		itemview_select_item (result);
}

/* menu commands */

void
itemlist_toggle_flag (itemPtr item)
{
	item_set_flag_state (item, !(item->flagStatus));
}

void
itemlist_toggle_read_status (itemPtr item)
{
	item_set_read_state (item, !(item->readStatus));
	g_signal_emit_by_name (itemlist, "item-updated", item->nodeId);
}

/* function to remove items due to item list filtering */
static void
itemlist_hide_item (itemPtr item)
{
	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if (itemlist->priv->selectedId != item->id) {
		itemview_remove_item (item);
		feed_list_view_update_node (item->nodeId);
	} else {
		/* update the item to show new state that forces
		   later removal */
		itemview_update_item (item);
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

	itemview_remove_item (item);
	itemview_update ();

	db_item_remove (item->id);

	/* update feed list counters*/
	vfolder_foreach (node_update_counters);
	node_update_counters (node_from_id (item->nodeId));

	item_unload (item);
	g_signal_emit_by_name (itemlist, "item-updated", item->nodeId);
}

/* soft possibly delayed item remove */
static void
itemlist_request_remove_item (itemPtr item)
{
	/* if the currently selected item should be removed we
	   don't do it and set a flag to do it when unselecting */
	if (itemlist->priv->selectedId != item->id) {
		itemlist_remove_item (item);
	} else {
		itemlist->priv->deferredRemove = TRUE;
		/* update the item to show new state that forces
		   later removal */
		itemview_update_item (item);
	}
}

void
itemlist_remove_items (itemSetPtr itemSet, GList *items)
{
	GList		*iter = items;

	while (iter) {
		itemPtr item = (itemPtr) iter->data;
		if (itemlist->priv->selectedId != item->id) {
			itemview_remove_item(item);
			db_item_remove(item->id);
		} else {
			itemlist_request_remove_item(item);
		}
		g_object_unref (item);

		iter = g_list_next (iter);
	}

	itemview_update ();
	vfolder_foreach (node_update_counters);
	node_update_counters (node_from_id (itemSet->nodeId));
	g_signal_emit_by_name (itemlist, "item-updated", itemSet->nodeId);
}

void
itemlist_remove_all_items (nodePtr node)
{
	if (node == itemlist->priv->currentNode)
		itemview_clear ();

	db_itemset_remove_all (node->id);

	if (node == itemlist->priv->currentNode) {
		itemview_update ();
		itemlist_duplicate_list_free ();
	}

	vfolder_foreach (node_update_counters);
	node_update_counters (node);
	g_signal_emit_by_name (itemlist, "item-updated", node->id);
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

	itemview_update_item (item);
}

/* mouse/keyboard interaction callbacks */
void
itemlist_selection_changed (itemPtr item)
{
	debug_enter ("itemlist_selection_changed");
	debug_start_measurement (DEBUG_GUI);

	if (0 == itemlist->priv->loading)	{
		/* folder&vfolder postprocessing to remove/filter unselected items no
		   more matching the display rules because they have changed state */
		itemlist_check_for_deferred_action ();

		debug1 (DEBUG_GUI, "item list selection changed to \"%s\"", item?item_get_title (item):"(null)");

		itemlist_set_selected (item);

		/* set read and unset update status when selecting */
		if (item) {
			gchar	*link = NULL;
			nodePtr	node = node_from_id (item->nodeId);

			item_set_read_state (item, TRUE);
			itemview_set_mode (ITEMVIEW_SINGLE_ITEM);

			if (node->loadItemLink && (link = item_make_link (item))) {
				itemview_launch_URL (link, TRUE /* force internal */);
				g_free (link);
			} else {
				if (IS_FEED(node) && !((feedPtr)node->data)->ignoreComments)
					comments_refresh (item);

				itemview_select_item (item);
				itemview_update ();
			}
			feed_list_view_update_node (item->nodeId);
		}

		feedlist_reset_new_item_count ();
	}

	if (item)
		g_object_unref (item);

	debug_end_measurement (DEBUG_GUI, "itemlist selection");
	debug_exit ("itemlist_selection_changed");
}

static void
itemlist_select_from_history (gboolean back)
{
	itemPtr item;
	nodePtr node;

	if (back)
		item = item_history_get_previous ();
	else
		item = item_history_get_next ();

	if (!item)
		return;

	node = node_from_id (item->parentNodeId);
	if (!node)
		return;

	if (node != feedlist_get_selected ())
		feed_list_view_select (node);

	itemview_select_item (item);
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

	debug0 (DEBUG_CACHE, "itemlist_item_batch_fetched_cb()");

	iter = items;
	while (iter) {
		itemPtr item = (itemPtr)iter->data;

		itemlist_merge_item (item);
		item_unload (item);

		iter= g_slist_next (iter);
	}

	itemview_update();
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
	itemview_set_mode (ITEMVIEW_SINGLE_ITEM);

	/* Set current node to search result dummy node so that
	   we except only items from the respective loader for
	   the item view. */
	itemlist->priv->currentNode = item_loader_get_node (loader);

	itemlist_add_loader (loader);
}

ItemList *
itemlist_create (void)
{
	return ITEMLIST (g_object_new (ITEMLIST_TYPE, NULL));
}
