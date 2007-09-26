/**
 * @file ui_update.c GUI update monitor
 * 
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <glib.h>

#include "common.h"
#include "feedlist.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "ui/ui_dialog.h"
#include "ui/ui_node.h"
#include "ui/ui_update.h" 

enum {
	UM_FAVICON,
	UM_REQUEST_TITLE,
	UM_LEN
};

static GtkWidget *umdialog = NULL;
static GtkTreeStore *um1store = NULL;
static GtkTreeStore *um2store = NULL;
static GHashTable *um1hash = NULL;
static GHashTable *um2hash = NULL;

static void ui_update_remove_request(nodePtr node, GtkTreeStore *store, GHashTable *hash) {
	GtkTreeIter	*iter;

	iter = (GtkTreeIter *)g_hash_table_lookup(hash, node->id);
	if(iter) {
		gtk_tree_store_remove(store, iter);
		g_hash_table_remove(hash, (gpointer)node->id);
		g_free(iter);
	}
}

static void ui_update_merge_request(nodePtr node, GtkTreeStore *store, GHashTable *hash) {
	GtkTreeIter	*iter;

	if(NULL != (iter = (GtkTreeIter *)g_hash_table_lookup(hash, (gpointer)node->id)))
		return;

	iter = g_new0(GtkTreeIter, 1);
	gtk_tree_store_append(store, iter, NULL);
	gtk_tree_store_set(store, iter, UM_REQUEST_TITLE, node_get_title(node), 
	                                UM_FAVICON, node_get_icon(node),
	                                -1);
	g_hash_table_insert(hash, (gpointer)node->id, (gpointer)iter);
}

static void
ui_update_find_requests (nodePtr node) {

	if (node->children)
		node_foreach_child (node, ui_update_find_requests);

	if (!node->subscription)
		return;
		
	if (node->subscription->updateJob) {
		if (REQUEST_STATE_PROCESSING == update_job_get_state (node->subscription->updateJob)) {
			ui_update_merge_request (node, um1store, um1hash);
			ui_update_remove_request (node, um2store, um2hash);
			return;
		}

		if (REQUEST_STATE_PENDING == update_job_get_state (node->subscription->updateJob)) {
			ui_update_merge_request (node, um2store, um2hash);
			return;
		}
	}
			
	ui_update_remove_request (node, um1store, um1hash);
	ui_update_remove_request (node, um2store, um2hash);
}

static gboolean ui_update_monitor_update(void *data) {

	if(umdialog) {
		feedlist_foreach(ui_update_find_requests);
		return TRUE;
	} else {
		return FALSE;
	}	
}

static void
ui_update_cancel (nodePtr node)
{
	if (node->children)
		node_foreach_child (node, ui_update_cancel);

	if (!node->subscription)
		return;
		
	if (!node->subscription->updateJob)
		return;
		
	subscription_cancel_update (node->subscription);
}

void on_cancel_all_requests_clicked(GtkButton *button, gpointer user_data) {

	feedlist_foreach(ui_update_cancel);
}

void on_close_update_monitor_clicked(GtkButton *button, gpointer user_data) {

	gtk_widget_destroy(umdialog);
	umdialog = NULL;
	g_hash_table_destroy(um1hash);
	g_hash_table_destroy(um2hash);
}
 
void on_menu_show_update_monitor(GtkWidget *widget, gpointer user_data) {
	GtkCellRenderer		*textRenderer, *iconRenderer;	
	GtkTreeViewColumn 	*column;
	GtkTreeView		*view;

	if(!umdialog) {
		umdialog = liferea_dialog_new (NULL, "updatedialog");
				
		/* Set up left store and view */
		view = GTK_TREE_VIEW(liferea_dialog_lookup(umdialog, "left"));
		um1store = gtk_tree_store_new(UM_LEN, GDK_TYPE_PIXBUF, G_TYPE_STRING);
		gtk_tree_view_set_model(view, GTK_TREE_MODEL(um1store));

		textRenderer = gtk_cell_renderer_text_new();
		iconRenderer = gtk_cell_renderer_pixbuf_new();
		column = gtk_tree_view_column_new();
	
		gtk_tree_view_column_pack_start(column, iconRenderer, FALSE);
		gtk_tree_view_column_pack_start(column, textRenderer, TRUE);
		gtk_tree_view_column_add_attribute(column, iconRenderer, "pixbuf", UM_FAVICON);
		gtk_tree_view_column_add_attribute(column, textRenderer, "markup", UM_REQUEST_TITLE);
		gtk_tree_view_append_column(view, column);
		
		/* Set up right store and view */
		view = GTK_TREE_VIEW(liferea_dialog_lookup(umdialog, "right"));
		um2store = gtk_tree_store_new(UM_LEN, GDK_TYPE_PIXBUF, G_TYPE_STRING);
		gtk_tree_view_set_model(view, GTK_TREE_MODEL(um2store));

		textRenderer = gtk_cell_renderer_text_new();
		iconRenderer = gtk_cell_renderer_pixbuf_new();
		column = gtk_tree_view_column_new();
	
		gtk_tree_view_column_pack_start(column, iconRenderer, FALSE);
		gtk_tree_view_column_pack_start(column, textRenderer, TRUE);
		gtk_tree_view_column_add_attribute(column, iconRenderer, "pixbuf", UM_FAVICON);
		gtk_tree_view_column_add_attribute(column, textRenderer, "markup", UM_REQUEST_TITLE);
		gtk_tree_view_append_column(view, column);		
		
		/* Fill in data */
		um1hash = g_hash_table_new(g_str_hash, g_str_equal);
		um2hash = g_hash_table_new(g_str_hash, g_str_equal);
	 	(void)g_timeout_add(1000, ui_update_monitor_update, NULL);
	}
	
	gtk_window_present(GTK_WINDOW(umdialog));
}
