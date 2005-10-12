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

GtkTreeModel		*filter;
GtkTreeStore		*feedstore = NULL;

nodePtr			displayed_node = NULL;

/* flag to enable/disable the GtkTreeModel filter */
gboolean filter_feeds_without_unread_headlines = FALSE;


/* signal handlers to update the tree view paths in the feed structures */
static void ui_feedlist_row_changed_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter) {
	nodePtr np;
	
	gtk_tree_model_get(model, iter, FS_PTR, &np, -1);
	if(NULL != np) {
		if(NULL == np->ui_data)
			np->ui_data = (void *)g_new0(struct ui_data, 1);
		((struct ui_data *)np->ui_data)->row = *iter;
	}
}

static void ui_feedlist_rows_reordered_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter, gpointer list, gpointer user_data) {

	g_print("rows reordered\n");
}

nodePtr ui_feedlist_get_parent(nodePtr ptr) {
	GtkTreeIter	*iter = &((ui_data*)(ptr->ui_data))->row;
	GtkTreeIter	parent;
	nodePtr	parentPtr;
	
	if(gtk_tree_model_iter_parent(GTK_TREE_MODEL(feedstore), &parent, iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &parent,
		                                              FS_PTR, &parentPtr,
		                                              -1);
		return parentPtr;
	}
	
	return NULL;
}

nodePtr ui_feedlist_get_selected(void) {
	GtkTreeSelection	*select;
	GtkTreeModel		*model;
	GtkTreeIter		iter;
	nodePtr			ptr = NULL;

	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(lookup_widget(mainwindow, "feedlist")));
	if(gtk_tree_selection_get_selected(select, &model, &iter))
		gtk_tree_model_get(model, &iter, FS_PTR, &ptr, -1);
	return ptr;
}

nodePtr ui_feedlist_get_target_folder(int *pos) {
	nodePtr		ptr;
	GtkTreeIter	iter;
	GtkTreePath 	*path;
	gint		*indices;
	
	if(NULL == (ptr = ui_feedlist_get_selected())) {
		*pos = -1;
		return NULL;
	}

	if(filter_feeds_without_unread_headlines) {
		gtk_tree_model_filter_convert_child_iter_to_iter(GTK_TREE_MODEL_FILTER(filter), &iter, &((ui_data*)(ptr->ui_data))->row);
	} else {
		iter = ((ui_data*)(ptr->ui_data))->row;
	}

	if(FST_FOLDER == ptr->type) {
		*pos = -1;
		return ptr;
	} else {
		path = gtk_tree_model_get_path(gtk_tree_view_get_model(GTK_TREE_VIEW(lookup_widget(mainwindow, "feedlist"))), &iter);
		indices = gtk_tree_path_get_indices(path);
		*pos = indices[gtk_tree_path_get_depth(path)-1] + 1;
		gtk_tree_path_free(path);
		return ui_feedlist_get_parent(ptr);
	}
}

static void ui_feedlist_node_update(nodePtr np) {
	GtkTreeIter	iter;
	gchar		*label, *tmp;
	
	if(np->ui_data == NULL)
		return;
	
	iter = ((ui_data*)np->ui_data)->row;
	
	label = unhtmlize(g_strdup(node_get_title(np)));
	/* FIXME: Unescape text here! */
	tmp = g_markup_escape_text(label,-1);
	g_free(label);
	if(np->unreadCount > 0)
		label = g_strdup_printf("<span weight=\"bold\">%s (%d)</span>", tmp, np->unreadCount);
	else
		label = g_strdup_printf("%s", tmp);
	g_free(tmp);
	
	gtk_tree_store_set(feedstore, &iter, FS_LABEL, label,
	                                    FS_UNREAD, np->unreadCount,
	                                    FS_ICON, np->icon,
	                                    -1);
	g_free(label);
}

static void ui_feedlist_update_(GtkTreeIter *iter) {
	GtkTreeIter	childiter;
	gboolean	valid;
	nodePtr		ptr = NULL;
	
	if(iter != NULL) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), iter, FS_PTR, &ptr, -1);		
		valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &childiter, iter);
	} else {
		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(feedstore), &childiter);
	}

/*	if(ptr != NULL)
		((ui_data*)(ptr->ui_data))->row = *iter;*/

	while(valid) {
		ui_feedlist_update_(&childiter);
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &childiter);
	}

	if(ptr != NULL)
		ui_node_update(ptr);
}

void ui_feedlist_update_iter(GtkTreeIter *iter) {

	ui_feedlist_update_(iter);

	if(filter_feeds_without_unread_headlines)
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(filter));
		
	ui_redraw_widget("feedlist");
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

		/* make sure thats no grouping iterator */
		if((FST_FEED == type) || (FST_VFOLDER == type)) {
			
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
		itemlist_load(np);
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
		nodePtr np = ui_feedlist_get_selected();
		
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
	g_signal_connect(G_OBJECT(feedstore), "row-changed", G_CALLBACK(ui_feedlist_row_changed_cb), NULL);
	g_signal_connect(G_OBJECT(feedstore), "rows-reordered", G_CALLBACK(ui_feedlist_rows_reordered_cb), NULL);
}

/* sets up the entry list store and connects it to the entry list
   view in the main window */
void ui_feedlist_init(GtkWidget *feedview) {
	GtkCellRenderer		*textRenderer;
	GtkCellRenderer		*iconRenderer;	
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;	
	
	g_assert(mainwindow != NULL);
	g_assert(feedview != NULL);
	
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
}

void ui_feedlist_select(nodePtr np) {
	GtkTreeIter 		iter;
	GtkWidget		*treeview;
	GtkWidget		*focused;
	GtkTreeSelection	*selection;
	GtkTreePath		*path;
	gint			count;

	/* some comfort: select the created iter */
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
			gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &((ui_data*)(np->ui_data))->row, FS_UNREAD, &count, -1);
			if(0 == count)
				return;
		}
		
		if(filter_feeds_without_unread_headlines) {
			gtk_tree_model_filter_convert_child_iter_to_iter(GTK_TREE_MODEL_FILTER(filter), &iter, &((ui_data*)(np->ui_data))->row);
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(filter), &iter);
		} else {
			path = gtk_tree_model_get_path(GTK_TREE_MODEL(feedstore), &((ui_data*)(np->ui_data))->row);
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
		itemlist_load(NULL);
	}
	
	gtk_window_set_focus(GTK_WINDOW(mainwindow), focused);
}

/*------------------------------------------------------------------------------*/
/* next unread callback								*/
/*------------------------------------------------------------------------------*/

enum scanStateType {
  UNREAD_SCAN_INIT,            /* selected not yet passed */
  UNREAD_SCAN_FOUND_SELECTED,  /* selected passed */
  UNREAD_SCAN_SECOND_PASS      /* no unread items after selected feed */
};

static enum scanStateType scanState = UNREAD_SCAN_INIT;

/* This method tries to find a feed with unread items 
   in two passes. In the first pass it tries to find one
   after the currently selected feed (including the
   selected feed). If there are no such feeds the 
   search is restarted for all feeds. */
static nodePtr ui_feedlist_unread_scan(nodePtr folder) {
	nodePtr			ptr, childNode, selectedNode;
	GtkTreeModel		*model;
	GtkTreeIter		iter, iter2, *selectedIter, *parent = NULL;
	gboolean		valid = FALSE;
	gint			count;

	model = gtk_tree_view_get_model(GTK_TREE_VIEW(lookup_widget(mainwindow, "feedlist")));
	if(NULL != (selectedNode = ui_feedlist_get_selected())) {
		selectedIter = &((ui_data *)(selectedNode->ui_data))->row;
	} else {
		scanState = UNREAD_SCAN_SECOND_PASS;
	}

	if(folder != NULL) {
		/* determine folder tree iter */
		if(filter_feeds_without_unread_headlines) {
			gtk_tree_model_filter_convert_child_iter_to_iter(GTK_TREE_MODEL_FILTER(filter), &iter2, &((ui_data*)(folder->ui_data))->row);
		} else {
			iter2 = ((ui_data*)(folder->ui_data))->row;
		}
		parent = &iter2;
	} else {
		if(0 == gtk_tree_model_iter_n_children(model, NULL))
			return NULL;	/* avoid problems in filtered mode */
	}
	
	valid = gtk_tree_model_iter_children(model, &iter, parent);
	while(valid) {
		gtk_tree_model_get(model, &iter,
		                   FS_PTR, &ptr,
		                   FS_UNREAD, &count,
		                   -1);

		if(ptr == selectedNode)
		  scanState = UNREAD_SCAN_FOUND_SELECTED;

		/* feed match if beyond the selected feed or in second pass... */
		if((scanState != UNREAD_SCAN_INIT) && (count > 0) &&
		   (FST_FEED == ptr->type)) {
		       return ptr;
		}

		/* folder traversal if we are searching the selected feed
		   which might be a descendant of the folder and if we
		   are beyond the selected feed and the folder contains
		   feeds with unread items... */
		if((FST_FOLDER == ptr->type) &&
		   (((scanState != UNREAD_SCAN_INIT) && (count > 0)) ||
		    gtk_tree_store_is_ancestor(GTK_TREE_STORE(model), &iter, selectedIter))) {
		       if(NULL != (childNode = ui_feedlist_unread_scan(ptr)))
			 return childNode;
		} /* Directories are never checked */

		valid = gtk_tree_model_iter_next(model, &iter);
	}

	if(NULL == folder) { /* we are on feed list root but didn't find anything */
		if(0 == feedlist_get_unread_item_count()) {
			/* this may mean there is nothing more to find */
		} else {
			/* or that we just didn't find anything after the selected feed */
			g_assert(scanState != UNREAD_SCAN_SECOND_PASS);
			scanState = UNREAD_SCAN_SECOND_PASS;
			childNode = ui_feedlist_unread_scan(NULL);
			return childNode;
		}
	}

	return NULL;
}

nodePtr ui_feedlist_find_unread_feed(nodePtr folder) {

	scanState = UNREAD_SCAN_INIT;
	return ui_feedlist_unread_scan(folder);
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
	ui_feedlist_select(NULL);
	itemlist_load(NULL);
	feedlist_remove_node(node);
}

void ui_feedlist_delete_prompt(nodePtr np) {
	GtkWidget	*dialog;
	gchar		*text;
	
	g_assert(np->ui_data != NULL);
	g_assert(np == ui_feedlist_get_selected());

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
			ui_feed_propdialog_new(GTK_WINDOW(mainwindow),(feedPtr)np->data);
			return;
		} 
		if(FST_VFOLDER == np->type) {
			ui_vfolder_propdialog_new(GTK_WINDOW(mainwindow),np->data);
			return;
		}
	}
	g_message(_("You must select a feed entry."));
	ui_show_error_box(_("You must select a feed entry."));
}

void on_menu_properties(GtkMenuItem *menuitem, gpointer user_data) {
	nodePtr ptr = ui_feedlist_get_selected();
	
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

void ui_feedlist_add(nodePtr parent, nodePtr node, gint position) {
	GtkTreeIter	*iter, *parentIter = NULL;

	g_assert(node->ui_data == NULL);

	/* if parent is NULL we have the root folder and don't create a new row! */
	node->ui_data = (gpointer)g_new0(struct ui_data, 1);
	iter = &(((ui_data*)(node->ui_data))->row);
	
	if(parent != NULL) {
		g_assert(parent->ui_data != NULL);
		parentIter = &(((ui_data*)(parent->ui_data))->row);
	}
	
	if(position < 0)
		gtk_tree_store_append(feedstore, iter, parentIter);
	else
		gtk_tree_store_insert(feedstore, iter, parentIter, position);

	gtk_tree_store_set(feedstore, iter, FS_PTR, node, -1);
	
	ui_node_check_if_folder_is_empty();
	ui_feedlist_update();
}

/*void ui_feedlist_new_subscription(const gchar *source, const gchar *filter, gint flags) {
	
	debug_enter("ui_feedlist_new_subscription");	
	
	feedlist_add_feed(parent, fp, pos);
	ui_feedlist_update();
	ui_feedlist_select((nodePtr)fp);
	
	feed_schedule_update(fp, flags | FEED_REQ_PRIORITY_HIGH | FEED_REQ_DOWNLOAD_FAVICON | FEED_REQ_AUTH_DIALOG);
	
	debug_exit("ui_feedlist_new_subscription");
}*/

void on_newbtn_clicked(GtkButton *button, gpointer user_data) {	
	nodePtr	parent;
	int	pos;

	parent = ui_feedlist_get_parent(&pos);
	node_add(parent, FST_FEED);
}

void on_menu_feed_new(GtkMenuItem *menuitem, gpointer user_data) {

	on_newbtn_clicked(NULL, NULL);
}

/* recursivly calls func for every feed in the feed list */
void ui_feedlist_do_for_all_full(nodePtr ptr, gint filter, gpointer func, gint params, gpointer user_data) {
	GtkTreeIter	childiter;
	gboolean	valid, apply, descend;
	nodePtr		child;
	
	if(NULL == ptr) {
		valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(feedstore), &childiter);
	} else {
		if(NULL == ptr->ui_data)
			return;	/* folder is hidden -> nothing to do */
		valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &childiter, &((ui_data*)ptr->ui_data)->row);
	}
	
	while(valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &childiter, FS_PTR, &child, -1);
		/* Must update counter here because the current node may be deleted! */
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &childiter);
		/* If child == NULL, this is an empty node. */
		if (child != NULL) {
			gboolean directory = (FST_FEED == child->type) && (((feedPtr)child)->fhp != NULL) && ((feedPtr)child)->fhp->directory;
			apply = (filter & ACTION_FILTER_CHILDREN) ||
				((filter & ACTION_FILTER_FEED) && (FST_FEED == child->type) && !directory) ||
				((filter & ACTION_FILTER_FEED) && (FST_VFOLDER == child->type) && !directory) ||
				((filter & ACTION_FILTER_DIRECTORY) && (FST_FEED == child->type) && directory) ||
				((filter & ACTION_FILTER_FOLDER) && (FST_FOLDER == child->type));
			descend = !(filter & ACTION_FILTER_CHILDREN);
			
			if(TRUE == apply) {
				if (params==0)
					((nodeActionFunc)func)(child);
				else 
					((nodeActionDataFunc)func)(child, user_data);
			}
			
			/* if the iter has children and we are descending, iterate over the children. */
			if(descend)
				ui_feedlist_do_for_all_data(child, filter, func, user_data);
		}
	}
}
