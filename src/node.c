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
#include "folder.h"
#include "itemset.h"
#include "vfolder.h"
#include "update.h"
#include "debug.h"
#include "support.h"
#include "fl_sources/node_source.h"

static GHashTable *nodes = NULL;
static GSList *nodeTypes = NULL;

void node_type_register(nodeTypePtr nodeType) {

	/* all attributes and methods are mandatory! */
	g_assert(nodeType->id);
	g_assert(0 != nodeType->type);
	g_assert(nodeType->import);
	g_assert(nodeType->export);
	g_assert(nodeType->initial_load);
	g_assert(nodeType->load);
	g_assert(nodeType->save);
	g_assert(nodeType->unload);
	g_assert(nodeType->reset_update_counter);
	g_assert(nodeType->request_update);
	g_assert(nodeType->request_auto_update);
	g_assert(nodeType->remove);
	g_assert(nodeType->mark_all_read);
	g_assert(nodeType->render);
	g_assert(nodeType->request_add);
	g_assert(nodeType->request_properties);
	
	nodeTypes = g_slist_append(nodeTypes, (gpointer)nodeType);
}

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

nodePtr node_from_id(const gchar *id) {

	g_assert(NULL != nodes);
	return (nodePtr)g_hash_table_lookup(nodes, id);
}

nodePtr node_new(void) {
	nodePtr	node;
	
	node = (nodePtr)g_new0(struct node, 1);
	node->id = node_new_id();
	node->sortColumn = IS_TIME;
	node->sortReversed = TRUE;	/* default sorting is newest date at top */
	node->available = TRUE;
	node->type = NODE_TYPE_INVALID;
	node_set_icon(node, NULL);	/* initialize favicon file name */
	
	if(!nodes)
		nodes = g_hash_table_new(g_str_hash, g_str_equal);
		
	g_hash_table_insert(nodes, node->id, node);

	return node;
}

static nodeTypePtr node_get_type_impl(guint type) {
	GSList	*iter = nodeTypes;
	
	while(iter) {
		if(type == ((nodeTypePtr)iter->data)->type)
			return iter->data;		
		iter = g_slist_next(iter);
	}
	
	g_error("node_get_type_impl(): fatal unknown node type %u", type);
	return NULL;
}

void node_set_type(nodePtr node, nodeTypePtr type) {

	node->nodeType = type;
	node->type = type?type->type:NODE_TYPE_INVALID;
}

void node_set_data(nodePtr node, gpointer data) {

	g_assert(NULL == node->data);
	g_assert(NULL != node->nodeType);

	node->data = data;

	/* Vfolders are not handled by the node
	   loading/unloading so the item set must be prepared 
	   upon folder creation */

	if(NODE_TYPE_VFOLDER == NODE_TYPE(node)->type) {
		itemSetPtr itemSet = g_new0(struct itemSet, 1);
		itemSet->type = ITEMSET_TYPE_VFOLDER;
		node_set_itemset(node, itemSet);
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

void node_free(nodePtr node) {

	g_assert(NULL == node->children);
	
	g_hash_table_remove(nodes, node);
	
	update_cancel_requests((gpointer)node);

	if(node->loaded)  {
		itemset_remove_items(node->itemSet);
		g_free(node->itemSet);
		node->itemSet = NULL;
	}

	if(node->icon)
		g_object_unref(node->icon);
	g_free(node->iconFile);
	g_free(node->title);
	g_free(node->id);
	g_free(node);
}

void node_update_counters(nodePtr node) {
	gint	unreadDiff, newDiff;
	GList	*iter;

	newDiff = -1 * node->newCount;
	unreadDiff = -1 * node->unreadCount;
	node->newCount = 0;
	node->unreadCount = 0;

	iter = node->itemSet->items;
	while(iter) {
		itemPtr item = (itemPtr)iter->data;
		if(!item->readStatus)
			node->unreadCount++;	
		if(item->newStatus)
			node->newCount++;
		if(item->popupStatus)
			node->popupCount++;
		iter = g_list_next(iter);
	}
	newDiff += node->newCount;
	unreadDiff += node->unreadCount;

	if(NODE_TYPE_VFOLDER == node->type)
		return;		/* prevent recursive counting and adding to statistics */

	/* update parent folder */
	if(node->parent)
		node_update_unread_count(node->parent, unreadDiff);

	/* propagate to feed list statistics */
	if(NODE_TYPE_FEED == node->type)
		feedlist_update_counters(unreadDiff, newDiff);		
}

void node_update_unread_count(nodePtr node, gint diff) {

	node->unreadCount += diff;

	/* vfolder unread counts are not interesting
	   in the following propagation handling */
	if(NODE_TYPE_VFOLDER == node->type)
		return;

	/* update parent node unread counters */
	if(NULL != node->parent)
		node_update_unread_count(node->parent, diff);

	/* update global feed list statistic */
	if(NODE_TYPE_FEED == node->type)
		feedlist_update_counters(diff, 0);
}

void node_update_new_count(nodePtr node, gint diff) {

	node->newCount += diff;

	/* vfolder new counts are not interesting
	   in the following propagation handling */
	if(NODE_TYPE_VFOLDER == node->type)
		return;

	/* no parent node propagation necessary */

	/* update global feed list statistic */
	if(NODE_TYPE_FEED == node->type)
		feedlist_update_counters(0, diff);	
}

guint node_get_unread_count(nodePtr node) { 
	
	return node->unreadCount; 
}

/* generic node item set merging functions */

static guint node_get_max_item_count(nodePtr node) {

	switch(node->type) {
		case NODE_TYPE_FEED:
			return feed_get_max_item_count(node);
			break;
		default:
			return G_MAXUINT;
	}
}

/**
 * Determine wether a given item is to be merged
 * into the itemset or if it was already added.
 */
static gboolean node_merge_check(itemSetPtr itemSet, itemPtr item) {

	switch(itemSet->type) {
		case ITEMSET_TYPE_FEED:
			return feed_merge_check(itemSet, item);
			break;
		case ITEMSET_TYPE_FOLDER:
		case ITEMSET_TYPE_VFOLDER:
		default:
			g_warning("node_merge_check(): If this happens something is wrong!");
			break;
	}
	
	return FALSE;
}

/* only to be called by node_merge_items() */
static void node_merge_item(nodePtr node, itemPtr item) {

	debug3(DEBUG_UPDATE, "trying to merge \"%s\" (id=%d) to node \"%s\"", item_get_title(item), item->nr, node_get_title(node));

	/* step 1: merge into node type internal data structures */
	if(node_merge_check(node->itemSet, item)) {
		g_assert(!item->sourceNode);
		
		/* step 1: add to itemset */
		itemset_prepend_item(node->itemSet, item);
		
		debug3(DEBUG_UPDATE, "-> adding \"%s\" to item set %p with #%d...", item_get_title(item), node->itemSet, item->nr);
		
		/* step 2: check for matching vfolders */
		vfolder_check_item(item);
	} else {
		debug2(DEBUG_UPDATE, "-> not adding \"%s\" to node \"%s\"...", item_get_title(item), node_get_title(node));
		item_free(item);
	}
}

/**
 * This method can be used to merge an ordered list of items
 * into the item list of the nodes item set.
 */
void node_merge_items(nodePtr node, GList *list) {
	GList	*iter;
	guint	max;
	
	if(debug_level & DEBUG_UPDATE) {
		debug4(DEBUG_UPDATE, "old item set %p (lastItemNr=%lu) of \"%s\" (%p):", node->itemSet, node->itemSet->lastItemNr, node_get_title(node), node);
		iter = node->itemSet->items;
		while(iter) {
			itemPtr item = (itemPtr)iter->data;
			debug2(DEBUG_UPDATE, " -> item #%d \"%s\"", item->nr, item_get_title(item));
			iter = g_list_next(iter);
		}
	}
	
	/* Truncate the new itemset if it is longer than
	   the maximum cache size which could cause items
	   to be dropped and added again on subsequent 
	   merges with the same feed content */
	max = node_get_max_item_count(node);
	if(g_list_length(list) > max) {
		debug3(DEBUG_UPDATE, "item list too long (%u, max=%u) when merging into \"%s\"!", g_list_length(list), max, node_get_title(node));
		guint i = 0;
		GList *iter, *copy;
		iter = copy = g_list_copy(list);
		while(iter) {
			i++;
			if(i > max) {
				itemPtr item = (itemPtr)iter->data;
				debug2(DEBUG_UPDATE, "ignoring item nr %u (%s)...", i, item_get_title(item));
				item_free(item);
				list = g_list_remove(list, item);
			}
			iter = g_list_next(iter);
		}
		g_list_free(copy);
	}	   

	/* Items are given in top to bottom display order. 
	   Adding them in this order would mean to reverse 
	   their order in the merged list, so merging needs
	   to be done bottom to top. */
	iter = g_list_last(list);
	while(iter) {
		node_merge_item(node, ((itemPtr)iter->data));
		iter = g_list_previous(iter);
	}
	g_list_free(list);
	
	/* Never update the overall feed list statistic 
	   for folders and vfolders (because these are item
	   list types with item copies or references)! */
	if((NODE_TYPE_FOLDER != node->type) && (NODE_TYPE_VFOLDER != node->type))
		node_update_counters(node);
}

void node_update_favicon(nodePtr node) {

	if(NODE_TYPE_FEED != node->type)
		return;

	debug1(DEBUG_UPDATE, "favicon of node %s needs to be updated...", node->title);
	feed_update_favicon(node);
}

/* plugin and import callbacks and helper functions */

const gchar *node_type_to_str(nodePtr node) {

	switch(node->type) {
		case NODE_TYPE_FEED:
			g_assert(NULL != node->data);
			return feed_type_fhp_to_str(((feedPtr)(node->data))->fhp);
			break;
		default:
			return NODE_TYPE(node)->id;
			break;
	}

	return NULL;
}

nodeTypePtr node_str_to_type(const gchar *str) {
	GSList	*iter = nodeTypes;

	g_assert(NULL != str);

	if(g_str_equal(str, ""))	/* type maybe "" if initial download is not yet done */
		return feed_get_node_type();

	if(NULL != feed_type_str_to_fhp(str))
		return feed_get_node_type();
		
	/* check against all node types */
	while(iter) {
		if(g_str_equal(str, ((nodeTypePtr)iter->data)->id))
			return (nodeTypePtr)iter->data;
		iter = g_slist_next(iter);
	}

	return NULL;
}

void node_add_child(nodePtr parent, nodePtr node, gint position) {

	if(!parent)
		parent = ui_feedlist_get_target_folder(&position);	

	parent->children = g_slist_insert(parent->children, node, position);
	node->parent = parent;
	
	/* new node may be provided by another feed list handler, if 
	   not they are handled by the parents handler */
	if(!node->source)
		node->source = parent->source;
	
	ui_node_add(parent, node, position);	
	ui_node_update(node);
}

/* To be called by node type implementations to add nodes */
void node_add(nodePtr node, nodePtr parent, gint pos, guint flags) {

	debug1(DEBUG_GUI, "new node will be added to folder \"%s\"", node_get_title(parent));

	ui_feedlist_get_target_folder(&pos);

	node_add_child(parent, node, pos);
	node_request_update(node, flags);
}

/* Interactive node adding (e.g. feed menu->new subscription) */
void node_request_interactive_add(guint type) {
	nodeTypePtr	nodeType;
	nodePtr		parent;

	parent = feedlist_get_insertion_point();

	if(0 == (NODE_TYPE(parent->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
		return;

	nodeType = node_get_type_impl(type);
	nodeType->request_add(parent);
}

/* Automatic subscription adding (e.g. URL DnD), creates a new feed node
   without any user interaction. */
void node_request_automatic_add(const gchar *source, const gchar *title, const gchar *filter, updateOptionsPtr options, gint flags) {
	nodePtr		node, parent;
	gint		pos;

	g_assert(NULL != source);

	parent = feedlist_get_insertion_point();

	if(0 == (NODE_SOURCE_TYPE(parent->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS))
		return;

	node = node_new();
	node_set_type(node,feed_get_node_type());
	node_set_title(node, title?title:_("New Subscription"));
	node_set_data(node, feed_new(source, filter, options));

	ui_feedlist_get_target_folder(&pos);
	node_add_child(parent, node, pos);
	node_request_update(node, flags);
}

void node_request_remove(nodePtr node) {

	node_remove(node);

	node->parent->children = g_slist_remove(node->parent->children, node);

	node_free(node);
}

/* wrapper for node type interface */

void node_import(nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted) {
	NODE_TYPE(node)->import(node, parent, cur, trusted);
}

void node_export(nodePtr node, xmlNodePtr cur, gboolean trusted) {
	NODE_TYPE(node)->export(node, cur, trusted);
}

void node_initial_load(nodePtr node) {
	NODE_TYPE(node)->initial_load(node);
}

void node_load(nodePtr node) {
	NODE_TYPE(node)->load(node);
}

void node_save(nodePtr node) {
	NODE_TYPE(node)->save(node);
}

void node_unload(nodePtr node) {
	NODE_TYPE(node)->unload(node);
}

void node_remove(nodePtr node) {
	NODE_TYPE(node)->remove(node);
}

void node_mark_all_read(nodePtr node) {

	if(0 != node->unreadCount) {
		node_load(node);
		NODE_TYPE(node)->mark_all_read(node);
		node_unload(node);
	}
}

gchar * node_render(nodePtr node) {
	return NODE_TYPE(node)->render(node);
}

void node_reset_update_counter(nodePtr node) {
	NODE_TYPE(node)->reset_update_counter(node);
}

void node_request_auto_update(nodePtr node) {
	NODE_TYPE(node)->request_auto_update(node);
}

void node_request_update(nodePtr node, guint flags) {
	NODE_TYPE(node)->request_update(node, flags);
}

void node_request_properties(nodePtr node) {
	NODE_TYPE(node)->request_properties(node);
}

/* node attributes encapsulation */

itemSetPtr node_get_itemset(nodePtr node) { return node->itemSet; }

void node_set_itemset(nodePtr node, itemSetPtr itemSet) {
	GList	*iter;

	g_assert(ITEMSET_TYPE_INVALID != itemSet->type);
	g_assert(NULL == itemSet->node);
	node->itemSet = itemSet;
	itemSet->node = node;
	
	iter = itemSet->items;
	while(iter) {
		itemPtr	item = (itemPtr)(iter->data);
		
		/* When the item has no source node yet (after
		   loading a feed from cache) it will be set here.
		   If node_set_itemset() is called for folders
		   and vfolders the items already should have a 
		   sourceNode. */
		if(!item->sourceNode)
			item->sourceNode = node;
			
		iter = g_list_next(iter);
	}
	
	node_update_counters(node);
}

void node_set_title(nodePtr node, const gchar *title) {

	g_free(node->title);
	node->title = g_strdup(title);
}

const gchar * node_get_title(nodePtr node) { return node->title; }

void node_set_icon(nodePtr node, gpointer icon) {

	if(node->icon) 
		g_object_unref(node->icon);
	node->icon = icon;

	if(node->icon)
		node->iconFile = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "favicons", node->id, "png");
	else
		node->iconFile = g_strdup(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "default.png");
}

/** determines the nodes favicon or default icon */
gpointer node_get_icon(nodePtr node) { 
	gpointer icon;

	icon = node->icon;

	if(!icon)
		icon = NODE_TYPE(node)->icon;

	if(!node->available)
		icon = icons[ICON_UNAVAILABLE];

	return icon;
}

const gchar * node_get_favicon_file(nodePtr node) { return node->iconFile; }

void node_set_id(nodePtr node, const gchar *id) {

	g_free(node->id);
	node->id = g_strdup(id);
}

const gchar *node_get_id(nodePtr node) { return node->id; }

void node_set_sort_column(nodePtr node, gint sortColumn, gboolean reversed) {

	node->sortColumn = sortColumn;
	node->sortReversed = reversed;
	feedlist_schedule_save();
}

void node_set_two_pane_mode(nodePtr node, gboolean newMode) { node->twoPane = newMode; }

gboolean node_get_two_pane_mode(nodePtr node) { return node->twoPane; }

/* node children iterating interface */

void node_foreach_child_full(nodePtr node, gpointer func, gint params, gpointer user_data) {
	GSList		*children, *iter;
	
	g_assert(NULL != node);

	/* We need to copy because func might modify the list */
	iter = children = g_slist_copy(node->children);
	while(iter) {
		nodePtr childNode = (nodePtr)iter->data;
		
		/* Apply the method to the child */
		if(0 == params)
			((nodeActionFunc)func)(childNode);
		else 
			((nodeActionDataFunc)func)(childNode, user_data);
			
		/* Never descend! */

		iter = g_slist_next(iter);
	}
	
	g_slist_free(children);
}

