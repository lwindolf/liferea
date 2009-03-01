/**
 * @file ui_feedlist.c GUI feed list handling
 *
 * Copyright (C) 2004-2009 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2005 Raphael Slinckx <raphael@slinckx.net>
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

#include "ui/ui_feedlist.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "common.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "net.h"
#include "newsbin.h"
#include "vfolder.h"
#include "ui/browser_tabs.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"
#include "ui/ui_dnd.h"
#include "ui/ui_node.h"
#include "ui/ui_subscription.h"
#include "fl_sources/node_source.h"

extern GHashTable	*feedHandler;

GtkTreeModel		*filter;
GtkTreeStore		*feedstore = NULL;

gboolean			feedlist_reduced_unread = FALSE;

static void
ui_feedlist_row_changed_cb(GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter)
{
	nodePtr node;
	
	gtk_tree_model_get (model, iter, FS_PTR, &node, -1);
	if (node)
		ui_node_update_iter(node->id, iter);
}

static void
ui_feedlist_selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter		iter;
	GtkTreeModel		*model;
	nodePtr			node;
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
		if (realNode)
			browser_tabs_show_headlines ();
		
		/* update feed list and item list states */
		feedlist_selection_changed (node);
		
		if (node) {
			liferea_shell_update_update_menu ((NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE) ||
			                                  (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE_CHILDS));
			liferea_shell_update_feed_menu (TRUE, (NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST));
		} else {
			liferea_shell_update_feed_menu (FALSE, FALSE);
		}
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
	if (node && IS_FOLDER (node)) {
		if (gtk_tree_view_row_expanded (tv, path))
			gtk_tree_view_collapse_row (tv, path);
		else
			gtk_tree_view_expand_row (tv, path, FALSE);
	}

}

static gboolean
ui_feedlist_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if ((event->type == GDK_KEY_PRESS) &&
	    (event->state == 0) &&
	    (event->keyval == GDK_Delete)) {
		nodePtr node = feedlist_get_selected ();
				
		if(node) {
			if (event->state & GDK_SHIFT_MASK)
				feedlist_remove_node (node);
			else
				ui_feedlist_delete_prompt (node);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean ui_feedlist_filter_visible_function(GtkTreeModel *model, GtkTreeIter *iter, gpointer data) {
	gint count;
	nodePtr np;

	if(!feedlist_reduced_unread)
		return TRUE;

	gtk_tree_model_get(model, iter, FS_PTR, &np, FS_UNREAD, &count, -1);

	if (np && (IS_FOLDER (np) || IS_NODE_SOURCE (np))) 
		return FALSE;

	if (0 != count)
		return TRUE;
	else
		return FALSE;
}

static void
ui_feedlist_reduce_unread (gboolean reduced) {
	GtkTreeView		*treeview;

	treeview = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));

	feedlist_reduced_unread = reduced;

	if (reduced) {
		gtk_tree_view_set_reorderable (GTK_TREE_VIEW(treeview), FALSE);
		gtk_tree_view_set_model (GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(filter));
		gtk_tree_model_filter_refilter( GTK_TREE_MODEL_FILTER (filter));
	}
	else {
		gtk_tree_view_set_reorderable (GTK_TREE_VIEW(treeview), TRUE);
		gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER (filter));
		gtk_tree_view_set_model (GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(feedstore));
	}
	
	ui_node_reload_feedlist ();
}

/* sets up the entry list store and connects it to the entry list
   view in the main window */
void
ui_feedlist_init (GtkTreeView *treeview)
{
	GtkCellRenderer		*textRenderer;
	GtkCellRenderer		*iconRenderer;	
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;	
	
	debug_enter ("ui_feedlist_init");

	/* Set up store */
	feedstore = gtk_tree_store_new (FS_LEN,
	                                G_TYPE_STRING,
	                                GDK_TYPE_PIXBUF,
	                                G_TYPE_POINTER,
	                                G_TYPE_UINT);

	gtk_tree_view_set_model (GTK_TREE_VIEW (treeview), GTK_TREE_MODEL (feedstore));

	/* Prepare filter */
	filter = gtk_tree_model_filter_new (GTK_TREE_MODEL(feedstore), NULL);
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER(filter),
	                                        ui_feedlist_filter_visible_function,
	                                        NULL,
	                                        NULL);

	g_signal_connect (G_OBJECT (feedstore), "row-changed", G_CALLBACK (ui_feedlist_row_changed_cb), NULL);

	/* we only render the state and title */
	iconRenderer = gtk_cell_renderer_pixbuf_new ();
	textRenderer = gtk_cell_renderer_text_new ();

	column = gtk_tree_view_column_new ();
	
	gtk_tree_view_column_pack_start (column, iconRenderer, FALSE);
	gtk_tree_view_column_pack_start (column, textRenderer, TRUE);
	
	gtk_tree_view_column_add_attribute (column, iconRenderer, "pixbuf", FS_ICON);
	gtk_tree_view_column_add_attribute (column, textRenderer, "markup", FS_LABEL);
	
	gtk_tree_view_column_set_resizable (column, TRUE);
	gtk_tree_view_append_column (treeview, column);
	
	g_object_set (textRenderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	g_signal_connect (G_OBJECT (treeview), "row-activated", G_CALLBACK (ui_feedlist_row_activated_cb), NULL);
	g_signal_connect (G_OBJECT (treeview), "key-press-event", G_CALLBACK (ui_feedlist_key_press_cb), NULL);

	select = gtk_tree_view_get_selection (treeview);
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	
	g_signal_connect (G_OBJECT (select), "changed",
	                  G_CALLBACK (ui_feedlist_selection_changed_cb),
                	  liferea_shell_lookup ("feedlist"));
	
	ui_dnd_setup_feedlist (feedstore);
	liferea_shell_update_feed_menu (FALSE, FALSE);
	liferea_shell_update_allitems_actions (FALSE, FALSE);
	debug_exit ("ui_feedlist_init");
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
	GtkTreeView		*treeview;
	GtkTreeModel		*model;
	GtkWindow		*mainwindow;
	
	mainwindow = GTK_WINDOW (liferea_shell_get_window ());
	treeview = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
	model = gtk_tree_view_get_model (treeview);
	
	if (node && node != feedlist_get_root ()) {
		GtkTreePath *path = gtk_tree_model_get_path (model, ui_node_to_iter(node->id));
		
		if (node->parent)
			ui_feedlist_expand_parents (node->parent);

		gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
		gtk_tree_path_free (path);

 	} else {
		GtkTreeSelection *selection = gtk_tree_view_get_selection (treeview);
		gtk_tree_selection_unselect_all (selection);
	}
}

/* delete feed callbacks */

static void
ui_feedlist_delete_response_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{	
	switch (response_id) {
		case GTK_RESPONSE_ACCEPT:
			feedlist_remove_node ((nodePtr)user_data);
			break;
	}
	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
ui_feedlist_delete_prompt (nodePtr node)
{
	GtkWidget	*dialog;
	GtkWindow	*mainwindow;
	gchar		*text;
	
	g_assert (node == feedlist_get_selected ());

	liferea_shell_set_status_bar ("%s \"%s\"", _("Deleting entry"), node_get_title (node));
	text = g_strdup_printf (IS_FOLDER (node)?_("Are you sure that you want to delete \"%s\" and its contents?"):_("Are you sure that you want to delete \"%s\"?"), node_get_title (node));

	mainwindow = GTK_WINDOW (liferea_shell_get_window ());
	dialog = gtk_message_dialog_new (mainwindow,
	                                 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
	                                 GTK_MESSAGE_QUESTION,
	                                 GTK_BUTTONS_NONE,
	                                 "%s", text);
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
	                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                        GTK_STOCK_DELETE, GTK_RESPONSE_ACCEPT,
	                        NULL);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Deletion Confirmation"));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), mainwindow);

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

void on_menu_delete(GtkWidget *widget, gpointer user_data)
{
	ui_feedlist_delete_prompt (feedlist_get_selected());
}

void
on_menu_update (GtkWidget *widget, gpointer user_data)
{
	if (!feedlist_get_selected ()) {
		ui_show_error_box (_("You have to select a feed entry"));
		return;
	}

	if (network_is_online ()) 
		node_update_subscription (feedlist_get_selected (), GUINT_TO_POINTER (FEED_REQ_PRIORITY_HIGH));
	else
		liferea_shell_set_status_bar (_("Liferea is in offline mode. No update possible."));
}

void
on_menu_update_all(GtkWidget *widget, gpointer user_data)
{ 
	if (network_is_online ()) 
		node_update_subscription (feedlist_get_root(), GUINT_TO_POINTER (FEED_REQ_PRIORITY_HIGH));
	else
		liferea_shell_set_status_bar (_("Liferea is in offline mode. No update possible."));
}

void
on_menu_allread (GtkWidget *widget, gpointer user_data)
{	
	feedlist_mark_all_read (feedlist_get_selected ());
}

void
on_menu_allfeedsread (GtkWidget *widget, gpointer user_data)
{
	feedlist_mark_all_read (feedlist_get_root ());
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

void
on_feedlist_reduced_activate (GtkToggleAction *menuitem, gpointer user_data)
{
	ui_feedlist_reduce_unread (gtk_toggle_action_get_active (menuitem));
}
