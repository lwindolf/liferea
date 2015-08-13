/**
 * @file feed_list_node.c  Handling feed list nodes
 * 
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2015 Lars Windolf <lars.windolf@gmx.de>
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

#include "ui/feed_list_node.h"

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "fl_sources/node_source.h"
#include "folder.h"
#include "render.h"
#include "vfolder.h"
#include "ui/icons.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/feed_list_view.h"

static GHashTable	*flIterHash = NULL;	/**< hash table used for fast node id <-> tree iter lookup */
static GtkWidget	*nodenamedialog = NULL;

GtkTreeIter *
feed_list_node_to_iter (const gchar *nodeId)
{
	if (!flIterHash)
		return NULL;

	return (GtkTreeIter *)g_hash_table_lookup (flIterHash, (gpointer)nodeId);
}

void
feed_list_node_update_iter (const gchar *nodeId, GtkTreeIter *iter)
{
	GtkTreeIter *old;

	if (!flIterHash)
		return;

	old = (GtkTreeIter *)g_hash_table_lookup (flIterHash, (gpointer)nodeId);
	if (old)
		*old = *iter;
}

static void
feed_list_node_add_iter (const gchar *nodeId, GtkTreeIter *iter)
{
	if (!flIterHash)
		flIterHash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

	g_hash_table_insert (flIterHash, (gpointer)nodeId, (gpointer)iter);
}

/* Expansion & Collapsing */

gboolean
feed_list_node_is_expanded (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	gboolean 	expanded = FALSE;
	
	if (feedlist_reduced_unread)
		return FALSE;

	iter = feed_list_node_to_iter (nodeId);
	if (iter) {
		GtkTreeView *treeview = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
		GtkTreePath *path = gtk_tree_model_get_path (gtk_tree_view_get_model (treeview), iter);
		expanded = gtk_tree_view_row_expanded (treeview, path);
		gtk_tree_path_free (path);
	}

	return expanded;
}

void
feed_list_node_set_expansion (nodePtr folder, gboolean expanded)
{
	GtkTreeIter		*iter;
	GtkTreePath		*path;
	GtkTreeView		*treeview;
	
	if (feedlist_reduced_unread)
		return;

	iter = feed_list_node_to_iter (folder->id);
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
feed_list_node_add_empty_node (GtkTreeIter *parent)
{
	GtkTreeIter	iter;

	gtk_tree_store_append (feedstore, &iter, parent);
	gtk_tree_store_set (feedstore, &iter,
	                    FS_LABEL, _("(Empty)"),
	                    FS_PTR, NULL,
	                    FS_UNREAD, 0,
			    FS_COUNT, "",
	                    -1);
}

void
feed_list_node_remove_empty_node (GtkTreeIter *parent)
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
   
static void
feed_list_node_check_if_folder_is_empty (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	int		count;

	debug1 (DEBUG_GUI, "folder empty check for node id \"%s\"", nodeId);

	/* this function does two things:
	   
	1. add "(empty)" entry to an empty folder
	2. remove an "(empty)" entry from a non empty folder
	(this state is possible after a drag&drop action) */

	iter = feed_list_node_to_iter (nodeId);
	if (!iter)
		return;
		
	count = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (feedstore), iter);
	
	/* case 1 */
	if (0 == count) {
		feed_list_node_add_empty_node (iter);
		return;
	}
	
	if (1 == count)
		return;
	
	/* else we could have case 2 */
	feed_list_node_remove_empty_node (iter);
}

void
feed_list_node_add (nodePtr node)
{
	gint		position;
	GtkTreeIter	*iter, *parentIter = NULL;

	debug2 (DEBUG_GUI, "adding node \"%s\" as child of parent=\"%s\"", node_get_title(node), (NULL != node->parent)?node_get_title(node->parent):"feed list root");

	g_assert (NULL != node->parent);
	g_assert (NULL == feed_list_node_to_iter (node->id));

	/* if parent is NULL we have the root folder and don't create a new row! */
	iter = (GtkTreeIter *)g_new0 (GtkTreeIter, 1);
	
	/* if reduced feedlist, show flat treeview */
	if (feedlist_reduced_unread)
		parentIter = NULL;
	else if (node->parent != feedlist_get_root ())
		parentIter = feed_list_node_to_iter (node->parent->id);

	position = g_slist_index (node->parent->children, node);

	if (feedlist_reduced_unread || position < 0)
		gtk_tree_store_append (feedstore, iter, parentIter);
	else
		gtk_tree_store_insert (feedstore, iter, parentIter, position);

	gtk_tree_store_set (feedstore, iter, FS_PTR, node, -1);
	feed_list_node_add_iter (node->id, iter);
	feed_list_node_update (node->id);
	
	if (node->parent != feedlist_get_root ())
		feed_list_node_check_if_folder_is_empty (node->parent->id);

	if (IS_FOLDER (node))
		feed_list_node_check_if_folder_is_empty (node->id);
}

static void
feed_list_node_load_feedlist (nodePtr node)
{
	GSList		*iter;
	
	iter = node->children;
	while (iter) {
		node = (nodePtr)iter->data;
		feed_list_node_add (node);
		
		if (IS_FOLDER (node) || IS_NODE_SOURCE (node))
			feed_list_node_load_feedlist (node);

		iter = g_slist_next(iter);
	}
}

static void
feed_list_node_clear_feedlist ()
{
	gtk_tree_store_clear (feedstore);
	g_hash_table_remove_all (flIterHash);
}

void
feed_list_node_reload_feedlist ()
{
	feed_list_node_clear_feedlist ();
	feed_list_node_load_feedlist (feedlist_get_root ());
}

void
feed_list_node_remove_node (nodePtr node)
{
	GtkTreeIter	*iter;
	gboolean 	parentExpanded = FALSE;
	
	iter = feed_list_node_to_iter (node->id);
	if (!iter)
		return;	/* must be tolerant because of DnD handling */

	if (node->parent)
		parentExpanded = feed_list_node_is_expanded (node->parent->id); /* If the folder becomes empty, the folder would collapse */
	
	gtk_tree_store_remove (feedstore, iter);
	g_hash_table_remove (flIterHash, node->id);
	
	if (node->parent) {
		feed_list_node_check_if_folder_is_empty (node->parent->id);
		if (parentExpanded)
			feed_list_node_set_expansion (node->parent, TRUE);

		feed_list_node_update (node->parent->id);
	}
}

void
feed_list_node_update (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	gchar		*label, *count = NULL;
	guint		labeltype;
	nodePtr		node;

	static gchar	*countColor = NULL;

	node = node_from_id (nodeId);
	iter = feed_list_node_to_iter (nodeId);
	if (!iter)
		return;

	/* Initialize unread item color Pango CSS */
	if (!countColor) {
		const gchar *bg = NULL, *fg = NULL;

		bg = render_get_theme_color ("FEEDLIST_UNREAD_BG");
		fg = render_get_theme_color ("FEEDLIST_UNREAD_FG");
		if (fg && bg) {
			countColor = g_strdup_printf ("foreground='#%s' background='#%s'", fg, bg);
			debug1 (DEBUG_HTML, "Feed list unread CSS: %s\n", countColor);
		}
	}

	labeltype = NODE_TYPE (node)->capabilities;
	labeltype &= (NODE_CAPABILITY_SHOW_UNREAD_COUNT |
        	      NODE_CAPABILITY_SHOW_ITEM_COUNT);

	if (node->unreadCount == 0 && (labeltype & NODE_CAPABILITY_SHOW_UNREAD_COUNT))
		labeltype &= ~NODE_CAPABILITY_SHOW_UNREAD_COUNT;

	label = g_markup_escape_text (node_get_title (node), -1);
	switch (labeltype) {
		case NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		     NODE_CAPABILITY_SHOW_ITEM_COUNT:
	     		/* treat like show unread count */
		case NODE_CAPABILITY_SHOW_UNREAD_COUNT:
			count = g_strdup_printf ("<span weight='bold' %s> %u </span>", countColor?countColor:"", node->unreadCount);
			break;
		case NODE_CAPABILITY_SHOW_ITEM_COUNT:
			count = g_strdup_printf ("<span weight='bold' %s> %u </span>", countColor?countColor:"", node->itemCount);
		     	break;
		default:
			break;
	}

	/* Extra message for search folder rebuilds */
	if (IS_VFOLDER (node) && node->data) {
		if (((vfolderPtr)node->data)->reloading) {
			gchar *tmp = label;
			label = g_strdup_printf (_("%s\n<i>Rebuilding</i>"), label);
			g_free (tmp);
		}
	}

	gtk_tree_store_set (feedstore, iter,
	                    FS_LABEL, label,
	                    FS_UNREAD, node->unreadCount,
	                    FS_ICON, node->available?node_get_icon (node):icon_get (ICON_UNAVAILABLE),
	                    FS_COUNT, count,
	                    -1);
	g_free (label);
	g_free (count);

	if (node->parent)
		feed_list_node_update (node->parent->id);
}

/* node renaming dialog */

static void
on_nodenamedialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	nodePtr	node = (nodePtr)user_data;

	if (response_id == GTK_RESPONSE_OK) {
		node_set_title (node, (gchar *) gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "nameentry"))));

		feed_list_node_update (node->id);
		feedlist_schedule_save ();
	}
	
	gtk_widget_destroy (GTK_WIDGET (dialog));
	nodenamedialog = NULL;
}

void
feed_list_node_rename (nodePtr node)
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

/* node deletion dialog */

static void
feed_list_node_remove_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{	
	if (GTK_RESPONSE_ACCEPT == response_id)
		feedlist_remove_node ((nodePtr)user_data);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
feed_list_node_remove (nodePtr node)
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
	                        _("_Cancel"), GTK_RESPONSE_CANCEL,
	                        _("_Delete"), GTK_RESPONSE_ACCEPT,
	                        NULL);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Deletion Confirmation"));
	gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
	gtk_window_set_transient_for (GTK_WINDOW (dialog), mainwindow);

	g_free (text);
	
	gtk_widget_show_all (dialog);

	g_signal_connect (G_OBJECT (dialog), "response",
	                  G_CALLBACK (feed_list_node_remove_cb), node);
}
