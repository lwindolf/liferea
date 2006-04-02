/**
 * @file ui_node.c GUI folder handling
 * 
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include "support.h"
#include "interface.h"
#include "callbacks.h"
#include "conf.h"
#include "debug.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_node.h"

extern GHashTable 	*flIterHash;

GtkTreeIter * ui_node_to_iter(nodePtr node) {

	return (GtkTreeIter *)g_hash_table_lookup(flIterHash, (gpointer)node);
}

/*
 * Expansion & Collapsing
 */

gboolean ui_node_is_folder_expanded(nodePtr folder) {
	GtkTreePath	*path;
	GtkTreeIter	*iter;
	gboolean 	expanded = FALSE;

	if( (iter = ui_node_to_iter(folder)) ) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), iter);
		expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(lookup_widget(mainwindow, "feedlist")), path);
		gtk_tree_path_free(path);
	}

	return expanded;
}

void ui_node_set_expansion(nodePtr folder, gboolean expanded) {
	GtkTreeIter		*iter;
	GtkTreePath		*path;
	GtkWidget		*treeview;	

	treeview = lookup_widget(mainwindow, "feedlist");
	if( (iter = ui_node_to_iter(folder)) ) {
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), iter);
		if(expanded)
			gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, FALSE);
		else
			gtk_tree_view_collapse_row(GTK_TREE_VIEW(treeview), path);
		gtk_tree_path_free(path);
	}
}

/* Subfolders */

/* this function is a workaround to the cant-drop-rows-into-emtpy-
   folders-problem, so we simply pack an (empty) entry into each
   empty folder like Nautilus does... */
   
void ui_node_check_if_folder_is_empty(nodePtr folder) {
	GtkTreeIter	*parent;
	GtkTreeIter	iter;
	int		count;
	gboolean	valid;
	nodePtr		np;

	debug1(DEBUG_GUI, "folder empty check for \"%s\"", node_get_title(folder));

	/* this function does two things:
	   
	1. add "(empty)" entry to an empty folder
	2. remove an "(empty)" entry from a non empty folder
	(this state is possible after a drag&drop action) */

	parent = ui_node_to_iter(folder);
	debug1(DEBUG_GUI, "folder empty check for \"%s\"", node_get_title(folder));
	count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(feedstore), parent);
	debug1(DEBUG_GUI, "folder empty check for \"%s\"", node_get_title(folder));
	
	/* case 1 */
	if(0 == count) {
		gtk_tree_store_append(feedstore, &iter, parent);
		gtk_tree_store_set(feedstore, &iter,
		                   FS_LABEL, _("<i>(empty)</i>"), /* FIXME: Should this be italicized? */
		                   FS_ICON, icons[ICON_EMPTY],
		                   FS_PTR, NULL,
		                   FS_UNREAD, 0,
		                   -1);
		return;
	}
	
	if(1 == count)
		return;
	
	/* else we could have case 2 */
	gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &iter, parent);
	do {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter, FS_PTR, &np, -1);

		if(np == NULL) {
			gtk_tree_store_remove(feedstore, &iter);
			return;
		}
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	} while(valid);
}

void ui_node_add(nodePtr parent, nodePtr node, gint position) {
	GtkTreeIter	*iter, *parentIter = NULL;

	debug2(DEBUG_GUI, "adding node \"%s\" as child of parent=\"%s\"", node_get_title(node), (NULL != parent)?node_get_title(parent):"feed list root");

	g_assert(NULL != parent);
	g_assert(NULL == ui_node_to_iter(node));

	/* if parent is NULL we have the root folder and don't create a new row! */
	iter = (GtkTreeIter *)g_new0(GtkTreeIter, 1);
	
	if(!(parent->type == FST_ROOT))
		parentIter = ui_node_to_iter(parent);
	
	if(position < 0)
		gtk_tree_store_append(feedstore, iter, parentIter);
	else
		gtk_tree_store_insert(feedstore, iter, parentIter, position);

	gtk_tree_store_set(feedstore, iter, FS_PTR, node, -1);
	g_hash_table_insert(flIterHash, (gpointer)node, (gpointer)iter);

	ui_node_update(node);
	
	if(!(parent->type == FST_ROOT))
		ui_node_check_if_folder_is_empty(parent);

	if(FST_FOLDER == node->type)
		ui_node_check_if_folder_is_empty(node);
}

void ui_node_remove_node(nodePtr np) {
	GtkTreeIter	*iter;
	gboolean 	parentExpanded = FALSE;
	
	iter = ui_node_to_iter(np);
	g_return_if_fail(NULL != iter);

	if(np->parent)
		parentExpanded = ui_node_is_folder_expanded(np->parent); /* If the folder becomes empty, the folder would collapse */
	
	gtk_tree_store_remove(feedstore, iter);
	g_hash_table_remove(flIterHash, iter);
	
	if(np->parent) {
		ui_node_check_if_folder_is_empty(np->parent);
		if(parentExpanded)
			ui_node_set_expansion(np->parent, TRUE);

		ui_node_update(np->parent);
	}
}

/** determines the feeds favicon or default icon */
GdkPixbuf* ui_node_get_icon(nodePtr node) {
	gpointer	favicon;
	feedPtr		feed;

	favicon = node->icon;

	if(!favicon)
		favicon = icons[ICON_DEFAULT];

	/* special icons */
	switch(node->type) {
		case FST_FOLDER:
			favicon = icons[ICON_FOLDER];
			break;
		case FST_VFOLDER:
			favicon = icons[ICON_VFOLDER];
			break;
		case FST_FEED:
			feed = (feedPtr)node->data;

			if(!feed->available)
				favicon = icons[ICON_UNAVAILABLE];

			if(!favicon && feed->fhp && (feed->fhp->icon != 0))
				favicon = icons[feed->fhp->icon];

			break;
	}

	return favicon;
}

void ui_node_update(nodePtr np) {
	GtkTreeIter	*iter;
	gchar		*label;

	iter = ui_node_to_iter(np);
	if(!iter)
		return;

	gint count = node_get_unread_count(np);
	
	if(count > 0)
		label = g_markup_printf_escaped("<span weight=\"bold\">%s (%d)</span>", node_get_title(np), count);
	else
		label = g_markup_printf_escaped("%s", node_get_title(np));
	
	gtk_tree_store_set(feedstore, iter, FS_LABEL, label,
	                                    FS_UNREAD, count,
	                                    FS_ICON, ui_node_get_icon(np),
	                                    -1);
	g_free(label);

	/* Not sure if it is good to have the parent
	   folder recursion here... (Lars) */
	if(np->parent)
		ui_node_update(np->parent);
}
