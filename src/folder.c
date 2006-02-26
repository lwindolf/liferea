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

static void folder_initial_load(nodePtr node) {
	node_foreach_child(node, node_initial_load);
}

static void folder_load(nodePtr node) {
	node_foreach_child(node, node_load);
}

static void folder_save(nodePtr node) {
	node_foreach_child(node, node_save);
}

static void folder_unload(nodePtr node) {

	// FIXME: this does not seem to be correct
	g_list_free(node->itemSet->items);
	node->itemSet->items = NULL;
	node_foreach_child(node, node_unload);
}

static void folder_reset_update_counter(nodePtr node) {
	node_foreach_child(node, node_reset_update_counter);
}

static void folder_request_update(nodePtr node, guint flags) {
	// FIXME: int -> gpointer
	node_foreach_child_data(node, node_request_update, (gpointer)flags);
}

static void folder_request_auto_update(nodePtr node) {
	node_foreach_child(node, node_request_auto_update);
}

static void folder_schedule_update(nodePtr node, guint flags) {

	// FIXME ???
}

static void folder_remove(nodePtr node) {

	/* remove all children */
	node_foreach_child(node, node_remove);
}

static void folder_mark_all_read(nodePtr node) {

	node_foreach_child(node, node_mark_all_read);
}

static gchar * folder_render(nodePtr node) {

	// FIXME: !!!
}

static struct nodeType nti = {
	folder_initial_load,
	folder_load,
	folder_save,
	folder_unload,
	folder_reset_update_counter,
	folder_request_update,
	folder_request_auto_update,
	folder_schedule_update,
	folder_remove,
	folder_mark_all_read,
	folder_render
};

nodeTypePtr folder_get_node_type(void) { return &nti; }
