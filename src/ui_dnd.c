/**
 * @file ui_dnd.c everything concerning DnD
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include <string.h>		/* For strncmp */
#include "net/os-support.h"	/* for strsep */
#include "support.h"
#include "callbacks.h"
#include "feed.h"
#include "folder.h"
#include "debug.h"
#include "conf.h"
#include "ui_folder.h"
#include "ui_dnd.h"

extern GtkTreeStore *feedstore;

static gboolean (*old_drop_possible) (GtkTreeDragDest   *drag_dest,
							   GtkTreePath       *dest_path,
							   GtkSelectionData  *selection_data);

/* ---------------------------------------------------------------------------- */
/* DnD callbacks								*/
/* ---------------------------------------------------------------------------- */

void on_feedlist_drag_end(GtkWidget *widget, GdkDragContext  *drag_context, gpointer user_data) {

	ui_feedlist_update();
	ui_folder_check_if_empty();
	conf_feedlist_save();
	ui_itemlist_prefocus();
}

/* ---------------------------------------------------------------------------- */
/* GtkTreeDragSource/GtkTreeDragDest implementation				*/
/* ---------------------------------------------------------------------------- */

/** decides wether a feed cannot be dragged or not */
static gboolean 
ui_dnd_feed_draggable(GtkTreeDragSource *drag_source, GtkTreePath *path) {
	GtkTreeIter	iter;
	nodePtr		ptr;
	
	debug1(DEBUG_GUI, "DnD check if dragging is possible (%d)", path);

	g_assert(NULL != feedstore);	
	if(gtk_tree_model_get_iter(GTK_TREE_MODEL(feedstore), &iter, path)) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter, FS_PTR, &ptr, -1);
		
		/* everything besides "empty" entries may be dragged */		
		if(ptr == NULL)
			return FALSE;
		return TRUE;
	} else {
		g_warning("fatal error! could not resolve tree path!");
		return FALSE;
	}
}

/** decides wether a feed cannot be dropped onto a user selection tree position or not */
static gboolean 
ui_dnd_feed_drop_possible(GtkTreeDragDest *drag_dest, GtkTreePath *dest_path, GtkSelectionData *selection_data) {
	GtkTreeModel	*tree_model;
	GtkTreeStore	*tree_store;
	GtkTreeIter	iter;
	
	debug1(DEBUG_GUI, "DnD check if dropping is possible (%d)", dest_path);

	/* The only situation when we don't want to drop is when a
	   feed was selected (note you can select drop targets between
	   feeds/folders, a folder or a feed). Dropping onto a feed
	   is not possible with GTK 2.0-2.2 because it disallows to
	   drops as a children but its possible since GTK 2.4 */
		   	
	tree_model = GTK_TREE_MODEL (drag_dest);
	tree_store = GTK_TREE_STORE (drag_dest);

	if(((old_drop_possible)(drag_dest, dest_path, selection_data)) == FALSE)
		return FALSE;
	
	if(gtk_tree_model_get_iter(tree_model, &iter, dest_path)) {
		/* if we get an iterator its either a folder or the feed 
		   iterator after the insertion point */
	} else {
		/* we come here if a drop on a feed happens */
		return FALSE;
	}
	return TRUE;
}

void ui_dnd_init(void) {
	GtkTreeDragSourceIface	*drag_source_iface = NULL;
	GtkTreeDragDestIface	*drag_dest_iface = NULL;

	g_assert(NULL != feedstore);
	
	if(NULL != (drag_source_iface = GTK_TREE_DRAG_SOURCE_GET_IFACE(GTK_TREE_MODEL(feedstore)))) {
		drag_source_iface->row_draggable = ui_dnd_feed_draggable;
	}

	if(NULL != (drag_dest_iface = GTK_TREE_DRAG_DEST_GET_IFACE(GTK_TREE_MODEL(feedstore)))) {
		old_drop_possible = drag_dest_iface->row_drop_possible;
		drag_dest_iface->row_drop_possible = ui_dnd_feed_drop_possible;
	}
}

/* ---------------------------------------------------------------------------- */
/* receiving URLs 								*/
/* ---------------------------------------------------------------------------- */

/* method to receive URLs which were dropped anywhere in the main window */
static void ui_dnd_URL_received(GtkWidget *mainwindow, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time) {
	gchar		*tmp1, *tmp2, *freeme;
	
	g_return_if_fail (data->data != NULL);
		
	if((data->length >= 0) && (data->format == 8)) {
		/* extra handling to accept multiple drops (same code in ui_feedlist.c) */	
		freeme = tmp1 = g_strdup(data->data);
		while((tmp2 = strsep(&tmp1, "\n\r"))) {
			if(0 != strlen(tmp2))
				ui_feedlist_new_subscription(g_strdup(tmp2), NULL, FEED_REQ_SHOW_PROPDIALOG | FEED_REQ_RESET_TITLE | FEED_REQ_RESET_UPDATE_INT);
		}
		g_free(freeme);
		gtk_drag_finish(context, TRUE, FALSE, time);		
	} else {
		gtk_drag_finish(context, FALSE, FALSE, time);
	}
}

/* sets up URL receiving */
void ui_dnd_setup_URL_receiver(GtkWidget *mainwindow) {

	GtkTargetEntry target_table[] = {
		{ "STRING",     		0, 0 },
		{ "text/plain", 		0, 0 },
		{ "text/uri-list",		0, 1 },
		{ "_NETSCAPE_URL",		0, 1 },
		{ "application/x-rootwin-drop", 0, 2 }
	};

	/* doesn't work with GTK_DEST_DEFAULT_DROP... */
	gtk_drag_dest_set(mainwindow, GTK_DEST_DEFAULT_ALL,
			target_table, sizeof(target_table)/sizeof(target_table[0]),
			GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
		       
	gtk_signal_connect(GTK_OBJECT(mainwindow), "drag_data_received",
			G_CALLBACK(ui_dnd_URL_received), NULL);
}
