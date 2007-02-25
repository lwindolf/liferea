/**
 * @file folder.c feed list and plugin callbacks for folders
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

static void folder_save(nodePtr node) {
	
	/* A folder has no own state but must give all childs the chance to save theirs */
	node_foreach_child(node, node_save);
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
		folder_save,
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
		folder_save,
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
