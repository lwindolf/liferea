/**
 * @file folder.c  sub folders for hierarchic node sources
 *
 * Copyright (C) 2006-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "node_providers/folder.h"

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "itemset.h"
#include "node.h"
#include "node_providers/vfolder.h"
#include "ui/feed_list_view.h"
#include "ui/icons.h"
#include "ui/liferea_dialog.h"

/* Note: The folder node type implements the behaviour of a folder like
   node in the feed list. The two most important features are viewing the
   unread items of all child feeds and displaying the aggregated unread count
   of all child feeds.

   The folder node type does not implement the hierarchy of the feed list! */

static void
folder_merge_child_items (Node *node, gpointer user_data)
{
	itemSetPtr	folderItemSet = (itemSetPtr)user_data;
	itemSetPtr	nodeItemSet;

	nodeItemSet = node_get_itemset (node);
	folderItemSet->ids = g_list_concat (folderItemSet->ids, nodeItemSet->ids);
	nodeItemSet->ids = NULL;
	itemset_free (nodeItemSet);
}

static itemSetPtr
folder_load (Node *node)
{
	itemSetPtr	itemSet;

	itemSet = g_new0 (struct itemSet, 1);
	itemSet->nodeId = node->id;

	node_foreach_child_data (node, folder_merge_child_items, itemSet);
	return itemSet;
}

static void
folder_import (Node *node, Node *parent, xmlNodePtr cur, gboolean trusted)
{
	/* Folders have no special properties to be imported. */
}

static void
folder_export (Node *node, xmlNodePtr cur, gboolean trusted)
{
	/* Folders have no special properties to be exported. */
}

static void
folder_save (Node *node)
{
	/* A folder has no own state but must give all childs the chance to save theirs */
	node_foreach_child (node, node_save);
}

static void
folder_add_child_update_counters (Node *node, gpointer user_data)
{
	guint	*unreadCount = (guint *)user_data;

	if (!IS_VFOLDER (node))
		*unreadCount += node->unreadCount;
}

static void
folder_update_counters (Node *node)
{
	/* We never need a total item count for folders.
	   Only the total unread count is interesting */
	node->itemCount = 0;
	node->unreadCount = 0;
	node_foreach_child_data (node, folder_add_child_update_counters, &node->unreadCount);
}

static void
folder_remove (Node *node)
{
	/* Ensure that there are no children anymore */
	g_assert (!node->children);
}

static void
on_newfolder_clicked (GtkButton *button, gpointer user_data)
{	
	feedlist_add_folder (liferea_dialog_entry_get (GTK_WIDGET (user_data), "foldertitleentry"));
}

static gboolean
folder_add_dialog (void)
{
	GtkWidget *dialog = liferea_dialog_new ("new_folder");
	liferea_dialog_entry_set (dialog, "foldertitleentry", "");
	g_signal_connect (liferea_dialog_lookup (dialog, "newfolderbtn"), "clicked", G_CALLBACK (on_newfolder_clicked), dialog);
	
	return TRUE;
}

nodeProviderPtr
folder_get_provider (void)
{
	static struct nodeProvider fnti = {
		NODE_CAPABILITY_SHOW_ITEM_FAVICONS |
		NODE_CAPABILITY_ADD_CHILDS |
		NODE_CAPABILITY_REMOVE_CHILDS |
		NODE_CAPABILITY_SUBFOLDERS |
		NODE_CAPABILITY_REORDER |
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_UPDATE_CHILDS |
		NODE_CAPABILITY_EXPORT,
		"folder",
		ICON_FOLDER,
		folder_import,
		folder_export,
		folder_load,
		folder_save,
		folder_update_counters,
		folder_remove,
		folder_add_dialog,
		feed_list_view_rename_node,
		NULL
	};

	return &fnti;
}

nodeProviderPtr
root_get_provider (void)
{
	/* the root node is identical to the folder type,
	   just a different node type... */
	static struct nodeProvider rnti = {
		NODE_CAPABILITY_ADD_CHILDS |
		NODE_CAPABILITY_REMOVE_CHILDS |
		NODE_CAPABILITY_SUBFOLDERS |
		NODE_CAPABILITY_REORDER |
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_UPDATE_CHILDS |
		NODE_CAPABILITY_EXPORT,
		"root",
		0,		/* and no need for an icon */
		folder_import,
		folder_export,
		folder_load,
		folder_save,
		folder_update_counters,
		folder_remove,
		folder_add_dialog,
		feed_list_view_rename_node,
		NULL
	};

	return &rnti;
}
