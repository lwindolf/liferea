/**
 * @file ui_feedlist.c GUI feed list handling
 *
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "favicon.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "newsbin.h"
#include "update.h"
#include "vfolder.h"
#include "ui/ui_dnd.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"
#include "ui/ui_shell.h"
#include "ui/ui_subscription.h"
#include "ui/ui_tabs.h"
#include "ui/ui_vfolder.h"
#include "fl_sources/node_source.h"

extern GHashTable	*feedHandler;

GtkTreeModel		*filter;
GtkTreeStore		*feedstore = NULL;

static void ui_feedlist_row_changed_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter) {
	nodePtr node;
	
	gtk_tree_model_get(model, iter, FS_PTR, &node, -1);
	if(node)
		ui_node_update_iter(node->id, iter);
}

nodePtr
ui_feedlist_get_target_folder (int *pos) {
	nodePtr		node;
	GtkTreeIter	*iter = NULL;
	GtkTreePath 	*path;
	gint		*indices;

	g_assert (NULL != pos);
	
	*pos = -1;
	node = feedlist_get_selected ();
	if (!node)
		return feedlist_get_root ();

	if (IS_FOLDER (node)) {
		return node;
	} else {
		iter = ui_node_to_iter (node->id);
		path = gtk_tree_model_get_path (gtk_tree_view_get_model (GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"))), iter);
		indices = gtk_tree_path_get_indices (path);
		if (pos)
			*pos = indices[gtk_tree_path_get_depth (path)-1] + 1;
		gtk_tree_path_free (path);
		return node->parent;
	}
}

static void
ui_feedlist_selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter		iter;
	GtkTreeModel		*model;
	nodePtr			node;
	GdkGeometry		geometry;
	gboolean		realNode = TRUE;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
	 	gtk_tree_model_get (model, &iter, FS_PTR, &node, -1);

		debug1 (DEBUG_GUI, "feed list selection changed to \"%s\"", node_get_title(node));

		/* make sure thats no grouping iterator */
		// FIXME: check based on node capabilities!!!
		realNode = (node && (IS_FEED (node) || 
		                     IS_NEWSBIN (node) || 
				     IS_VFOLDER (node) || 
				     IS_NODE_SOURCE (node)));
		if(realNode) {			
			/* FIXME: another workaround to prevent strange window
			   size increasings after feed selection changing 
			   
			   Why is this workaround necessary? Missing documentation!!!
			   
			   Starting with 1.4.4 changing the minimum size from
			   640x480 to 50x50. If anyone experiences it there will
			   be bug reports soon. If there are none over about a month
			   this should be removed! */
			geometry.min_height = 50;
			geometry.min_width = 50;
			g_assert (mainwindow != NULL);
			gtk_window_set_geometry_hints (GTK_WINDOW (mainwindow), mainwindow, &geometry, GDK_HINT_MIN_SIZE);
		
			ui_tabs_show_headlines ();
			
			/* workaround to ensure the feedlist is focussed when we click it
			   (Mozilla might prevent this, ui_itemlist_display() depends on this */
			gtk_widget_grab_focus (liferea_shell_lookup ("feedlist"));
		}
		
		/* update feed list and item list states */
		feedlist_selection_changed (node);
		
		if (node)
			ui_mainwindow_update_feed_menu (TRUE, (NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST));
		else
			ui_mainwindow_update_feed_menu (FALSE, FALSE);
	} else {
		/* If we cannot get the new selection we keep the old one
		   this happens when we're doing drag&drop for example. */
	}
}

static void
ui_feedlist_row_activated_cb (GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
{
	GtkTreeIter	iter;
	nodePtr		node;
	
	gtk_tree_model_get_iter (gtk_tree_view_get_model (tv), &iter, path);
	gtk_tree_model_get (gtk_tree_view_get_model (tv), &iter, FS_PTR, &node, -1);
	if(node && IS_FOLDER (node)) {
		if (gtk_tree_view_row_expanded (tv, path))
			gtk_tree_view_collapse_row (tv, path);
		else
			gtk_tree_view_expand_row (tv, path, FALSE);
	}

}

static gboolean ui_feedlist_key_press_cb(GtkWidget *widget, GdkEventKey *event, gpointer data) {

	if((event->type == GDK_KEY_PRESS) &&
	   (event->state == 0) &&
	   (event->keyval == GDK_Delete)) {
		nodePtr np = feedlist_get_selected();
		
		if(NULL != np) {
			if(event->state & GDK_SHIFT_MASK)
				feedlist_remove_node(np);
			else
				ui_feedlist_delete_prompt(np);
			return TRUE;
		}
	}
	return FALSE;
}

static void ui_feedlist_set_model(GtkTreeView *feedview, GtkTreeStore *feedstore) {
	GtkTreeModel	*model;
		
	model = GTK_TREE_MODEL(feedstore);
	gtk_tree_view_set_model(GTK_TREE_VIEW(feedview), model);
	g_signal_connect(G_OBJECT(feedstore), "row-changed", G_CALLBACK(ui_feedlist_row_changed_cb), NULL);
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

	/* Set up store */
	feedstore = gtk_tree_store_new(FS_LEN,
	                               G_TYPE_STRING,
	                               GDK_TYPE_PIXBUF,
	                               G_TYPE_POINTER,
	                               G_TYPE_INT);

	ui_feedlist_set_model(GTK_TREE_VIEW(feedview), feedstore);

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
	
#if GTK_CHECK_VERSION(2,6,0)
	g_object_set(textRenderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
#endif

	/* And connect signals */
	g_signal_connect(G_OBJECT(feedview), "row-activated", G_CALLBACK(ui_feedlist_row_activated_cb), NULL);
	g_signal_connect(G_OBJECT(feedview), "key-press-event", G_CALLBACK(ui_feedlist_key_press_cb), NULL);

	/* Setup the selection handler for the main view */
	select = gtk_tree_view_get_selection(GTK_TREE_VIEW(feedview));
	gtk_tree_selection_set_mode(select, GTK_SELECTION_SINGLE);
	
	g_signal_connect (G_OBJECT (select), "changed",
	                  G_CALLBACK (ui_feedlist_selection_changed_cb),
                	  liferea_shell_lookup ("feedlist"));
	
	ui_dnd_setup_feedlist(feedstore);			
	ui_mainwindow_update_feed_menu(FALSE, FALSE);

	debug_exit("ui_feedlist_init");
}

static void
ui_feedlist_expand_parents (nodePtr parentNode)
{
	if (parentNode->parent)
		ui_feedlist_expand_parents (parentNode->parent);
		
	ui_node_set_expansion (parentNode, TRUE);
}

void
ui_feedlist_select (nodePtr node)
{
	GtkWidget		*treeview;
	GtkWidget		*focused;

	treeview = liferea_shell_lookup ("feedlist");
	
	/* To work around a GTK+ bug. If the treeview is not
	   focused, setting the selected item will always select the
	   first item! */
	focused = gtk_window_get_focus (GTK_WINDOW (mainwindow));
	gtk_window_set_focus (GTK_WINDOW (mainwindow), treeview);
	
	if (node && node != feedlist_get_root ()) {
		GtkTreePath *path = gtk_tree_model_get_path (GTK_TREE_MODEL (feedstore), ui_node_to_iter(node->id));
		
		if (node->parent)
			ui_feedlist_expand_parents (node->parent);

		gtk_tree_view_scroll_to_cell (GTK_TREE_VIEW (treeview), path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_view_set_cursor (GTK_TREE_VIEW (treeview), path, NULL, FALSE);
		gtk_tree_path_free (path);

 	} else {
		GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
		gtk_tree_selection_unselect_all (selection);
	}
	
	gtk_window_set_focus (GTK_WINDOW (mainwindow), focused);
}

/*------------------------------------------------------------------------------*/
/* delete entry callbacks 							*/
/*------------------------------------------------------------------------------*/

static void ui_feedlist_delete_response_cb(GtkDialog *dialog, gint response_id, gpointer user_data) {
	
	switch(response_id) {
		case GTK_RESPONSE_YES:
			feedlist_remove_node((nodePtr)user_data);
			break;
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

void
ui_feedlist_delete_prompt (nodePtr node)
{
	GtkWidget	*dialog;
	gchar		*text;
	
	g_assert (node == feedlist_get_selected ());

	ui_mainwindow_set_status_bar ("%s \"%s\"", _("Deleting entry"), node_get_title (node));
	text = g_strdup_printf (IS_FOLDER (node)?_("Are you sure that you want to delete \"%s\" and its contents?"):_("Are you sure that you want to delete \"%s\"?"), node_get_title (node));

	dialog = gtk_message_dialog_new (GTK_WINDOW (mainwindow),
	                                 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
	                                 GTK_MESSAGE_QUESTION,
	                                 GTK_BUTTONS_YES_NO,
	                                 "%s", text);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Deletion Confirmation"));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (mainwindow));

	g_free (text);
	
	gtk_widget_show_all (dialog);

	g_signal_connect (G_OBJECT (dialog), "response",
	                  G_CALLBACK (ui_feedlist_delete_response_cb), node);
}

void
on_menu_properties (GtkMenuItem *menuitem, gpointer user_data)
{
	nodePtr node = feedlist_get_selected ();
	
	NODE_TYPE (node)->request_properties (node);
}

void
on_newbtn_clicked (GtkButton *button, gpointer user_data)
{	
	node_type_request_interactive_add (feed_get_node_type ());
}

void
on_menu_feed_new (GtkMenuItem *menuitem, gpointer user_data)
{
	node_type_request_interactive_add (feed_get_node_type ());
}

void
on_new_plugin_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	node_type_request_interactive_add (node_source_get_node_type ());
}

void
on_new_newsbin_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	node_type_request_interactive_add (newsbin_get_node_type ());
}

void
on_menu_folder_new (GtkMenuItem *menuitem, gpointer user_data)
{
	node_type_request_interactive_add (folder_get_node_type ());
}
