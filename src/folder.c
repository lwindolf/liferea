/**
 * @file folder.c feed list callbacks for folders
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "export.h"
#include "feedlist.h"
#include "folder.h"
#include "itemset.h"
#include "node.h"
#include "vfolder.h"
#include "ui/ui_folder.h"
#include "ui/ui_node.h"

static void folder_merge_child_items(nodePtr node, gpointer user_data) {
	itemSetPtr	folderItemSet = (itemSetPtr)user_data;
	itemSetPtr	nodeItemSet;

	nodeItemSet = node_get_itemset(node);
	folderItemSet->ids = g_list_concat(folderItemSet->ids, nodeItemSet->ids);
	nodeItemSet->ids = NULL;
	itemset_free(nodeItemSet);
}

static itemSetPtr folder_load(nodePtr node) {
	itemSetPtr	itemSet;
	
	itemSet = g_new0(struct itemSet, 1);
	itemSet->nodeId = node->id;

	node_foreach_child_data(node, folder_merge_child_items, itemSet);
	return itemSet;
}

static void folder_import(nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted) {
	
	node_set_data(node, NULL);
	node_add_child(parent, node, -1);
	
	cur = cur->xmlChildrenNode;
	while(cur) {
		if(!xmlStrcmp(cur->name, BAD_CAST"outline"))
			import_parse_outline(cur, node, node->source, trusted);
		cur = cur->next;				
	}
}

static void folder_export(nodePtr node, xmlNodePtr cur, gboolean trusted) {
	
	if(trusted) {
		if(ui_node_is_folder_expanded(node->id))
			xmlNewProp(cur, BAD_CAST"expanded", BAD_CAST"true");
		else
			xmlNewProp(cur, BAD_CAST"collapsed", BAD_CAST"true");
	}

	debug1(DEBUG_CACHE, "adding folder: title=%s", node_get_title(node));
	export_node_children(node, cur, trusted);	
}

static void folder_save(nodePtr node) {
	
	/* A folder has no own state but must give all childs the chance to save theirs */
	node_foreach_child(node, node_save);
}

static void
folder_add_child_unread_count (nodePtr node, gpointer user_data)
{
	guint	*unreadCount = (guint *)user_data;

	if (!IS_VFOLDER (node))
		*unreadCount += node->unreadCount;
}

static void folder_update_unread_count(nodePtr node) {

	node->unreadCount = 0;
	node_foreach_child_data(node, folder_add_child_unread_count, &node->unreadCount);
}

static void folder_remove(nodePtr node) {

	/* remove all children */
	node_foreach_child(node, node_request_remove);
	g_assert(!node->children);
	
	/* remove the folder */
	ui_node_remove_node(node);
}

nodeTypePtr folder_get_node_type(void) { 

	static struct nodeType fnti = {
		NODE_CAPABILITY_ADD_CHILDS |
		NODE_CAPABILITY_REMOVE_CHILDS |
		NODE_CAPABILITY_SUBFOLDERS |
		NODE_CAPABILITY_REORDER |
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_UPDATE_CHILDS,
		"folder",
		NULL,
		folder_import,
		folder_export,
		folder_load,
		folder_save,
		folder_update_unread_count,
		NULL,			/* process_update_result */
		folder_remove,
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
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_UPDATE_CHILDS,		
		"root",
		NULL,		/* and no need for an icon */
		folder_import,
		folder_export,
		folder_load,
		folder_save,
		folder_update_unread_count,
		NULL,		/* process_update_result() */
		folder_remove,
		node_default_render,
		ui_folder_add,
		ui_node_rename,
		NULL
	};

	return &rnti; 
}
