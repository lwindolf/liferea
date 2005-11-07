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

	node_add(FST_FOLDER);
}

void on_menu_folder_new(GtkMenuItem *menuitem, gpointer user_data) {

	on_popup_newfolder_selected();
}

void ui_folder_newdialog(nodePtr np) {
	GtkWidget	*foldernameentry;
	
	if(NULL == newfolderdialog || !G_IS_OBJECT(newfolderdialog))
		newfolderdialog = create_newfolderdialog();

	foldernameentry = lookup_widget(newfolderdialog, "foldertitleentry");
	gtk_entry_set_text(GTK_ENTRY(foldernameentry), "");
	gtk_object_set_data(GTK_OBJECT(newfolderdialog), "folder", np);
		
	gtk_widget_show(newfolderdialog);
}

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*foldertitleentry;
	gchar		*foldertitle;
	nodePtr		folder, parentNode;
	int		pos;
	
	g_assert(newfolderdialog != NULL);
	
	foldertitleentry = lookup_widget(newfolderdialog, "foldertitleentry");
	foldertitle = (gchar *)gtk_entry_get_text(GTK_ENTRY(foldertitleentry));

	folder = (nodePtr)gtk_object_get_data(GTK_OBJECT(newfolderdialog), "folder");
	node_set_title(folder, foldertitle);
	node_add_data(folder, FST_FOLDER, NULL);

	/* add the new folder to the model */
	parentNode = ui_feedlist_get_target_folder(&pos);
	feedlist_add_node(parentNode, folder, pos);
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
	
	folder = (nodePtr)gtk_object_get_data(GTK_OBJECT(foldernamedialog), "folder");
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	node_set_title(folder, (gchar *)gtk_entry_get_text(GTK_ENTRY(foldernameentry)));
	ui_node_update(folder);
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
   
void ui_node_check_if_folder_is_empty(nodePtr folder) {
	GtkTreeIter	*parent = &((ui_data*)(folder->ui_data))->row;
	GtkTreeIter	iter;
	int		count;
	gboolean	valid;
	nodePtr		np;

	/* this function does two things:
	   
	1. add "(empty)" entry to an empty folder
	2. remove an "(empty)" entry from a non empty folder
	(this state is possible after a drag&drop action) */

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
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter, FS_PTR, &np, -1);

		if(np == NULL) {
			gtk_tree_store_remove(feedstore, &iter);
			return;
		}
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	} while(valid);
}

void ui_node_remove_node(nodePtr np) {
	GtkTreeIter	iter;
	gboolean 	parentExpanded = FALSE;
	
	g_return_if_fail(NULL != np->ui_data);

	iter = ((ui_data*)(np->ui_data))->row;
	if(np->parent != NULL)
		parentExpanded = ui_node_is_folder_expanded(np->parent); /* If the folder becomes empty, the folder would collapse */
	
	gtk_tree_store_remove(feedstore, &iter);
	
	g_free((ui_data*)(np->ui_data));
	np->ui_data = NULL;

	if(np->parent != NULL) {
		ui_node_check_if_folder_is_empty(np->parent);
		if(parentExpanded)
			ui_node_set_expansion(np->parent, TRUE);

		ui_node_update(np->parent);
	}
}

/** determines the feeds favicon or default icon */
static GdkPixbuf* ui_node_get_icon(nodePtr np) {
	gpointer	favicon;
	feedPtr		fp;
	
	favicon = np->icon;

	if(NULL == favicon)
		favicon = icons[ICON_AVAILABLE];

	/* special icons */
	switch(np->type) {
		case FST_FOLDER:
			favicon = icons[ICON_FOLDER];
			break;
		case FST_VFOLDER:
			favicon = icons[ICON_VFOLDER];
			break;
		case FST_FEED:
			fp = (feedPtr)np->data;
		
			if(!feed_get_available(fp))
				favicon = icons[ICON_UNAVAILABLE];

			if(fp->fhp != NULL && fp->fhp->icon < MAX_ICONS)
				return icons[fp->fhp->icon];

			break;
	}

	return favicon;
}

void ui_node_update(nodePtr np) {
	GtkTreeIter	iter;
	gchar		*label;
	int		count;

	if(np->ui_data == NULL)
		return;

	iter = ((ui_data*)np->ui_data)->row;
	
	count = node_get_unread_count(np);
	
	if(count > 0)
		label = g_markup_printf_escaped("<span weight=\"bold\">%s (%d)</span>", node_get_title(np), count);
	else
		label = g_markup_printf_escaped("%s", node_get_title(np));
	
	gtk_tree_store_set(feedstore, &iter, FS_LABEL, label,
	                                    FS_UNREAD, count,
	                                    FS_ICON, ui_node_get_icon(np),
	                                    -1);
	g_free(label);
}
