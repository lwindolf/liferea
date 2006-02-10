/**
 * @file node.c common feed list node handling
 * 
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "node.h"
#include "common.h"
#include "conf.h"
#include "callbacks.h"
#include "favicon.h"
#include "feed.h"
#include "feedlist.h"
#include "update.h"
#include "debug.h"
#include "support.h"
#include "fl_providers/fl_plugin.h"
#include "ui/ui_itemlist.h"

/* returns a unique node id */
gchar * node_new_id() {
	int		i;
	gchar		*id, *filename;
	gboolean	already_used;
	
	id = g_new0(gchar, 10);
	do {
		for(i=0;i<7;i++)
			id[i] = (char)g_random_int_range('a', 'z');
		id[7] = '\0';
		
		filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", id, NULL);
		already_used = g_file_test(filename, G_FILE_TEST_EXISTS);
		g_free(filename);
	} while(already_used);
	
	return id;
}

nodePtr node_new() {
	nodePtr	np;

	np = (nodePtr)g_new0(struct node, 1);
	np->id = node_new_id();
	np->sortColumn = IS_TIME;
	np->sortReversed = TRUE;	/* default sorting is newest date at top */
	np->available = FALSE;
	np->type = FST_INVALID;

	return np;
}

void node_add_data(nodePtr np, guint type, gpointer data) {
	itemSetPtr	sp;

	g_assert(NULL == np->data);

	np->type = type;
	np->data = data;

	/* Vfolders/folders are not handled by the node
	   loading/unloading so the item set must be prepared 
	   upon folder creation */

	if(FST_FOLDER == type) {
		sp = g_new0(struct itemSet, 1);
		sp->type = ITEMSET_TYPE_FOLDER;
		node_set_itemset(np, sp);
	}

	if(FST_VFOLDER == type) {
		sp = g_new0(struct itemSet, 1);
		sp->type = ITEMSET_TYPE_VFOLDER;
		node_set_itemset(np, sp);
	}
}

gboolean node_is_ancestor(nodePtr node1, nodePtr node2) {
	nodePtr	tmp;

	tmp = node2->parent;
	while(tmp) {
		if(node1 == tmp)
			return TRUE;
		tmp = tmp->parent;
	}
	return FALSE;
}

void node_free(nodePtr np) {

	g_assert(0 == np->loaded);
	g_free(np->icon);
	g_free(np->title);
	g_free(np->id);
	g_free(np);
}

void node_load(nodePtr np) {

	debug2(DEBUG_CACHE, "+ node_load (%s, ref count=%d)", node_get_title(np), np->loaded);
	np->loaded++;

	if(1 < np->loaded) {
		debug1(DEBUG_CACHE, "no loading %s because it is already loaded", node_get_title(np));
		return;
	}

	if(NULL == FL_PLUGIN(np)->node_load)
		return;

	switch(np->type) {
		case FST_FEED:
		case FST_PLUGIN:
			g_assert(NULL == np->itemSet);
			FL_PLUGIN(np)->node_load(np);
			g_assert(NULL != np->itemSet);
			break;
		case FST_FOLDER:
		case FST_VFOLDER:
			/* not loading vfolders and other types! */
			break;
		default:
			g_warning("internal error: unknown node type (%d)!", np->type);
			break;
	}

	debug2(DEBUG_CACHE, "- node_load (%s, new ref count=%d)", node_get_title(np), np->loaded);
}

void node_save(nodePtr np) {

	g_assert(0 < np->loaded);
	g_assert(NULL != np->itemSet);

	if(FALSE == np->needsCacheSave)
		return;

	if(NULL == FL_PLUGIN(np)->node_save)
		return;

	FL_PLUGIN(np)->node_save(np);
	np->needsCacheSave = FALSE;
}

void node_unload(nodePtr np) {

	debug2(DEBUG_CACHE, "+ node_unload (%s, ref count=%d)", node_get_title(np), np->loaded);

	if(0 >= np->loaded) {
		debug0(DEBUG_CACHE, "node is not loaded, nothing to do...");
		return;
	}

	node_save(np);	/* save before unloading */

	if(NULL == FL_PLUGIN(np)->node_unload)
		return;

	if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
		if(1 == np->loaded) {
			switch(np->type) {
				case FST_FEED:
				case FST_PLUGIN:
					g_assert(NULL != np->itemSet);
					FL_PLUGIN(np)->node_unload(np);
					g_assert(NULL == np->itemSet);
					break;
				case FST_FOLDER:
				case FST_VFOLDER:
					/* not unloading vfolders and other types! */
					break;
				default:
					g_warning("internal error: unknown node type!");
					break;
			}
		} else {
			debug1(DEBUG_CACHE, "not unloading %s because it is still used", node_get_title(np));
		}
		np->loaded--;
	}

	debug2(DEBUG_CACHE, "- node_unload (%s, new ref count=%d)", node_get_title(np), np->loaded);
}

void node_update_counters(nodePtr np) {
	gint	unreadDiff, newDiff;
	GList	*iter;
	itemPtr	ip;

	newDiff = -1 * np->newCount;
	unreadDiff = -1 * np->unreadCount;
	np->newCount = 0;
	np->unreadCount = 0;

	iter = np->itemSet->items;
	while(NULL != iter) {
		ip = (itemPtr)iter->data;
		if(FALSE == ip->readStatus)
			np->unreadCount++;	
		if(TRUE == ip->newStatus)
			np->newCount++;
		if(TRUE == ip->popupStatus)
			np->popupCount++;
		iter = g_list_next(iter);
	}
	newDiff += np->newCount;
	unreadDiff += np->unreadCount;

	if(FST_VFOLDER == np->type)
		return;		/* prevent recursive counting and adding to statistics */

	/* update parent folder */
	if(NULL != np->parent)
		node_update_unread_count(np->parent, unreadDiff);

	/* propagate to feed list statistics */
	feedlist_update_counters(unreadDiff, newDiff);
}

static void node_merge_item(nodePtr np, itemPtr ip) {

	debug3(DEBUG_UPDATE, "merging \"%s\" (id=%d) to node \"%s\"", item_get_title(ip), ip->nr, node_get_title(np));

	/* step 1: merge into node type internal data structures */
	if(itemset_merge_check(np->itemSet, ip)) {
		debug2(DEBUG_UPDATE, "adding \"%s\" to node \"%s\"...", item_get_title(ip), node_get_title(np));

		/* step 1: add to itemset */
		itemset_add_item(np->itemSet, ip);

		/* step 2: check for matching vfolders */
		vfolder_check_item(ip);

		/* step 3: update feed list statistics */

		/* Never update the overall feed list statistic 
		   for folders and vfolders (because these are item
		   list types with item copies or references)! */
		if((FST_FOLDER != np->type) && (FST_VFOLDER != np->type))
			feedlist_update_counters(ip->readStatus?0:1,
						 ip->newStatus?1:0);
	} else {
		debug2(DEBUG_UPDATE, "not adding \"%s\" to node \"%s\"...", item_get_title(ip), node_get_title(np));
	}
}

/**
 * This method can be used to merge an ordered list of items
 * into the item list of the given item set.
 */
void node_merge_items(nodePtr np, GList *list) {
	GList	*iter;

	/* Items are given in top to bottom display order. 
	   Adding them in this order would mean to reverse 
	   their order in the merged list, so merging needs
	   to be done bottom to top. */
	iter = g_list_last(list);
	while(iter != NULL) {
		node_merge_item(np, ((itemPtr)iter->data));
		iter = g_list_previous(iter);
	}
	g_list_free(list);

	node_update_counters(np);
}

gchar * node_render(nodePtr np) {

	return FL_PLUGIN(np)->node_render(np);
}

void node_request_update(nodePtr np, guint flags) {

	if(FST_VFOLDER == np->type)
		return;

	if(NULL == FL_PLUGIN(np)->node_update)
		return;

	FL_PLUGIN(np)->node_update(np, flags);
}

void node_request_auto_update(nodePtr np) {

	if(FST_VFOLDER == np->type)
		return;

	if(NULL == FL_PLUGIN(np)->node_auto_update)
		return;

	FL_PLUGIN(np)->node_auto_update(np);
}

void node_schedule_update(nodePtr np, request_cb callback, guint flags) {
	feedPtr			fp = (feedPtr)np->data;
	struct request		*request;

	debug1(DEBUG_CONF, "Scheduling %s to be updated", node_get_title(np));

	/* can only be called for feeds, doesn't
	   make sense for other types */
	g_assert(FST_FEED == np->type);

	if(feed_can_be_updated(fp)) {
		ui_mainwindow_set_status_bar(_("Updating \"%s\""), node_get_title(np));
		request = download_request_new();
		request->user_data = np;
		request->callback = ui_feed_process_update_result;
		feed_prepare_request(fp, request, flags);
		download_queue(request);
	} else {
		debug0(DEBUG_CONF, "Update cancelled");
	}
}

void node_remove(nodePtr np) {

	debug_enter("node_remove");

	g_assert(0 != (FL_PLUGIN(np)->capabilities & FL_PLUGIN_CAPABILITY_REMOVE));

	if(NULL != np->icon) {
		g_object_unref(np->icon);
		favicon_remove(np);
	}

	node_unload(np);
	FL_PLUGIN(np)->node_remove(np);
	node_free(np);

	debug_exit("node_remove");
}

void node_add(guint type) {
	nodePtr		parent;
	nodePtr		child;

	debug_enter("node_add");

	parent = feedlist_get_selected_parent();
	debug1(DEBUG_GUI, "new node will be added to folder \"%s\"", node_get_title(parent));

	g_assert(0 != (FL_PLUGIN(parent)->capabilities & FL_PLUGIN_CAPABILITY_ADD));

	child = node_new();
	child->type = type;
	child->handler = parent->handler;
	FL_PLUGIN(parent)->node_add(child);

	debug_exit("node_add");
}

/* ---------------------------------------------------------------------------- */
/* node attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

itemSetPtr node_get_itemset(nodePtr np) { return np->itemSet; }

void node_set_itemset(nodePtr np, itemSetPtr sp) {

	g_assert(NULL == np->itemSet);
	g_assert(ITEMSET_TYPE_INVALID != sp->type);
	np->itemSet = sp;
	sp->node = np;
	node_update_counters(np);
}

void node_set_title(nodePtr np, const gchar *title) {

	g_free(np->title);
	np->title = g_strdup(title);
}

const gchar * node_get_title(nodePtr np) { return np->title; }

const gchar *node_type_to_str(nodePtr np) {

	switch(np->type) {
		case FST_FEED:
			g_assert(NULL != np->data);
			return feed_type_fhp_to_str(((feedPtr)(np->data))->fhp);
			break;
		case FST_VFOLDER:
			return "vfolder";
			break;
		case FST_PLUGIN:
			return "plugin";
			break;
	}

	return NULL;
}

guint node_str_to_type(const gchar *str) {

	g_assert(NULL != str);

	if(g_str_equal(str, "vfolder"))
		return FST_VFOLDER;

	if(g_str_equal(str, "plugin"))
		return FST_PLUGIN;

	if(g_str_equal(str, ""))	/* type maybe "" if initial download is not yet done */
		return FST_FEED;

	if(NULL != feed_type_str_to_fhp(str))
		return FST_FEED;

	return FST_INVALID;
}

void node_update_unread_count(nodePtr np, gint diff) {

	np->unreadCount += diff;

	/* vfolder unread counts are not interesting
	   in the following propagation handling */
	if(FST_VFOLDER == np->type)
		return;

	/* update parent node unread counters */
	if(NULL != np->parent)
		node_update_unread_count(np->parent, diff);

	/* update global feed list statistic */
	feedlist_update_counters(diff, 0);
}

void node_update_new_count(nodePtr np, gint diff) {

	np->newCount += diff;

	/* vfolder new counts are not interesting
	   in the following propagation handling */
	if(FST_VFOLDER == np->type)
		return;

	/* no parent node propagation necessary */

	/* update global feed list statistic */
	feedlist_update_counters(0, diff);
}

guint node_get_unread_count(nodePtr np) { 
	
	return np->unreadCount; 
}

void node_set_id(nodePtr np, const gchar *id) {

	g_free(np->id);
	np->id = g_strdup(id);
}

const gchar *node_get_id(nodePtr np) { return np->id; }

void node_set_sort_column(nodePtr np, gint sortColumn, gboolean reversed) {

	np->sortColumn = sortColumn;
	np->sortReversed = reversed;
	feedlist_schedule_save();
}

void node_set_two_pane_mode(nodePtr np, gboolean newMode) { np->twoPane = newMode; }

gboolean node_get_two_pane_mode(nodePtr np) { return np->twoPane; }


