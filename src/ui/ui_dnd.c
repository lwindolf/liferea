/**
 * @file ui_dnd.c everything concerning DnD
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>		/* For strncmp */
#include "common.h"
#include "feed.h"
#include "feedlist.h"
#include "debug.h"
#include "conf.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_node.h"
#include "ui/ui_dnd.h"
#include "fl_sources/node_source.h"

static gboolean (*old_feed_drop_possible)(GtkTreeDragDest   *drag_dest,
                                          GtkTreePath       *dest_path,
                                          GtkSelectionData  *selection_data);
					  		     
static gboolean (*old_feed_drag_data_received)(GtkTreeDragDest *drag_dest,
                                               GtkTreePath *dest,
                                               GtkSelectionData *selection_data);

/* ---------------------------------------------------------------------------- */
/* GtkTreeDragSource/GtkTreeDragDest implementation				*/
/* ---------------------------------------------------------------------------- */

/** decides wether a feed cannot be dragged or not */
static gboolean ui_dnd_feed_draggable(GtkTreeDragSource *drag_source, GtkTreePath *path) {
	GtkTreeIter	iter;
	nodePtr		node;
	
	debug1(DEBUG_GUI, "DnD check if feed dragging is possible (%d)", path);

	if(gtk_tree_model_get_iter(GTK_TREE_MODEL(drag_source), &iter, path)) {
		gtk_tree_model_get(GTK_TREE_MODEL(drag_source), &iter, FS_PTR, &node, -1);
		
		/* never drag "empty" entries or nodes of read-only subscription lists*/
		if (!node || !(NODE_SOURCE_TYPE (node->parent)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST))
			return FALSE;
		
		return TRUE;
	} else {
		g_warning("fatal error! could not resolve tree path!");
		return FALSE;
	}
}

static gboolean
ui_dnd_feed_drop_possible (GtkTreeDragDest *drag_dest, GtkTreePath *dest_path, GtkSelectionData *selection_data)
{
	GtkTreeIter	iter;
	
	debug1 (DEBUG_GUI, "DnD check if feed dropping is possible (%d)", dest_path);

	/* The only situation when we don't want to drop is when a
	   feed was selected (note you can select drop targets between
	   feeds/folders, a folder or a feed). Dropping onto a feed
	   is not possible with GTK 2.0-2.2 because it disallows to
	   drops as a children but its possible since GTK 2.4 */
		   	
	if (((old_feed_drop_possible) (drag_dest, dest_path, selection_data)) == FALSE)
		return FALSE;
	
	if (gtk_tree_model_get_iter (GTK_TREE_MODEL (drag_dest), &iter, dest_path)) {
	
		nodePtr node;
		
		/* If we get an iterator it's either a possible dropping
		   candidate (a folder or source node to drop into, or a
		   iterator to insert after). In any case we have to check
		   if it is a writeable node source. */

		gtk_tree_model_get (GTK_TREE_MODEL (drag_dest), &iter, FS_PTR, &node, -1);

		/* never drag nodes of read-only subscription lists */
		if (node && !(NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST))
			return FALSE;		
	} else {
		/* we come here if a drop on a feed happens */
		return FALSE;
	}
	return TRUE;
}

static gboolean ui_dnd_feed_drag_data_received(GtkTreeDragDest *drag_dest, GtkTreePath *dest, GtkSelectionData *selection_data) {
	GtkTreeIter	iter, iter2, parentIter;
	nodePtr		node, oldParent, newParent;
	gboolean	result, valid, added;
	gint		oldPos, pos;

	if(TRUE == (result = old_feed_drag_data_received(drag_dest, dest, selection_data))) {
		if(gtk_tree_model_get_iter(GTK_TREE_MODEL(drag_dest), &iter, dest)) {
			gtk_tree_model_get(GTK_TREE_MODEL(drag_dest), &iter, FS_PTR, &node, -1);
			
			/* remove from old parents child list */
			oldParent = node->parent;
			g_assert(oldParent);
			oldPos = g_slist_index(oldParent->children, node);
			oldParent->children = g_slist_remove(oldParent->children, node);
			
			if(0 == g_slist_length(oldParent->children))
				ui_node_add_empty_node(ui_node_to_iter(oldParent->id));
			
			/* and rebuild new parents child list */
			if(gtk_tree_model_iter_parent(GTK_TREE_MODEL(drag_dest), &parentIter, &iter)) {
				gtk_tree_model_get(GTK_TREE_MODEL(drag_dest), &parentIter, FS_PTR, &newParent, -1);
			} else {
				gtk_tree_model_get_iter_first(GTK_TREE_MODEL(drag_dest), &parentIter);
				newParent = feedlist_get_root();
			}

			/* drop old list... */
			debug3(DEBUG_GUI, "old parent is %s (%d, position=%d)", oldParent->title, g_slist_length(oldParent->children), oldPos);
			debug2(DEBUG_GUI, "new parent is %s (%d)", newParent->title, g_slist_length(newParent->children));
			g_slist_free(newParent->children);
			newParent->children = NULL;
			node->parent = newParent;
			
			debug0(DEBUG_GUI, "new new parent child list:");
				
			/* and rebuild it from the tree model */
			if(feedlist_get_root() != newParent)
				valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(drag_dest), &iter2, &parentIter);
			else
				valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(drag_dest), &iter2, NULL);
				
			pos = 0;
			added = FALSE;
			while(valid) {
				nodePtr	child;			
				gtk_tree_model_get(GTK_TREE_MODEL(drag_dest), &iter2, FS_PTR, &child, -1);
				if(child) {
					/* Well this is a bit complicated... If we move a feed inside a folder
					   we need to skip the old insertion point (oldPos). This is easy if the
					   feed is added behind this position. If it is dropped before the flag
					   added is set once the new copy is encountered. The remaining copy
					   is skipped automatically when the flag is set.
					 */
					 
					/* check if this is a copy of the dragged node or the original itself */
					if((newParent == oldParent) && !strcmp(node->id, child->id)) {
						if((pos == oldPos) || added) {
							/* it is the original */
							debug2(DEBUG_GUI, "   -> %d: skipping old insertion point %s", pos, child->title);
						} else {
							/* it is a copy inserted before the original */
							added = TRUE;
							debug2(DEBUG_GUI, "   -> %d: new insertion point of %s", pos, child->title);
							newParent->children = g_slist_append(newParent->children, child);
						}
					} else {
						/* all other nodes */
						debug2(DEBUG_GUI, "   -> %d: adding %s", pos, child->title);
						newParent->children = g_slist_append(newParent->children, child);
					}
				} else {
					debug0(DEBUG_GUI, "   -> removing empty node");
					/* remove possible existing "(empty)" node from newParent */
					ui_node_remove_empty_node(&parentIter);
				}
				valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(drag_dest), &iter2);
				pos++;
			}
			
			node_update_counters (newParent);
			node_update_counters (oldParent);
			
			feedlist_schedule_save();
			ui_itemlist_prefocus();
		}
	}
						      
	return result;
}

void ui_dnd_setup_feedlist(GtkTreeStore *feedstore) {
	GtkTreeDragSourceIface	*drag_source_iface;
	GtkTreeDragDestIface	*drag_dest_iface;

	drag_source_iface = GTK_TREE_DRAG_SOURCE_GET_IFACE(GTK_TREE_MODEL(feedstore));
	drag_source_iface->row_draggable = ui_dnd_feed_draggable;

	drag_dest_iface = GTK_TREE_DRAG_DEST_GET_IFACE(GTK_TREE_MODEL(feedstore));
	old_feed_drop_possible = drag_dest_iface->row_drop_possible;
	old_feed_drag_data_received = drag_dest_iface->drag_data_received;
	drag_dest_iface->row_drop_possible = ui_dnd_feed_drop_possible;
	drag_dest_iface->drag_data_received = ui_dnd_feed_drag_data_received;
}

/* ---------------------------------------------------------------------------- */
/* receiving URLs 								*/
/* ---------------------------------------------------------------------------- */

/* method to receive URLs which were dropped anywhere in the main window */
static void ui_dnd_URL_received(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time) {
	gchar		*tmp1, *tmp2, *freeme;
	
	g_return_if_fail (data->data != NULL);
		
	if((data->length >= 0) && (data->format == 8)) {
		/* extra handling to accept multiple drops (same code in ui_feedlist.c) */	
		freeme = tmp1 = g_strdup(data->data);
		while((tmp2 = strsep(&tmp1, "\n\r"))) {
			if(strlen(tmp2))
				node_request_automatic_add(g_strdup(tmp2),
					                   NULL,
				                           NULL, 
							   NULL,
					                   FEED_REQ_RESET_TITLE |
			                                   FEED_REQ_RESET_UPDATE_INT | 
					                   FEED_REQ_AUTO_DISCOVER | 
					                   FEED_REQ_PRIORITY_HIGH);
		}
		g_free(freeme);
		gtk_drag_finish(context, TRUE, FALSE, time);		
	} else {
		gtk_drag_finish(context, FALSE, FALSE, time);
	}
}

void ui_dnd_setup_URL_receiver(GtkWidget *widget) {

	GtkTargetEntry target_table[] = {
		{ "STRING",     		0, 0 },
		{ "text/plain", 		0, 0 },
		{ "text/uri-list",		0, 1 },
		{ "_NETSCAPE_URL",		0, 1 },
		{ "application/x-rootwin-drop", 0, 2 }
	};

	/* doesn't work with GTK_DEST_DEFAULT_DROP... */
	gtk_drag_dest_set(widget, GTK_DEST_DEFAULT_ALL,
			target_table, sizeof(target_table)/sizeof(target_table[0]),
			GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
		       
	gtk_signal_connect(GTK_OBJECT(widget), "drag_data_received",
			G_CALLBACK(ui_dnd_URL_received), NULL);
}
