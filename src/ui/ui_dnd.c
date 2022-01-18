/**
 * @file ui_dnd.c everything concerning Drag&Drop
 *
 * Copyright (C) 2003-2012 Lars Windolf <lars.windolf@gmx.de>
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

#include <string.h>		/* For strcmp */
#include "common.h"
#include "db.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "debug.h"
#include "ui/item_list_view.h"
#include "ui/feed_list_view.h"
#include "ui/liferea_shell.h"
#include "ui/ui_dnd.h"
#include "fl_sources/node_source.h"

/*
    Why does Liferea need such a complex DnD handling (for the feed list)?

     -> Because parts of the feed list might be un-draggable.
     -> Because drag source and target might be different node sources
	with even incompatible subscription types.
     -> Because removal at drag source and insertion at drop target
        must be atomic to avoid subscription losses.

    For simplicity the DnD code reuses the UI node removal and insertion
    methods that asynchronously apply the actions at the node source.

    (FIXME: implement the last part)
 */

static gboolean (*old_feed_drop_possible)(GtkTreeDragDest   *drag_dest,
                                          GtkTreePath       *dest_path,
                                          GtkSelectionData  *selection_data);

static gboolean (*old_feed_drag_data_received)(GtkTreeDragDest *drag_dest,
                                               GtkTreePath *dest,
                                               GtkSelectionData *selection_data);

/* GtkTreeDragSource/GtkTreeDragDest implementation				*/

/** decides whether a feed cannot be dragged or not */
static gboolean
ui_dnd_feed_draggable (GtkTreeDragSource *drag_source, GtkTreePath *path)
{
	GtkTreeIter	iter;
	nodePtr		node;

	debug1 (DEBUG_GUI, "DnD check if feed dragging is possible (%d)", path);

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_source), &iter, path)) {
		gtk_tree_model_get (GTK_TREE_MODEL (drag_source), &iter, FS_PTR, &node, -1);

		/* never drag "empty" entries or nodes of read-only subscription lists*/
		if (!node || !(NODE_SOURCE_TYPE (node->parent)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST))
			return FALSE;

		return TRUE;
	} else {
		g_warning ("fatal error! could not resolve tree path!");
		return FALSE;
	}
}

static gboolean
ui_dnd_feed_drop_possible (GtkTreeDragDest *drag_dest, GtkTreePath *dest_path, GtkSelectionData *selection_data)
{
	GtkTreeModel	*model = NULL;
	GtkTreePath	*src_path = NULL;
	GtkTreeIter	iter;
	nodePtr		sourceNode, targetNode;

	debug1 (DEBUG_GUI, "DnD check if feed dropping is possible (%d)", dest_path);

	if (!(old_feed_drop_possible) (drag_dest, dest_path, selection_data))
		return FALSE;

	if (!gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_dest), &iter, dest_path))
		return FALSE;

	/* Try to get an iterator, if we get none it means either feed list
	   root or an "Empty" node. Both cases are fine */
	gtk_tree_model_get (GTK_TREE_MODEL (drag_dest), &iter, FS_PTR, &targetNode, -1);
	if (!targetNode)
		return TRUE;

	/* If we got an iterator it's either a possible dropping
	   candidate (a folder or source node to drop into, or a
	   iterator to insert after). In any case we have to check
	   if it is a writeable node source. */

	/* Never drop into read-only subscription node sources */
	if (!(NODE_SOURCE_TYPE (targetNode)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST))
		return FALSE;

	/* never drag folders into non-hierarchic node sources */
	if (!gtk_tree_get_row_drag_data (selection_data, &model, &src_path))
		return TRUE;

	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (model), &iter, src_path)) {
		gtk_tree_model_get (GTK_TREE_MODEL (model), &iter, FS_PTR, &sourceNode, -1);

		g_assert (sourceNode);

		/* Never drop into another node source as this arises to many problems
		   (e.g. remote sync, different subscription type, e.g. SF #2855990) */
		if (NODE_SOURCE_TYPE (targetNode) != NODE_SOURCE_TYPE (sourceNode))
			return FALSE;

		if (IS_FOLDER(sourceNode) && !(NODE_SOURCE_TYPE (targetNode)->capabilities & NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST))
			return FALSE;
	}

	gtk_tree_path_free (src_path);

	return TRUE;
}

static gboolean
ui_dnd_feed_drag_data_received (GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data)
{
	GtkTreeIter	iter, iter2, parentIter;
	nodePtr		node, oldParent, newParent;
	gboolean	result, valid, added;
	gint		oldPos, pos;

	result = old_feed_drag_data_received (drag_dest, dest, selection_data);
	if (result) {
		if (gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_dest), &iter, dest)) {
			gtk_tree_model_get (GTK_TREE_MODEL (drag_dest), &iter, FS_PTR, &node, -1);

			/* If we don't do anything, then because DnD is implemented by removal and
			   re-insertion, and the removed node is selected, the treeview selects
			   the next row after the removal, which is supremely irritating.
			   But setting a selection at this point is pointless, because the treeview
			   will reset it as soon as the DnD callback returns. Instead, we set
			   the cursor, which controls where treeview resets the selection later.
			 */
			gtk_tree_view_set_cursor(GTK_TREE_VIEW (liferea_shell_lookup ("feedlist")),
			    dest, NULL, FALSE);

			/* remove from old parents child list */
			oldParent = node->parent;
			g_assert (oldParent);
			oldPos = g_slist_index (oldParent->children, node);
			oldParent->children = g_slist_remove (oldParent->children, node);
			node_update_counters (oldParent);

			if (0 == g_slist_length (oldParent->children))
				feed_list_view_add_empty_node (feed_list_view_to_iter (oldParent->id));

			/* and rebuild new parents child list */
			if (gtk_tree_model_iter_parent (GTK_TREE_MODEL (drag_dest), &parentIter, &iter)) {
				gtk_tree_model_get (GTK_TREE_MODEL (drag_dest), &parentIter, FS_PTR, &newParent, -1);
			} else {
				gtk_tree_model_get_iter_first (GTK_TREE_MODEL (drag_dest), &parentIter);
				newParent = feedlist_get_root ();
			}

			/* drop old list... */
			debug3 (DEBUG_GUI, "old parent is %s (%d, position=%d)", oldParent->title, g_slist_length (oldParent->children), oldPos);
			debug2 (DEBUG_GUI, "new parent is %s (%d)", newParent->title, g_slist_length (newParent->children));
			g_slist_free (newParent->children);
			newParent->children = NULL;
			node->parent = newParent;

			debug0 (DEBUG_GUI, "new newParent child list:");

			/* and rebuild it from the tree model */
			if (feedlist_get_root() != newParent)
				valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (drag_dest), &iter2, &parentIter);
			else
				valid = gtk_tree_model_iter_children (GTK_TREE_MODEL (drag_dest), &iter2, NULL);

			pos = 0;
			added = FALSE;
			while (valid) {
				nodePtr	child;
				gtk_tree_model_get (GTK_TREE_MODEL (drag_dest), &iter2, FS_PTR, &child, -1);
				if (child) {
					/* Well this is a bit complicated... If we move a feed inside a folder
					   we need to skip the old insertion point (oldPos). This is easy if the
					   feed is added behind this position. If it is dropped before the flag
					   added is set once the new copy is encountered. The remaining copy
					   is skipped automatically when the flag is set.
					 */

					/* check if this is a copy of the dragged node or the original itself */
					if ((newParent == oldParent) && !strcmp(node->id, child->id)) {
						if ((pos == oldPos) || added) {
							/* it is the original */
							debug2 (DEBUG_GUI, "   -> %d: skipping old insertion point %s", pos, child->title);
						} else {
							/* it is a copy inserted before the original */
							added = TRUE;
							debug2 (DEBUG_GUI, "   -> %d: new insertion point of %s", pos, child->title);
							newParent->children = g_slist_append (newParent->children, child);
						}
					} else {
						/* all other nodes */
						debug2 (DEBUG_GUI, "   -> %d: adding %s", pos, child->title);
						newParent->children = g_slist_append (newParent->children, child);
					}
				} else {
					debug0 (DEBUG_GUI, "   -> removing empty node");
					/* remove possible existing "(empty)" node from newParent */
					feed_list_view_remove_empty_node (&parentIter);
				}
				valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (drag_dest), &iter2);
				pos++;
			}

			db_node_update (node);
			node_update_counters (newParent);

			if (NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_REPARENT_NODE)
				NODE_SOURCE_TYPE (node)->reparent_node(node, oldParent, newParent);

			feedlist_schedule_save ();
		}
	}

	return result;
}

void
ui_dnd_setup_feedlist (GtkTreeStore *feedstore)
{
	GtkTreeDragSourceIface	*drag_source_iface;
	GtkTreeDragDestIface	*drag_dest_iface;

	drag_source_iface = GTK_TREE_DRAG_SOURCE_GET_IFACE (GTK_TREE_MODEL (feedstore));
	drag_source_iface->row_draggable = ui_dnd_feed_draggable;

	drag_dest_iface = GTK_TREE_DRAG_DEST_GET_IFACE (GTK_TREE_MODEL (feedstore));
	old_feed_drop_possible = drag_dest_iface->row_drop_possible;
	old_feed_drag_data_received = drag_dest_iface->drag_data_received;
	drag_dest_iface->row_drop_possible = ui_dnd_feed_drop_possible;
	drag_dest_iface->drag_data_received = ui_dnd_feed_drag_data_received;
}
