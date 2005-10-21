/**
 * @file ui_folder.c GUI folder handling
 * 
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
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
#include "ui_feedlist.h"
#include "ui_node.h"

static GtkWidget	*newfolderdialog = NULL;
static GtkWidget	*foldernamedialog = NULL;

/*------------------------------------------------------------------------------*/
/* new/change/remove folder dialog callbacks 					*/
/*------------------------------------------------------------------------------*/

void on_popup_newfolder_selected(void) {
	GtkWidget	*foldernameentry;
	
	if(NULL == newfolderdialog || !G_IS_OBJECT(newfolderdialog))
		newfolderdialog = create_newfolderdialog();

	foldernameentry = lookup_widget(newfolderdialog, "foldertitleentry");
	gtk_entry_set_text(GTK_ENTRY(foldernameentry), "");
		
	gtk_widget_show(newfolderdialog);
}

void on_menu_folder_new(GtkMenuItem *menuitem, gpointer user_data) {

	on_popup_newfolder_selected();
}

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*foldertitleentry;
	gchar		*foldertitle;
	nodePtr		folder, parentNode;
	int		pos;
	
	g_assert(newfolderdialog != NULL);
	
	foldertitleentry = lookup_widget(newfolderdialog, "foldertitleentry");
	foldertitle = (gchar *)gtk_entry_get_text(GTK_ENTRY(foldertitleentry));

	folder = node_new();
	node_set_title(folder, foldertitle);
	node_add_data(folder, FST_FOLDER, NULL);

	/* add the new folder to the model */
	parentNode = ui_feedlist_get_target_folder(&pos);
	ui_feedlist_add(parentNode, folder, pos);
	ui_feedlist_select(folder);
}

void on_popup_foldername_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	nodePtr	folder = (nodePtr)callback_data;
	GtkWidget	*foldernameentry;
	
	if((NULL == folder) || (FST_FOLDER != folder->type)) {
		ui_show_error_box(_("A folder must be selected."));
		return;
	}
	
	if(NULL == foldernamedialog || !G_IS_OBJECT(foldernamedialog))
		foldernamedialog = create_foldernamedialog();
	
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	gtk_entry_set_text(GTK_ENTRY(foldernameentry), node_get_title(folder));
	gtk_object_set_data(GTK_OBJECT(foldernamedialog), "folder", folder);

	gtk_widget_show(foldernamedialog);
}

void on_foldernamechangebtn_clicked(GtkButton *button, gpointer user_data) {
	nodePtr		folder;
	GtkWidget	*foldernameentry;
	
	folder = gtk_object_get_data(GTK_OBJECT(foldernamedialog), "folder");
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	node_set_title(folder, (gchar *)gtk_entry_get_text(GTK_ENTRY(foldernameentry)));
	ui_feedlist_update();
	gtk_widget_hide(foldernamedialog);
}

/*
 * Expansion & Collapsing
 */

gboolean ui_node_is_folder_expanded(nodePtr folder) {
	GtkTreeIter		*iter;
	GtkTreePath		*path;
	GtkWidget		*treeview;
	gboolean expanded = FALSE;

	g_assert(NULL != feedstore);

	if(folder->ui_data == NULL)
		return FALSE;

	if(NULL != (treeview = lookup_widget(mainwindow, "feedlist"))) {
		iter = &((ui_data*)(folder->ui_data))->row;
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), iter);
		expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(treeview), path);
		gtk_tree_path_free(path);
	}
	return expanded;
}

void ui_node_set_expansion(nodePtr folder, gboolean expanded) {
	GtkTreeIter		*iter;
	GtkTreePath		*path;
	GtkWidget		*treeview;	

	treeview = lookup_widget(mainwindow, "feedlist");
	iter = &((ui_data*)((nodePtr)folder)->ui_data)->row;
	path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), iter);
	if(expanded)
		gtk_tree_view_expand_row(GTK_TREE_VIEW(treeview), path, FALSE);
	else
		gtk_tree_view_collapse_row(GTK_TREE_VIEW(treeview), path);
	gtk_tree_path_free(path);
}

/* Subfolders */

/* this function is a workaround to the cant-drop-rows-into-emtpy-
   folders-problem, so we simply pack an (empty) entry into each
   empty folder like Nautilus does... */
   
void ui_node_empty_check(nodePtr folder) {
	GtkTreeIter	*parent = &((ui_data*)(folder->ui_data))->row;
	GtkTreeIter	iter;
	int		count;
	gboolean	valid;
	nodePtr		ptr;
	/* this function does two things:
	   
	1. add "(empty)" entry to an empty folder
	2. remove an "(empty)" entry from a non empty folder
	(this state is possible after a drag&drop action) */

	/* key is folder keyprefix, value is folder tree iterator */
	count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(feedstore), parent);
	
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
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter, FS_PTR, &ptr, -1);

		if(ptr == NULL) {
			gtk_tree_store_remove(feedstore, &iter);
			return;
		}
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	} while(valid);
}

void ui_node_check_if_folder_is_empty(nodePtr folder) {

	ui_feedlist_do_for_all(folder, ACTION_FILTER_FOLDER, ui_node_empty_check);
}

void ui_node_remove_node(nodePtr np) {
	GtkTreeIter	iter;
	gboolean 	parentExpanded = FALSE;
	nodePtr 	parentNode;
	
	g_return_if_fail(NULL != np->ui_data);

	parentNode = ui_feedlist_get_parent(np);

	iter = ((ui_data*)(np->ui_data))->row;
	parentNode = ui_feedlist_get_parent(np);
	if(parentNode != NULL)
		parentExpanded = ui_node_is_folder_expanded(parentNode); /* If the folder becomes empty, the folder would collapse */
	
	gtk_tree_store_remove(feedstore, &iter);
	
	g_free((ui_data*)(np->ui_data));
	np->ui_data = NULL;

	if(parentNode != NULL) {
		ui_node_check_if_folder_is_empty(parentNode);
		if(parentExpanded)
			ui_node_set_expansion(parentNode, TRUE);
	}
}

void ui_node_update(nodePtr np) {

	// FIXME: update node, icon, title, unread count
}
