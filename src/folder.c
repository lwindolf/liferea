/**
 * @file folder.c feed list and plugin callbacks for folders
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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

#include "folder.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "export.h"
#include "feedlist.h"
#include "support.h"
#include "ui/ui_folder.h"
#include "ui/ui_node.h"
#include "fl_sources/node_source.h"

static void folder_import(nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted) {
	
	node_set_data(node, NULL);
	node_add_child(parent, node, -1);
	
	cur = cur->xmlChildrenNode;
	while(cur) {
		if((!xmlStrcmp(cur->name, BAD_CAST"outline")))
		import_parse_outline(cur, node, node->source, trusted);
		cur = cur->next;				
	}
}

static void folder_export(nodePtr node, xmlNodePtr cur, gboolean trusted) {
	
	if(trusted) {
		if(ui_node_is_folder_expanded(node))
			xmlNewProp(cur, BAD_CAST"expanded", BAD_CAST"true");
		else
			xmlNewProp(cur, BAD_CAST"collapsed", BAD_CAST"true");
	}

	debug1(DEBUG_CACHE, "adding folder: title=%s", node_get_title(node));
	export_node_children(node, cur, trusted);	
}

static void folder_initial_load(nodePtr node) {
	node_foreach_child(node, node_initial_load);
}

static void folder_merge_nr_hash(gpointer key, gpointer value, gpointer user_data) {

	g_hash_table_insert((GHashTable *)user_data, key, value);
}

/* This callback is used to compute the itemset of folder nodes */
static void folder_merge_itemset(nodePtr node, gpointer userdata) {
	itemSetPtr	itemSet = (itemSetPtr)userdata;

	debug1(DEBUG_GUI, "merging items of node \"%s\"", node_get_title(node));

	switch(node->type) {
		case NODE_TYPE_FOLDER:
			node_foreach_child_data(node, folder_merge_itemset, itemSet);
			break;
		case NODE_TYPE_VFOLDER:
			return;
			break;
		default:
			debug1(DEBUG_GUI, "   pre merge item set: %d items", g_list_length(itemSet->items));
			itemSet->items = g_list_concat(itemSet->items, g_list_copy(node->itemSet->items));
			if(node->itemSet->nrHashes)
				g_hash_table_foreach(node->itemSet->nrHashes, folder_merge_nr_hash, itemSet->nrHashes);
			debug1(DEBUG_GUI, "  post merge item set: %d items", g_list_length(itemSet->items));
			break;
	}
}

static void folder_load(nodePtr node) {

	node_foreach_child(node, node_load);

	if(0 >= node->loaded) {
		/* Concatenate all child item sets to form the folders item set */
		itemSetPtr itemSet = g_new0(struct itemSet, 1);
		itemSet->type = ITEMSET_TYPE_FOLDER;
		itemSet->nrHashes = g_hash_table_new(g_direct_hash, g_direct_equal);
		node_foreach_child_data(node, folder_merge_itemset, itemSet);
		node_set_itemset(node, itemSet);
	}

	node->loaded++;
}

static void folder_save(nodePtr node) {
	node_foreach_child(node, node_save);
}

static void folder_unload(nodePtr node) {

	if(0 >= node->loaded)
		return;

	if(1 == node->loaded) {
		g_assert(NULL != node->itemSet);
		g_list_free(node->itemSet->items);
		node->itemSet->items = NULL;
	}

	node->loaded--;

	node_foreach_child(node, node_unload);
}

static void folder_reset_update_counter(nodePtr node) {
	node_foreach_child(node, node_reset_update_counter);
}

static void folder_request_update(nodePtr node, guint flags) {
	// FIXME: int -> gpointer
	node_foreach_child_data(node, node_request_update, GUINT_TO_POINTER(flags));
}

static void folder_request_auto_update(nodePtr node) {
	
	node_foreach_child(node, node_request_auto_update);
}

static void folder_remove(nodePtr node) {

	/* remove all children */
	node_foreach_child(node, node_request_remove);
	g_assert(!node->children);
	
	/* remove the folder */
	ui_node_remove_node(node);
}

static void folder_mark_all_read(nodePtr node) {

	node_foreach_child(node, node_mark_all_read);
}

nodeTypePtr folder_get_node_type(void) { 

	static struct nodeType fnti = {
		NODE_CAPABILITY_ADD_CHILDS |
		NODE_CAPABILITY_REMOVE_CHILDS |
		NODE_CAPABILITY_SUBFOLDERS |
		NODE_CAPABILITY_REORDER |
		NODE_CAPABILITY_SHOW_UNREAD_COUNT,
		"folder",
		NULL,
		NODE_TYPE_FOLDER,
		folder_import,
		folder_export,
		folder_initial_load,
		folder_load,
		folder_save,
		folder_unload,
		folder_reset_update_counter,
		folder_request_update,
		folder_request_auto_update,
		folder_remove,
		folder_mark_all_read,
		node_default_render,
		ui_folder_add,
		ui_node_rename
	};
	fnti.icon = icons[ICON_FOLDER];

	return &fnti; 
}

nodeTypePtr root_get_node_type(void) { 

	/* the root node is identical to the folder type,
	   just a different node type... */
	static struct nodeType rnti = {
		NODE_CAPABILITY_ADD_CHILDS |
		NODE_CAPABILITY_REMOVE_CHILDS |
		NODE_CAPABILITY_SUBFOLDERS |
		NODE_CAPABILITY_REORDER |
		NODE_CAPABILITY_SHOW_UNREAD_COUNT,
		"root",
		NULL,		/* and no need for an icon */
		NODE_TYPE_ROOT,
		folder_import,
		folder_export,
		folder_initial_load,
		folder_load,
		folder_save,
		folder_unload,
		folder_reset_update_counter,
		folder_request_update,
		folder_request_auto_update,
		folder_remove,
		folder_mark_all_read,
		node_default_render,
		ui_folder_add,
		ui_node_rename
	};

	return &rnti; 
}
