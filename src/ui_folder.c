/*
   GUI folder handling
   
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
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

static gboolean folder_is_empty(folderPtr folder) {
	GtkTreeIter child;
	GtkTreeIter *folderIter = &((ui_data*)(folder->ui_data))->row;
	nodePtr ptr;

	if (gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &child, folderIter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &child,
					    FS_PTR, &ptr,
					    -1);
		if (ptr == NULL)
			return TRUE;
		else
			return FALSE;
	} else
		return TRUE;
	
}

void on_popup_newfolder_selected(void) {
	if(NULL == newfolderdialog || !G_IS_OBJECT(newfolderdialog))
		newfolderdialog = create_newfolderdialog();
		
	gtk_widget_show(newfolderdialog);
}

/* This is used as a hack in order to pass a folderPtr to the save
   button callback. This is very evil and should be changed at some
   point. Unfortunatly, glade does not make it easy to change. */

folderPtr activeFolder = NULL;

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*foldertitleentry;
	gchar *foldertitle;
	folderPtr folder;
	g_assert(newfolderdialog != NULL);
	
	foldertitleentry = lookup_widget(newfolderdialog, "foldertitleentry");
	foldertitle = (gchar *)gtk_entry_get_text(GTK_ENTRY(foldertitleentry));
	folder = restore_folder(folder_get_root(), foldertitle, NULL, FST_FOLDER);
	if(folder) {
		/* add the new folder to the model */
		ui_add_folder(folder, -1);
	} else {
		g_warning("internal error! could not get a new folder key!");
	}	
}

void on_popup_foldername_selected(gpointer callback_data,
						    guint callback_action,
						    GtkWidget *widget) {
	folderPtr folder = (folderPtr)callback_data;
	GtkWidget	*foldernameentry;
	gchar 		*title;
	GtkTreeIter	*iter;
	
	g_assert(folder);
	g_assert(folder->type == FST_FOLDER);
	
	if(NULL == foldernamedialog || !G_IS_OBJECT(foldernamedialog))
		foldernamedialog = create_foldernamedialog();
	
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	title = folder_get_title(folder);
	gtk_entry_set_text(GTK_ENTRY(foldernameentry), title);
	activeFolder = folder;

	gtk_widget_show(foldernamedialog);
	g_free(iter);
}

void on_foldername_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkTreeIter *iter = (GtkTreeIter *)g_malloc(sizeof(GtkTreeIter));
	folderPtr folder = (folderPtr)ui_feedlist_get_selected();
	if (folder && IS_FOLDER(folder->type)) {
		*iter = ((ui_data*)(folder->ui_data))->row;
		on_popup_foldername_selected(iter, 0, NULL);
	} else
		g_message(_("A folder must be selected."));
}

void on_foldernamechangebtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*foldernameentry;
	g_assert(activeFolder);
	
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	setFolderTitle(activeFolder, (gchar *)gtk_entry_get_text(GTK_ENTRY(foldernameentry)));

	gtk_widget_hide(foldernamedialog);
	activeFolder = NULL;
}

void on_popup_removefolder_selected(gpointer callback_data,
							 guint callback_action,
							 GtkWidget *widget) {
	folderPtr		folder = (folderPtr)callback_data;
	
	g_assert(folder);
	g_assert(folder->type == FST_FOLDER);
	
	if(folder_is_empty(folder))
		folder_remove(folder);
	else
		ui_show_error_box(_("A folder must be empty to delete it!"));
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
   
void checkForEmptyFolder(folderPtr folder) {
	GtkTreeIter	*parent = &((ui_data*)(folder->ui_data))->row;
	GtkTreeIter	iter;
	int			count;
	gboolean		valid;
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
					    FS_ICON, icons[ICON_AVAILABLE],
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

void checkForEmptyFolders(void) {
     ui_feedlist_do_for_all(NULL, ACTION_FILTER_FOLDER, checkForEmptyFolder);
}

void ui_add_folder(folderPtr folder, gint position) {
	GtkTreeIter		*iter;
	/* check if a folder with this keyprefix already
	   exists to check config consistency */

	g_assert(folder->ui_data == NULL);

	g_assert(feedstore != NULL);
	g_assert(folder->parent);

	/* if parent is NULL we have the root folder and don't create
	   a new row! */
	folder->ui_data = g_malloc(sizeof(ui_data));
	iter = &(((ui_data*)(folder->ui_data))->row);
	gchar *path = folder_get_conf_path(folder);
	if (folder->parent == folder_get_root()) {
		if (position < 0)
			gtk_tree_store_append(feedstore, iter, NULL);
		else
			gtk_tree_store_insert(feedstore, iter, NULL,position);
	} else {
		if (position < 0)
			gtk_tree_store_append(feedstore, iter, &((ui_data*)folder->parent->ui_data)->row);
		else
			gtk_tree_store_insert(feedstore, iter, &((ui_data*)folder->parent->ui_data)->row,position);
	}
	gtk_tree_store_set(feedstore, iter,
				    FS_PTR, folder,
				    -1);
	g_free(path);

	checkForEmptyFolders();

	ui_update_folder(folder);
}

void ui_remove_folder(folderPtr folder) {
	GtkTreeIter	parent, iter;
	gboolean has_parent, parentExpanded;
	folderPtr parentNode;
	
	g_assert(folder);
	g_assert(folder->ui_data);

	has_parent = gtk_tree_model_iter_parent(GTK_TREE_MODEL(feedstore),&parent, &((ui_data*)(folder->ui_data))->row);
	g_message("%s has a parent: %d", folder->title, has_parent);
	iter = ((ui_data*)(folder->ui_data))->row;
	parentNode = folder->parent;
	parentExpanded = ui_is_folder_expanded(parentNode); /* If the folder becomes empty, the folder would collapse */
	
	gtk_tree_store_remove(feedstore, &iter);
	
	g_free((ui_data*)(folder->ui_data));
	folder->ui_data = NULL;

	if(has_parent) {
		ui_update_folder(parentNode);
		checkForEmptyFolders();
		if (parentExpanded)
			ui_folder_set_expansion(parentNode, TRUE);
	}
}

/* If pos == -1, position == end */
void ui_folder_add_feed(folderPtr parent, feedPtr fp, gint position) {
	GtkTreeIter		*topiter;

	g_assert(NULL != fp);
	g_assert(NULL != parent);
	g_assert(NULL != feedstore);
	g_assert(NULL == fp->ui_data);
	
	if (parent == folder_get_root() || parent == NULL)
		topiter = NULL;
	else {
		g_assert(parent->ui_data);
		topiter = &((ui_data*)(parent->ui_data))->row;
	}

	fp->ui_data = (gpointer)g_new0(ui_data, 1);

	if(position >= 0) {
		/* if a feed entry is marked after which we shall insert */
		gtk_tree_store_insert(feedstore, &((ui_data*)(fp->ui_data))->row, topiter, position);
	} else {
		/* typically on startup when adding feeds from configuration */
		gtk_tree_store_append(feedstore, &((ui_data*)(fp->ui_data))->row, topiter);
	}
	
	gtk_tree_store_set(feedstore, &((ui_data*)(fp->ui_data))->row,
				    FS_PTR, fp,
				    -1);

	checkForEmptyFolders();
	ui_update_feed(fp);
}

void ui_folder_remove_feed(feedPtr fp) {
	GtkTreeIter iter;
	gboolean has_parent;
	GtkTreeIter parent;
	folderPtr parentNode;
	gboolean parentExpanded;

	g_assert(fp);
	g_assert(fp->ui_data);

	has_parent = gtk_tree_model_iter_parent(GTK_TREE_MODEL(feedstore),&parent, &((ui_data*)(fp->ui_data))->row);
	iter = ((ui_data*)(fp->ui_data))->row;
	parentNode = fp->parent;
	parentExpanded = ui_is_folder_expanded(fp->parent); /* If the folder becomes empty, the folder would collapse */
	gtk_tree_store_remove(feedstore, &iter);
	
	g_free((ui_data*)(fp->ui_data));
	fp->ui_data = NULL;

	if(has_parent) {
		ui_update_folder(parentNode);
		checkForEmptyFolders();
		if (parentExpanded)
			ui_folder_set_expansion(parentNode, TRUE);
	}
}
static GdkPixbuf* ui_folder_select_icon(folderPtr np) {
	g_assert(IS_FOLDER(np->type));
	switch(np->type) {
	case FST_FOLDER:
		return icons[ICON_FOLDER];
	case FST_HELPFOLDER:
		return icons[ICON_HELP];
	default:
		g_print(_("internal error! unknown entry type! cannot display appropriate icon!\n"));
		return icons[ICON_UNAVAILABLE];
	}
}

static void ui_update_folder_from_iter(GtkTreeIter *iter) {
	gboolean			rc;
	GtkTreeIter		child, parent;
	gint 			count, ccount;
	gchar			*title, *label;
	GtkTreeModel		*model;
	folderPtr			ptr;
	

	model =  GTK_TREE_MODEL(feedstore);

	gtk_tree_model_get(model, iter,
				    FS_PTR, &ptr,
				    -1);
	g_assert(IS_FOLDER(ptr->type));

	if(folder_get_root() == ptr)
		return;

	g_assert(ptr->ui_data);

	title = folder_get_title(ptr);

	count = 0;
	rc = gtk_tree_model_iter_children(model, &child, iter);

	while(rc) {
		gtk_tree_model_get(model, &child,
					    FS_UNREAD, &ccount,
					    -1);
		count += ccount;
		rc = gtk_tree_model_iter_next(model, &child);
	}
	
	if (count>0) {
		label = g_strdup_printf("<span weight=\"bold\">%s (%d)</span>", title, count);
	} else {
		label = g_strdup_printf("%s", title);
	}

	gtk_tree_store_set(feedstore, iter,
				    FS_LABEL, label,
				    FS_ICON, ui_folder_select_icon(ptr),
				    FS_UNREAD, count,
				    -1);
	g_free(label);

	if(gtk_tree_model_iter_parent(model,&parent, iter))
		ui_update_folder_from_iter(&parent);
}

void ui_update_folder(folderPtr folder) {
	nodePtr node = (nodePtr)folder;
	
	if(NULL != node) {
		g_assert(IS_FOLDER(node->type));
		g_assert(NULL != node->ui_data);
		ui_update_folder_from_iter(&((ui_data*)(node->ui_data))->row);
	}
}
