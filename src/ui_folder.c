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

extern gchar	*selected_keyprefix;
extern gint	selected_type;

/*------------------------------------------------------------------------------*/
/* new/change/remove folder dialog callbacks 					*/
/*------------------------------------------------------------------------------*/

void on_popup_newfolder_selected(void) {
	if(NULL == newfolderdialog || !G_IS_OBJECT(newfolderdialog))
		newfolderdialog = create_newfolderdialog();
		
	gtk_widget_show(newfolderdialog);
}

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*foldertitleentry;
	gchar		*folderkey, *foldertitle;
	
	g_assert(newfolderdialog != NULL);
	
	foldertitleentry = lookup_widget(newfolderdialog, "foldertitleentry");
	foldertitle = (gchar *)gtk_entry_get_text(GTK_ENTRY(foldertitleentry));
	if(NULL != (folderkey = addFolderToConfig(foldertitle))) {
		/* add the new folder to the model */
		addFolder(folderkey, g_strdup(foldertitle), FST_FOLDER);
		checkForEmptyFolders();
	} else {
		print_status(g_strdup(_("internal error! could not get a new folder key!")));
	}	
}

void on_popup_foldername_selected(void) {
	GtkWidget	*foldernameentry;
	gchar 		*title;

	if(selected_type != FST_FOLDER) {
		showErrorBox(_("You have to select a folder entry!"));
		return;
	}
	
	if(NULL == foldernamedialog || !G_IS_OBJECT(foldernamedialog))
		foldernamedialog = create_foldernamedialog();
		
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	if(NULL != selected_keyprefix) {
		title = getFolderTitle(selected_keyprefix);
		gtk_entry_set_text(GTK_ENTRY(foldernameentry), title);
		g_free(title);
		gtk_widget_show(foldernamedialog);
	} else {
		showErrorBox("internal error: could not determine folder key!");
	}
}

void on_foldername_activate(GtkMenuItem *menuitem, gpointer user_data) { on_popup_foldername_selected(); }

void on_foldernamechangebtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*foldernameentry;
	
	if(NULL != selected_keyprefix) {
		foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
		setFolderTitle(selected_keyprefix, (gchar *)gtk_entry_get_text(GTK_ENTRY(foldernameentry)));
	} else {
		showErrorBox("internal error: could not determine folder key!");
	}

	gtk_widget_hide(foldernamedialog);
}

void on_popup_removefolder_selected(void) {
	GtkTreeStore	*feedstore;
	GtkTreeIter	childiter;
	GtkTreeIter	selected_iter;	
	gint		tmp_type, count;
	
	if(selected_type != FST_FOLDER) {
		showErrorBox(_("You have to select a folder entry!"));
		return;
	}

	ui_feedlist_get_iter(&selected_iter);
	feedstore = getFeedStore();
	g_assert(feedstore != NULL);
	
	/* make sure thats no grouping iterator */
	if(NULL != selected_keyprefix) {
		/* check if folder is empty */
		count = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(feedstore), &selected_iter);
		if(0 == count) 
			g_error("this should not happen! A folder must have an empty entry!!!\n");

		if(1 == count) {
			/* check if the only entry is type FST_EMPTY */
			gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &childiter, &selected_iter);
			gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &childiter, FS_TYPE, &tmp_type, -1);
			if(FST_EMPTY == tmp_type) {
				gtk_tree_store_remove(feedstore, &childiter);		/* remove "(empty)" iter */
				gtk_tree_store_remove(feedstore, &selected_iter);	/* remove folder iter */
				removeFolder(selected_keyprefix);
				g_free(selected_keyprefix);
				selected_keyprefix = g_strdup(ROOT_FOLDER_PREFIX);
			} else {
				showErrorBox(_("A folder must be empty to delete it!"));
			}
		} else {
			showErrorBox(_("A folder must be empty to delete it!"));
		}
	} else {
		print_status(g_strdup(_("Error: Cannot determine folder key!")));
	}
}

/* methos to mark everything in a folder as unread */

static gboolean ui_folder_mark_all_as_read_foreach(GtkTreeModel *model, GtkTreePath	*child_path, GtkTreeIter *child_iter, gpointer user_data) {
	GtkTreePath	*selected = (GtkTreePath *)user_data;
	
	if(gtk_tree_path_is_ancestor(selected, child_path)) {
		ui_feedlist_mark_items_as_unread(child_iter);
	}
	
	return FALSE;
}

void ui_folder_mark_all_as_read(void) {
	GtkTreeModel	*model;
	GtkTreeIter	selected_iter;	
	GtkTreePath	*selected_path;

	if(selected_type != FST_FOLDER) {
		showErrorBox(_("You have to select a folder entry!"));
		return;
	}

	model = GTK_TREE_MODEL(getFeedStore());
	g_assert(NULL != model);

	ui_feedlist_get_iter(&selected_iter);
	selected_path = gtk_tree_model_get_path(model, &selected_iter);	
	g_assert(NULL != selected_path);
	gtk_tree_model_foreach(model, ui_folder_mark_all_as_read_foreach, selected_path);
}

