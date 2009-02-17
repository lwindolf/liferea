/**
 * @file ui_node.c GUI folder handling
 * 
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2008 Lars Lindner <lars.lindner@gmail.com>
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

#include "ui/ui_node.h"

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "fl_sources/node_source.h"
#include "folder.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_popup.h"

static GHashTable	*flIterHash = NULL;	/* hash table used for fast node id <-> tree iter lookup */
static GtkWidget	*nodenamedialog = NULL;

GtkTreeIter *
ui_node_to_iter (const gchar *nodeId)
{
	if (!flIterHash)
		return NULL;

	return (GtkTreeIter *)g_hash_table_lookup (flIterHash, (gpointer)nodeId);
}

void
ui_node_update_iter (const gchar *nodeId, GtkTreeIter *iter)
{
	GtkTreeIter *old;

	if (!flIterHash)
		return;

	old = (GtkTreeIter *)g_hash_table_lookup (flIterHash, (gpointer)nodeId);
	if (old)
		*old = *iter;
}

static void
ui_node_add_iter (const gchar *nodeId, GtkTreeIter *iter)
{
	if (!flIterHash)
		flIterHash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, NULL);

	g_hash_table_insert (flIterHash, (gpointer)nodeId, (gpointer)iter);
}

/* Expansion & Collapsing */

gboolean
ui_node_is_expanded (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	gboolean 	expanded = FALSE;

	iter = ui_node_to_iter (nodeId);
	if (iter) {
		GtkTreeView *treeview = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
		GtkTreePath *path = gtk_tree_model_get_path (gtk_tree_view_get_model (treeview), iter);
		expanded = gtk_tree_view_row_expanded (treeview, path);
		gtk_tree_path_free (path);
	}

	return expanded;
}

void
ui_node_set_expansion (nodePtr folder, gboolean expanded)
{
	GtkTreeIter		*iter;
	GtkTreePath		*path;
	GtkTreeView		*treeview;	

	iter = ui_node_to_iter (folder->id);
	if (!iter)
		return;

	treeview = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
	path = gtk_tree_model_get_path (gtk_tree_view_get_model (treeview), iter);
	if (expanded)
		gtk_tree_view_expand_row (treeview, path, FALSE);
	else
		gtk_tree_view_collapse_row (treeview, path);
	gtk_tree_path_free (path);
}

/* Folder expansion workaround using "empty" nodes */

void
ui_node_add_empty_node (GtkTreeIter *parent)
{
	GtkTreeIter	iter;

	gtk_tree_store_append (feedstore, &iter, parent);
	gtk_tree_store_set (feedstore, &iter,
	                    FS_LABEL, _("<i>(empty)</i>"), /* FIXME: Should this be italicized? */
	                    FS_ICON, icons[ICON_FOLDER_EMPTY],
	                    FS_PTR, NULL,
	                    FS_UNREAD, 0,
	                    -1);
}

void
ui_node_remove_empty_node (GtkTreeIter *parent)
{
	GtkTreeIter	iter;
	nodePtr		node;
	gboolean	valid;
		
	gtk_tree_model_iter_children (GTK_TREE_MODEL (feedstore), &iter, parent);
	do {
		gtk_tree_model_get (GTK_TREE_MODEL (feedstore), &iter, FS_PTR, &node, -1);

		if (!node) {
			gtk_tree_store_remove (feedstore, &iter);
			return;
		}
		
		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (feedstore), &iter);
	} while (valid);
}

/* this function is a workaround to the cant-drop-rows-into-emtpy-
   folders-problem, so we simply pack an "(empty)" entry into each
   empty folder like Nautilus does... */
   
void
ui_node_check_if_folder_is_empty (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	int		count;

	debug1 (DEBUG_GUI, "folder empty check for node id \"%s\"", nodeId);

	/* this function does two things:
	   
	1. add "(empty)" entry to an empty folder
	2. remove an "(empty)" entry from a non empty folder
	(this state is possible after a drag&drop action) */

	iter = ui_node_to_iter (nodeId);
	if (!iter)
		return;
		
	count = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (feedstore), iter);
	
	/* case 1 */
	if (0 == count) {
		ui_node_add_empty_node (iter);
		return;
	}
	
	if (1 == count)
		return;
	
	/* else we could have case 2 */
	ui_node_remove_empty_node (iter);
}

void
ui_node_add (nodePtr node)
{
	gint		position;
	GtkTreeIter	*iter, *parentIter = NULL;

	debug2 (DEBUG_GUI, "adding node \"%s\" as child of parent=\"%s\"", node_get_title(node), (NULL != node->parent)?node_get_title(node->parent):"feed list root");

	g_assert (NULL != node->parent);
	g_assert (NULL == ui_node_to_iter (node->id));

	/* if parent is NULL we have the root folder and don't create a new row! */
	iter = (GtkTreeIter *)g_new0 (GtkTreeIter, 1);
	
	/* if reduced feedlist, show flat treeview */
	if (feedlist_reduced_unread)
		parentIter = NULL;
	else if (node->parent != feedlist_get_root ())
		parentIter = ui_node_to_iter (node->parent->id);

	position = g_slist_index (node->parent->children, node);
	if (position < 0)
		gtk_tree_store_append (feedstore, iter, parentIter);
	else
		gtk_tree_store_insert (feedstore, iter, parentIter, position);

	gtk_tree_store_set (feedstore, iter, FS_PTR, node, -1);
	ui_node_add_iter (node->id, iter);
	ui_node_update (node->id);
	
	if (node->parent != feedlist_get_root ())
		ui_node_check_if_folder_is_empty (node->parent->id);

	if (IS_FOLDER (node))
		ui_node_check_if_folder_is_empty (node->id);
}

void
ui_node_reload_feedlist ()
{
	ui_node_clear_feedlist ();
	ui_node_load_feedlist (feedlist_get_root ());
}

void
ui_node_clear_feedlist ()
{
	gtk_tree_store_clear (feedstore);
	g_hash_table_remove_all (flIterHash);
}

void
ui_node_load_feedlist (nodePtr node)
{
	GSList		*iter;
	
	iter = node->children;
	while (iter) {
		node = (nodePtr)iter->data;
		ui_node_add (node);
		
		if (IS_FOLDER (node) || IS_NODE_SOURCE (node))
			ui_node_load_feedlist (node);

		iter = g_slist_next(iter);
	}
}

void
ui_node_remove_node (nodePtr node)
{
	GtkTreeIter	*iter;
	gboolean 	parentExpanded = FALSE;
	
	iter = ui_node_to_iter (node->id);
	if (!iter)
		return;	/* must be tolerant because of DnD handling */

	if (node->parent)
		parentExpanded = ui_node_is_expanded (node->parent->id); /* If the folder becomes empty, the folder would collapse */
	
	gtk_tree_store_remove (feedstore, iter);
	g_hash_table_remove (flIterHash, node->id);
	g_free (iter);
	
	if (node->parent) {
		ui_node_check_if_folder_is_empty (node->parent->id);
		if (parentExpanded)
			ui_node_set_expansion (node->parent, TRUE);

		ui_node_update (node->parent->id);
	}
}

void
ui_node_update (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	gchar		*label;
	guint		labeltype;
	nodePtr		node;

	node = node_from_id (nodeId);
	iter = ui_node_to_iter (nodeId);
	if (!iter)
		return;

	labeltype = NODE_TYPE (node)->capabilities;
	labeltype &= (NODE_CAPABILITY_SHOW_UNREAD_COUNT |
        	      NODE_CAPABILITY_SHOW_ITEM_COUNT);

	if (node->unreadCount == 0 && (labeltype & NODE_CAPABILITY_SHOW_UNREAD_COUNT))
		labeltype -= NODE_CAPABILITY_SHOW_UNREAD_COUNT;

	switch (labeltype) {
		case NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		     NODE_CAPABILITY_SHOW_ITEM_COUNT:
	     		/* treat like show unread count */
		case NODE_CAPABILITY_SHOW_UNREAD_COUNT:
			label = g_markup_printf_escaped ("<span weight=\"bold\">%s (%u)</span>",
			                        	 node_get_title(node), node->unreadCount);
			break;
		case NODE_CAPABILITY_SHOW_ITEM_COUNT:
			label = g_markup_printf_escaped ("%s (%u)", node_get_title(node), node->itemCount);
		     	break;
		default:
			label = g_markup_printf_escaped ("%s", node_get_title(node));
			break;
	}

	gtk_tree_store_set (feedstore, iter, FS_LABEL, label,
	                                     FS_UNREAD, node->unreadCount,
	                                     FS_ICON, node->available?node_get_icon (node):icons[ICON_UNAVAILABLE],
	                                     -1);
	g_free (label);

	if (node->parent)
		ui_node_update (node->parent->id);
}

/* node renaming dialog */

static void
on_nodenamedialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	nodePtr	node = (nodePtr)user_data;

	if (response_id == GTK_RESPONSE_OK) {
		node_set_title (node, (gchar *) gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "nameentry"))));

		ui_node_update (node->id);
		feedlist_schedule_save ();
		ui_popup_update_menues ();
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
	nodenamedialog = NULL;
}

void
ui_node_rename (nodePtr node)
{
	GtkWidget	*nameentry;
	
	if (!nodenamedialog || !G_IS_OBJECT (nodenamedialog))
		nodenamedialog = liferea_dialog_new (NULL, "nodenamedialog");

	nameentry = liferea_dialog_lookup (nodenamedialog, "nameentry");
	gtk_entry_set_text (GTK_ENTRY (nameentry), node_get_title (node));
	g_signal_connect (G_OBJECT (nodenamedialog), "response", 
	                  G_CALLBACK (on_nodenamedialog_response), node);
	gtk_widget_show (nodenamedialog);
}
