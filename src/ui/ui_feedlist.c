/**
 * @file ui_feedlist.c GUI feed list handling
 *
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2005 Raphaël Slinckx <raphael@slinckx.net>
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

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include "support.h"
#include "interface.h"
#include "callbacks.h"
#include "common.h"
#include "feedlist.h"
#include "conf.h"
#include "update.h"
#include "favicon.h"
#include "debug.h"
#include "ui_feedlist.h"
#include "ui_mainwindow.h"
#include "ui_feed.h"
#include "ui_vfolder.h"
#include "ui_tabs.h"
#include "ui_queue.h"

extern GtkWidget	*mainwindow;
extern GHashTable	*feedHandler;

GHashTable		*flIterHash = NULL;	/* hash table used for fast feed -> tree iter lookup */

GtkTreeModel		*filter;
GtkTreeStore		*feedstore = NULL;

/* flag to enable/disable the GtkTreeModel filter */
gboolean filter_feeds_without_unread_headlines = FALSE;

nodePtr ui_feedlist_get_target_folder(int *pos) {
	nodePtr		np;
	GtkTreeIter	*iter;
	GtkTreePath 	*path;
	gint		*indices;

	if(NULL != pos)
		*pos = -1;
	
	if(NULL == (np = feedlist_get_selected()))
		return NULL;
	

	if(filter_feeds_without_unread_headlines) {
		gtk_tree_model_filter_convert_child_iter_to_iter(GTK_TREE_MODEL_FILTER(filter), iter, ui_node_to_iter(np));
	} else {
		iter = ui_node_to_iter(np);
	}

	if(FST_FOLDER == np->type) {
		return np;
	} else {
		path = gtk_tree_model_get_path(gtk_tree_view_get_model(GTK_TREE_VIEW(lookup_widget(mainwindow, "feedlist"))), iter);
		indices = gtk_tree_path_get_indices(path);
		if(NULL != pos)
			*pos = indices[gtk_tree_path_get_depth(path)-1] + 1;
		gtk_tree_path_free(path);
		return np->parent;
	}
}

static void ui_feedlist_selection_changed_cb(GtkTreeSelection *selection, gpointer data) {
	GtkTreeIter		iter;
	GtkTreeModel		*model;
	nodePtr			np;
	GdkGeometry		geometry;
	gint			type = FST_INVALID;

	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, FS_PTR, &np, -1);
		if(np != NULL) 
			type = np->type;

		debug1(DEBUG_GUI, "feed list selection changed to \"%s\"", node_get_title(np));

		/* make sure thats no grouping iterator */
		if((FST_FEED == type) || (FST_VFOLDER == type) || (FST_PLUGIN == type)) {
			
			/* FIXME: another workaround to prevent strange window
			   size increasings after feed selection changing */
			geometry.min_height=480;
			geometry.min_width=640;
			g_assert(mainwindow != NULL);
			gtk_window_set_geometry_hints(GTK_WINDOW(mainwindow), mainwindow, &geometry, GDK_HINT_MIN_SIZE);
		
			ui_tabs_show_headlines();
			itemlist_set_two_pane_mode(node_get_two_pane_mode(np));
			
			/* workaround to ensure the feedlist is focussed when we click it
			   (Mozilla might prevent this, ui_itemlist_display() depends on this */
			gtk_widget_grab_focus(lookup_widget(mainwindow, "feedlist"));
		}
		
		/* update feed list and item list states */
		feedlist_selection_changed(np);
	} else {
		/* If we cannot get the new selection we keep the old one
		   this happens when we're doing drag&drop for example. */
	}
	ui_mainwindow_update_feed_menu(type);
}

static void ui_feedlist_row_activated_cb(GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data) {
	GtkTreeIter iter;
	nodePtr ptr;
	
	gtk_tree_model_get_iter(gtk_tree_view_get_model(tv), &iter, path);
	gtk_tree_model_get(gtk_tree_view_get_model(tv), &iter, FS_PTR, &ptr, -1);
	if((NULL != ptr) && (FST_FOLDER == ptr->type)) {
		if (gtk_tree_view_row_expanded(tv, path))
			gtk_tree_view_collapse_row(tv, path);
		else
			gtk_tree_view_expand_row(tv,path,FALSE);
	}

}

static gboolean ui_feedlist_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data) {

	if((event->type == GDK_KEY_PRESS) &&
	   (event->state == 0) &&
	   (event->keyval == GDK_Delete)) {
		nodePtr np = feedlist_get_selected();
		
		if(NULL != np) {
			if(event->state & GDK_SHIFT_MASK)
				ui_feedlist_remove_node(np);
			else
				ui_feedlist_delete_prompt(np);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean filter_visible_function(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
	gint		count;
	nodePtr		np;

	if(!filter_feeds_without_unread_headlines)
		return TRUE;
		
	gtk_tree_model_get(model, iter, FS_PTR, &np, FS_UNREAD, &count, -1);

	if(0 != count)
		return TRUE;
	else
		return FALSE;
}

/* Sets either the unread feeds filter model or the standard
   GTK tree model. This is necessary because only the standard
   model supports drag and drop. */
void ui_feedlist_set_model(GtkTreeView *feedview, GtkTreeStore *feedstore, gboolean filtered) {
	GtkTreeModel	*model;
		
	if(filtered) {
		filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(feedstore), NULL);

		gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(filter),
	        	                               filter_visible_function,
	                	                       NULL,
	                        	               NULL);
		model = GTK_TREE_MODEL(filter);
	} else {
		model = GTK_TREE_MODEL(feedstore);
	}
	gtk_tree_view_set_model(GTK_TREE_VIEW(feedview), model);
}

/* sets up the entry list store and connects it to the entry list
   view in the main window */
void ui_feedlist_init(GtkWidget *feedview) {
	GtkCellRenderer		*textRenderer;
	GtkCellRenderer		*iconRenderer;	
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;	
	
	debug_enter("ui_feedlist_init");

	g_assert(mainwindow != NULL);
	g_assert(feedview != NULL);

	flIterHash = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, NULL);

	/* Set up store */
	feedstore = gtk_tree_store_new(FS_LEN,
	                               G_TYPE_STRING,
	                               GDK_TYPE_PIXBUF,
	                               G_TYPE_POINTER,
	                               G_TYPE_INT);

	ui_feedlist_set_model(GTK_TREE_VIEW(feedview), feedstore, FALSE);

	/* we only render the state and title */
	iconRenderer = gtk_cell_renderer_pixbuf_new();
	textRenderer = gtk_cell_renderer_text_new();

	column = gtk_tree_view_column_new();
	
	gtk_tree_view_column_pack_start(column, iconRenderer, FALSE);
	gtk_tree_view_column_pack_start(column, textRenderer, TRUE);
	
	gtk_tree_view_column_add_attribute(column, iconRenderer, "pixbuf", FS_ICON);
	gtk_tree_view_column_add_attribute(column, textRenderer, "markup", FS_LABEL);
	
	gtk_tree_view_column_set_resizable(column, TRUE);
	gtk_tree_view_append_column(GTK_TREE_VIEW(feedview), column);

	/* And connect signals */
	g_signal_connect(G_OBJECT(feedview), "row-activated", G_CALLBACK(ui_feedlist_row_activated_cb), NULL);
	g_signal_connect(G_OBJECT(feedview), "key-press-event", G_CALLBACK(ui_feedlist_key_press_cb), NULL);

	/* Setup the selection handler for the main view */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(feedview));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	
	g_signal_connect(G_OBJECT(select), "changed",
                 	 G_CALLBACK(ui_feedlist_selection_changed_cb),
                	 lookup_widget(mainwindow, "feedlist"));
	
	ui_dnd_init();			
	ui_mainwindow_update_feed_menu(FST_INVALID);

	debug_exit("ui_feedlist_init");
}

void ui_feedlist_select(nodePtr np) {
	GtkTreeIter 		iter;
	GtkWidget		*treeview;
	GtkWidget		*focused;
	GtkTreeSelection	*selection;
	GtkTreePath		*path;
	gint			count;

	treeview = lookup_widget(mainwindow, "feedlist");
	
	/* To work around a GTK+ bug. If the treeview is not
	   focused, setting the selected item will always select the
	   first item! */
	focused = gtk_window_get_focus(GTK_WINDOW(mainwindow));
	gtk_window_set_focus(GTK_WINDOW(mainwindow), treeview);
	
	if(NULL != np) {
		if(filter_feeds_without_unread_headlines) {
			/* check if the node has unread items, if not it is not in 
			   the filtered model and cannot be selected */
			gtk_tree_model_get(GTK_TREE_MODEL(feedstore), ui_node_to_iter(np), FS_UNREAD, &count, -1);
			if(0 == count)
				return;
		}
		
		if(filter_feeds_without_unread_headlines) {
			gtk_tree_model_filter_convert_child_iter_to_iter(GTK_TREE_MODEL_FILTER(filter), &iter, ui_node_to_iter(np));
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(filter), &iter);
		} else {
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), ui_node_to_iter(np));
		}
	
		if(FST_FOLDER != np->type)
			gtk_tree_view_expand_to_path(GTK_TREE_VIEW(treeview), path);
		gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(treeview), path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_view_set_cursor(GTK_TREE_VIEW(treeview), path, NULL, FALSE);
		gtk_tree_path_free(path);

 	} else {
		selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
		gtk_tree_selection_unselect_all(selection);
		/* The code to clear the itemlist when something is
		   unselected is disabled... so we must clear the itemlist
		   explicitly here*/
		itemlist_unload();
	}
	
	gtk_window_set_focus(GTK_WINDOW(mainwindow), focused);
}

/*------------------------------------------------------------------------------*/
/* feedlist filter [de]activation callback					*/
/*------------------------------------------------------------------------------*/

void on_filter_feeds_without_unread_headlines_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget	*feedview;
	
	ui_feedlist_select(NULL); /* This is needed to make the feed menu update itself correctly */
	
	filter_feeds_without_unread_headlines = GTK_CHECK_MENU_ITEM(menuitem)->active;
	feedview = lookup_widget(mainwindow, "feedlist");
	g_assert(feedview != NULL);
	ui_feedlist_set_model(GTK_TREE_VIEW(feedview), feedstore, filter_feeds_without_unread_headlines);
	
	if(filter_feeds_without_unread_headlines) {
		ui_mainwindow_set_status_bar(_("Note: Using the subscriptions filter disables drag & drop"));
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
	}
}

/*------------------------------------------------------------------------------*/
/* delete entry callbacks 							*/
/*------------------------------------------------------------------------------*/

static void ui_feedlist_delete_response_cb(GtkDialog *dialog, gint response_id, gpointer user_data) {
	
	switch(response_id) {
		case GTK_RESPONSE_YES:
			ui_feedlist_remove_node((nodePtr)user_data);
			break;
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

void ui_feedlist_remove_node(nodePtr node) {

	feedlist_remove_node(node);
}

void ui_feedlist_delete_prompt(nodePtr np) {
	GtkWidget	*dialog;
	gchar		*text;
	
	g_assert(np == feedlist_get_selected());

	ui_mainwindow_set_status_bar("%s \"%s\"",_("Deleting entry"), node_get_title(np));
	text = g_strdup_printf((FST_FOLDER == np->type)?_("Are you sure that you want to delete \"%s\" and its contents?"):_("Are you sure that you want to delete \"%s\"?"), node_get_title(np));

	dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
	                                GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
	                                GTK_MESSAGE_QUESTION,
	                                GTK_BUTTONS_YES_NO,
	                                "%s", text);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Deletion Confirmation"));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(mainwindow));

	g_free(text);
	
	gtk_widget_show_all(dialog);

	g_signal_connect(G_OBJECT(dialog), "response",
	                 G_CALLBACK(ui_feedlist_delete_response_cb), np);
}

/*------------------------------------------------------------------------------*/
/* property dialog callbacks 							*/
/*------------------------------------------------------------------------------*/

void on_popup_prop_selected(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	nodePtr		np = (nodePtr)callback_data;
	
	if(NULL != np) {
		if(FST_FEED == np->type) {
			/* loading/unloading the feed because the cache 
			   properties might be changed */
			feedlist_load_node(np);
			ui_feed_propdialog_new(np);
			feedlist_unload_node(np);
			return;
		} 
		if(FST_VFOLDER == np->type) {
			ui_vfolder_propdialog_new(GTK_WINDOW(mainwindow), np);
			return;
		}
	}
	g_message(_("You must select a feed entry."));
	ui_show_error_box(_("You must select a feed entry."));
}

void on_menu_properties(GtkMenuItem *menuitem, gpointer user_data) {
	nodePtr ptr = feedlist_get_selected();
	
	if((ptr != NULL) && (FST_FOLDER == ptr->type)) {
		on_popup_foldername_selected((gpointer)ptr, 0, NULL);
	} else if((ptr != NULL) && (FST_FEED == ptr->type)) {
		on_popup_prop_selected((gpointer)ptr, 0, NULL);
	} else {
		g_warning("You have found a bug in Liferea. You must select a node in the feedlist to do what you just did.");
	}
}

/*------------------------------------------------------------------------------*/
/* new entry dialog callbacks 							*/
/*------------------------------------------------------------------------------*/

void on_newbtn_clicked(GtkButton *button, gpointer user_data) {	

	node_add(FST_FEED);
}

void on_menu_feed_new(GtkMenuItem *menuitem, gpointer user_data) {

	node_add(FST_FEED);
}

void on_new_plugin_activate(GtkMenuItem *menuitem, gpointer user_data) {

	node_add(FST_PLUGIN);
}

void on_popup_newfolder_selected(void) {

	node_add(FST_FOLDER);
}

void on_menu_folder_new(GtkMenuItem *menuitem, gpointer user_data) {

	node_add(FST_FOLDER);
}


