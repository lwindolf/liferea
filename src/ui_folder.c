/**
 * @file ui_folder.c GUI folder handling
 * 
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include <gtk/gtk.h>
#include "support.h"
#include "interface.h"
#include "callbacks.h"
#include "conf.h"
#include "folder.h"
#include "ui_feedlist.h"
#include "ui_folder.h"

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

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*foldertitleentry;
	gchar		*foldertitle;
	folderPtr	folder;
	folderPtr	parent;
	int		pos;
	
	g_assert(newfolderdialog != NULL);
	
	foldertitleentry = lookup_widget(newfolderdialog, "foldertitleentry");
	foldertitle = (gchar *)gtk_entry_get_text(GTK_ENTRY(foldertitleentry));
	folder = restore_folder(NULL, foldertitle, NULL, FST_FOLDER);
	if(folder) {
		/* add the new folder to the model */
		parent = ui_feedlist_get_target_folder(&pos);
		ui_feedlist_add(parent, (nodePtr)folder, pos);
		ui_feedlist_select((nodePtr)folder);
	} else {
		g_warning("internal error! could not get a new folder key!");
	}	
}

void on_popup_foldername_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	folderPtr	folder = (folderPtr)callback_data;
	GtkWidget	*foldernameentry;
	gchar 		*title;
	
	if((NULL == folder) || (FST_FOLDER != folder->type)) {
		ui_show_error_box(_("A folder must be selected."));
		return;
	}
	
	if(NULL == foldernamedialog || !G_IS_OBJECT(foldernamedialog))
		foldernamedialog = create_foldernamedialog();
	
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	title = folder_get_title(folder);
	gtk_entry_set_text(GTK_ENTRY(foldernameentry), title);
	gtk_object_set_data(GTK_OBJECT(foldernamedialog), "folder", folder);

	gtk_widget_show(foldernamedialog);
}

void on_foldernamechangebtn_clicked(GtkButton *button, gpointer user_data) {
	folderPtr folder;
	GtkWidget	*foldernameentry;
	
	folder = gtk_object_get_data(GTK_OBJECT(foldernamedialog), "folder");
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	folder_set_title(folder, (gchar *)gtk_entry_get_text(GTK_ENTRY(foldernameentry)));
	ui_feedlist_update();
	gtk_widget_hide(foldernamedialog);
}

void ui_folder_remove(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	folderPtr	folder = (folderPtr)callback_data;
	
	g_assert((NULL != folder) && (FST_FOLDER == folder->type));
	folder_free(folder);
}

/*
 * Expansion & Collapsing
 */

gboolean ui_is_folder_expanded(folderPtr folder) {
	GtkTreeIter		*iter;
	GtkTreePath		*path;
	GtkWidget		*treeview;
	gboolean expanded = FALSE;

	g_assert(NULL != feedstore);

	if (folder->ui_data == NULL)
		return FALSE;

	if(NULL != (treeview = lookup_widget(mainwindow, "feedlist"))) {
		iter = &((ui_data*)(folder->ui_data))->row;
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), iter);
		expanded = gtk_tree_view_row_expanded(GTK_TREE_VIEW(treeview), path);
		gtk_tree_path_free(path);
	}
	return expanded;
}

void ui_folder_set_expansion(folderPtr folder, gboolean expanded) {
	GtkTreeIter		*iter;
	GtkTreePath		*path;
	GtkWidget		*treeview;	

	g_assert(NULL != feedstore);
	
	if(NULL != (treeview = lookup_widget(mainwindow, "feedlist"))) {
		iter = &((ui_data*)((nodePtr)folder)->ui_data)->row;
		path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), iter);
		if (expanded)
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
   
void ui_folder_empty_check(folderPtr folder) {
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

void ui_folder_check_if_empty(void) {

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FOLDER, ui_folder_empty_check);
}

void ui_folder_remove_node(nodePtr ptr) {
	GtkTreeIter	iter;
	gboolean 	parentExpanded = FALSE;
	folderPtr 	parent;
	
	g_assert(NULL != ptr);
	g_return_if_fail(NULL != ptr->ui_data);

	parent = ui_feedlist_get_parent(ptr);

	iter = ((ui_data*)(ptr->ui_data))->row;
	parent = ui_feedlist_get_parent(ptr);
	if(parent != NULL)
		parentExpanded = ui_is_folder_expanded(parent); /* If the folder becomes empty, the folder would collapse */
	
	gtk_tree_store_remove(feedstore, &iter);
	
	g_free((ui_data*)(ptr->ui_data));
	ptr->ui_data = NULL;

	if(parent != NULL) {
		ui_folder_check_if_empty();
		if (parent != NULL && parentExpanded)
			ui_folder_set_expansion(parent, TRUE);
	}
}

static GdkPixbuf* ui_folder_select_icon(folderPtr np) {

	g_assert(FST_FOLDER == np->type);
	switch(np->type) {
		case FST_FOLDER:
			return icons[ICON_FOLDER];
		default:
			g_print(_("internal error! unknown entry type! cannot display appropriate icon!\n"));
			return icons[ICON_UNAVAILABLE];
	}
}

static void ui_folder_update_from_iter(GtkTreeIter *iter) {
	gboolean		rc;
	GtkTreeIter		child;
	gint 			count, ccount;
	gchar			*title, *label;
	GtkTreeModel		*model;
	folderPtr		ptr;
	
	model = GTK_TREE_MODEL(feedstore);

	gtk_tree_model_get(model, iter, FS_PTR, &ptr, -1);
	
	g_assert(FST_FOLDER == ptr->type);
	g_assert(ptr != NULL);
	g_assert(ptr->ui_data);

	title = g_markup_escape_text(folder_get_title(ptr), -1);

	count = 0;
	rc = gtk_tree_model_iter_children(model, &child, iter);

	while(rc) {
		gtk_tree_model_get(model, &child, FS_UNREAD, &ccount, -1);
		count += ccount;
		rc = gtk_tree_model_iter_next(model, &child);
	}
	
	if(count>0) {
		label = g_strdup_printf("<span weight=\"bold\">%s (%d)</span>", title, count);
	} else {
		label = g_strdup_printf("%s", title);
	}

	gtk_tree_store_set(feedstore, iter,
	                              FS_LABEL, label,
	                              FS_ICON, ui_folder_select_icon(ptr),
	                              FS_UNREAD, count,
	                              -1);
	g_free(title);
	g_free(label);
}

void ui_folder_update(folderPtr folder) {
	nodePtr node = (nodePtr)folder;
	
	if(NULL != node) {
		g_assert(FST_FOLDER == node->type);
		g_assert(NULL != node->ui_data);
		ui_folder_update_from_iter(&((ui_data*)(node->ui_data))->row);
	}
}
