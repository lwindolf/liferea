/**
 * @file feed_list_view.c  the feed list in a GtkTreeView
 *
 * Copyright (C) 2004-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "ui/feed_list_view.h"

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "net_monitor.h"
#include "newsbin.h"
#include "render.h"
#include "vfolder.h"
#include "ui/icons.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/subscription_dialog.h"
#include "ui/ui_dnd.h"
#include "fl_sources/node_source.h"

struct _FeedListView {
	GObject			parentInstance;

	GtkTreeView		*treeview;
	GtkTreeModel	*filter;
	GtkTreeStore	*feedstore;
	GtkSearchEntry	*titlefilter;
	gchar		*casefolded_title_str;

	GHashTable		*flIterHash;				/**< hash table used for fast node id <-> tree iter lookup */

	enum feedlistViewMode	view_mode;
};

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static FeedListView *flv = NULL;	// singleton

static guint feed_list_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (FeedListView, feed_list_view, G_TYPE_OBJECT);

static void
feed_list_view_finalize (GObject *object)
{
	g_free (((FeedListView*)object)->casefolded_title_str);
}

static void
feed_list_view_class_init (FeedListViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = feed_list_view_finalize;

	feed_list_view_signals[SELECTION_CHANGED] =
		g_signal_new ("selection-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__STRING,
		G_TYPE_NONE,
		1,
		G_TYPE_STRING);
}

static void
feed_list_view_init (FeedListView *f)
{
}


enum feedlistViewMode
feed_list_view_mode_string_to_value (const gchar *str_mode)
{
	/* TODO: Load these values from Gsettings Schema. */
	if (!g_strcmp0 ("normal", str_mode)) {
		return FEEDLIST_VIEW_MODE_NORMAL;
	}
	if (!g_strcmp0 ("reduced", str_mode)) {
		return FEEDLIST_VIEW_MODE_REDUCED;
	}
	if (!g_strcmp0 ("flat", str_mode)) {
		return FEEDLIST_VIEW_MODE_FLAT;
	}

	/* Unknown or invalid mode, assume normal. */
	return FEEDLIST_VIEW_MODE_NORMAL;
}

const gchar *
feed_list_view_mode_value_to_string (enum feedlistViewMode mode)
{
	/* TODO: Load these values from Gsettings Schema. */
	switch (mode) {
	case FEEDLIST_VIEW_MODE_NORMAL:
		return "normal";
	case FEEDLIST_VIEW_MODE_REDUCED:
		return "reduced";
	case FEEDLIST_VIEW_MODE_FLAT:
		return "flat";
	}
	/* Unknown or invalid mode, assume normal. */
	return "normal";
}


static void
feed_list_view_row_changed_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter)
{
	nodePtr node;

	gtk_tree_model_get (model, iter, FS_PTR, &node, -1);
	if (node)
		feed_list_view_update_iter (node->id, iter);
}

static void
feed_list_view_selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter		iter;
	GtkTreeModel	*model;
	nodePtr			node;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
	 	gtk_tree_model_get (model, &iter, FS_PTR, &node, -1);

		debug (DEBUG_GUI, "feed list selection changed to \"%s\"", node?node_get_title (node):"Empty node");

		if (!node) {
			/* The selected iter is an "empty" node added to an empty folder. We get the parent's node
			 * to set it as the selected node. This is useful if the user adds a feed, the folder will
			 * be used as location for the new node. */
			GtkTreeIter parent;
			if (gtk_tree_model_iter_parent (model, &parent, &iter))
				gtk_tree_model_get (model, &parent, FS_PTR, &node, -1);
			else {
				debug (DEBUG_GUI, "A selected null node has no parent. This should not happen.");
				return;
			}
		}

		/* 1.) update feed list and item list states */
		g_signal_emit_by_name (FEED_LIST_VIEW (flv), "selection-changed", node->id);

		/* 2.) Refilter the GtkTreeView to get rid of nodes with 0 unread
		   messages when in reduced mode. */
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (flv->filter));

	} else {
		/* If we cannot get the new selection we keep the old one
		   this happens when we're doing drag&drop for example. */
	}
}

static void
feed_list_view_row_activated_cb (GtkTreeView *tv, GtkTreePath *path, GtkTreeViewColumn *col, gpointer data)
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
feed_list_view_key_press_cb (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if ((event->type == GDK_KEY_PRESS) &&
	    (event->state == 0) &&
	    (event->keyval == GDK_KEY_Delete)) {
		nodePtr node = feedlist_get_selected ();

		if(node) {
			if (event->state & GDK_SHIFT_MASK)
				feedlist_remove_node (node);
			else
				feed_list_view_remove (node);
			return TRUE;
		}
	}
	return FALSE;
}

static gboolean
feed_list_view_filter_visible_function (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gint			count;
	nodePtr			node;

	if (flv->view_mode == FEEDLIST_VIEW_MODE_NORMAL) {
		return TRUE;
	}

	/* From here, assume mode is REDUCED or FLAT */

	gtk_tree_model_get (model, iter, FS_PTR, &node, FS_UNREAD, &count, -1);
	if (!node) {
		return FALSE;
	}

	if (flv->titlefilter && node->title && flv->casefolded_title_str) {
		gchar *text = g_utf8_casefold (node->title, -1);
		const gboolean found = strstr (text, flv->casefolded_title_str) ? TRUE : FALSE;
		g_free (text);
		if (!found) {
			return FALSE;
		}
	}

	if (IS_NEWSBIN(node) && node->data && ((feedPtr)node->data)->alwaysShowInReduced) {
		return TRUE;
	}

	if (IS_FOLDER (node) || IS_NODE_SOURCE (node)) {
		return FALSE;
	}

	if (IS_VFOLDER (node))
		return TRUE;

	/* Do not hide in any case if the node is selected, otherwise
	   the last unread item of a feed causes the feed to vanish
	   when clicking it */
	if (feedlist_get_selected () == node)
		return TRUE;

	if (flv->view_mode == FEEDLIST_VIEW_MODE_REDUCED && count == 0) {
		return FALSE;
	}

	return TRUE;
}

static void
feed_list_view_expand (nodePtr node)
{
	if (node->parent)
		feed_list_view_expand (node->parent);

	feed_list_view_set_expansion (node, TRUE);
}

static void
feed_list_view_restore_folder_expansion (nodePtr node)
{
	if (node->expanded)
		feed_list_view_expand (node);

	node_foreach_child (node, feed_list_view_restore_folder_expansion);
}

static void
feed_list_view_mode_changed (void)
{
	if (flv->view_mode != FEEDLIST_VIEW_MODE_NORMAL) {
		gtk_tree_view_set_reorderable (flv->treeview, FALSE);
		gtk_tree_view_set_model (flv->treeview, GTK_TREE_MODEL (flv->filter));
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (flv->filter));
		gtk_widget_show (GTK_WIDGET (flv->titlefilter));
	} else {
		gtk_tree_view_set_reorderable (flv->treeview, TRUE);
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (flv->filter));
		gtk_tree_view_set_model (flv->treeview, GTK_TREE_MODEL (flv->feedstore));
		gtk_widget_hide (GTK_WIDGET (flv->titlefilter));

		feedlist_foreach (feed_list_view_restore_folder_expansion);
	}
}

static void
feed_list_view_set_view_mode (enum feedlistViewMode mode)
{
	flv->view_mode = mode;
	conf_set_enum_value (FEEDLIST_VIEW_MODE, flv->view_mode);
	feed_list_view_mode_changed ();
	feed_list_view_reload_feedlist ();
}

static gint
feed_list_view_sort_folder_compare (gconstpointer a, gconstpointer b)
{
	nodePtr n1 = (nodePtr)a;
	nodePtr n2 = (nodePtr)b;

	gchar *s1 = g_utf8_casefold (n1->title, -1);
	gchar *s2 = g_utf8_casefold (n2->title, -1);

	gint result = strcmp (s1, s2);

	g_free (s1);
	g_free (s2);

	return result;
}

void
feed_list_view_sort_folder (nodePtr folder)
{
	GtkTreeView             *treeview;

	treeview = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
	/* Unset the model from the view before clearing it and rebuilding it.*/
	gtk_tree_view_set_model (treeview, NULL);
	folder->children = g_slist_sort (folder->children, feed_list_view_sort_folder_compare);
	feed_list_view_reload_feedlist ();
	/* Reduce mode didn't actually change but we need to set the
	 * correct model according to the setting in the same way : */
	feed_list_view_mode_changed ();
	feedlist_foreach (feed_list_view_restore_folder_expansion);
	feedlist_schedule_save ();
}

FeedListView *
feed_list_view_create (GtkTreeView *treeview)
{
	GtkCellRenderer		*titleRenderer, *countRenderer;
	GtkCellRenderer		*iconRenderer;
	GtkTreeViewColumn 	*column, *column2;
	GtkTreeSelection	*select;


	/* Set up store */
	g_assert (NULL == flv);
	flv = FEED_LIST_VIEW (g_object_new (FEED_LIST_VIEW_TYPE, NULL));

	flv->treeview = treeview;
	flv->casefolded_title_str = NULL;
	flv->titlefilter = GTK_SEARCH_ENTRY (liferea_shell_lookup ("titleFilter"));
	flv->feedstore = gtk_tree_store_new (FS_LEN,
	                                     G_TYPE_STRING,
   	                                     G_TYPE_ICON,
	                                     G_TYPE_POINTER,
	                                     G_TYPE_UINT,
	                                     G_TYPE_STRING);

	gtk_tree_view_set_model (GTK_TREE_VIEW (flv->treeview), GTK_TREE_MODEL (flv->feedstore));

	/* Prepare filter */
	flv->filter = gtk_tree_model_filter_new (GTK_TREE_MODEL (flv->feedstore), NULL);
	gtk_tree_model_filter_set_visible_func (GTK_TREE_MODEL_FILTER (flv->filter),
	                                        feed_list_view_filter_visible_function,
	                                        NULL,
	                                        NULL);

	g_signal_connect (G_OBJECT (flv->feedstore), "row-changed",
                      G_CALLBACK (feed_list_view_row_changed_cb), flv);

	/* we render the icon/state, the feed title and the unread count */
	iconRenderer  = gtk_cell_renderer_pixbuf_new ();
	titleRenderer = gtk_cell_renderer_text_new ();
	countRenderer = gtk_cell_renderer_text_new ();

	gtk_cell_renderer_set_alignment (countRenderer, 1.0, 0);

	column  = gtk_tree_view_column_new ();
	column2 = gtk_tree_view_column_new ();

	gtk_tree_view_column_pack_start (column, iconRenderer, FALSE);
	gtk_tree_view_column_pack_start (column, titleRenderer, TRUE);
	gtk_tree_view_column_pack_end   (column2, countRenderer, FALSE);

	gtk_tree_view_column_add_attribute (column, iconRenderer, "gicon", FS_ICON);
	gtk_tree_view_column_add_attribute (column, titleRenderer, "markup", FS_LABEL);
	gtk_tree_view_column_add_attribute (column2, countRenderer, "markup", FS_COUNT);

	gtk_tree_view_column_set_expand (column, TRUE);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_column_set_sizing (column2, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	gtk_tree_view_append_column (flv->treeview, column);
	gtk_tree_view_append_column (flv->treeview, column2);

	g_object_set (titleRenderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	g_signal_connect (G_OBJECT (flv->treeview), "row-activated",   G_CALLBACK (feed_list_view_row_activated_cb), flv);
	g_signal_connect (G_OBJECT (flv->treeview), "key-press-event", G_CALLBACK (feed_list_view_key_press_cb), flv);

	select = gtk_tree_view_get_selection (flv->treeview);
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (select), "changed",
	                  G_CALLBACK (feed_list_view_selection_changed_cb),
                	  flv);
	g_signal_connect (GTK_SEARCH_ENTRY (flv->titlefilter), "search-changed", G_CALLBACK (on_titlefilter_entry_changed), flv);

	conf_get_enum_value (FEEDLIST_VIEW_MODE, (gint *) &flv->view_mode);
	if (flv->view_mode != FEEDLIST_VIEW_MODE_NORMAL)
		feed_list_view_mode_changed ();	/* before menu setup for reduced mode check box to be correct */

	ui_dnd_setup_feedlist (flv->feedstore);


	return flv;
}

void
feed_list_view_select (nodePtr node)
{
	GtkTreeModel *model = gtk_tree_view_get_model (flv->treeview);

	if (model && node && node != feedlist_get_root ()) {
		GtkTreePath *path = NULL;

		/* in filtered mode we need to convert the iterator */
		if (flv->view_mode != FEEDLIST_VIEW_MODE_NORMAL) {
			GtkTreeIter iter;
			gboolean valid = gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (flv->filter), &iter, feed_list_view_to_iter (node->id));
			if (valid)
				path = gtk_tree_model_get_path (model, &iter);
		} else {
			path = gtk_tree_model_get_path (model, feed_list_view_to_iter (node->id));
		}

		if (node->parent)
			feed_list_view_expand (node->parent);

		if (path) {
			gtk_tree_view_scroll_to_cell (flv->treeview, path, NULL, FALSE, 0.0, 0.0);
			gtk_tree_view_set_cursor (flv->treeview, path, NULL, FALSE);
			gtk_tree_path_free (path);
		}
 	} else {
		GtkTreeSelection *selection = gtk_tree_view_get_selection (flv->treeview);
		gtk_tree_selection_unselect_all (selection);
	}
}

// Callbacks

void
on_menu_properties (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	nodePtr node = feedlist_get_selected ();

	NODE_TYPE (node)->request_properties (node);
}

void
on_menu_delete(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	feed_list_view_remove (feedlist_get_selected ());
}

static void
do_menu_update (nodePtr node)
{
	if (network_monitor_is_online ())
		node_update_subscription (node, GUINT_TO_POINTER (FEED_REQ_PRIORITY_HIGH));
	else
		liferea_shell_set_status_bar (_("Liferea is in offline mode. No update possible."));

}

void
on_menu_update (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	nodePtr node = NULL;

	if (user_data)
		node = (nodePtr) user_data;
	else
		node = feedlist_get_selected ();

	if (node)
		do_menu_update (node);
	else
		g_warning ("on_menu_update: no feedlist selected");
}

void
on_menu_update_all(GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	do_menu_update (feedlist_get_root ());
}

void
on_action_mark_all_read (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	nodePtr 	feedlist;
	gboolean 	confirm_mark_read;
	gboolean 	do_mark_read = TRUE;

	if (!g_strcmp0 (g_action_get_name (G_ACTION (action)), "mark-all-feeds-read"))
		feedlist = feedlist_get_root ();
	else if (user_data)
		feedlist = (nodePtr) user_data;
	else
		feedlist = feedlist_get_selected ();

	conf_get_bool_value (CONFIRM_MARK_ALL_READ, &confirm_mark_read);

	if (confirm_mark_read) {
		gint result;
		GtkMessageDialog *confirm_dialog = GTK_MESSAGE_DIALOG (liferea_dialog_new ("mark_read_dialog"));
		GtkWidget *dont_ask_toggle = liferea_dialog_lookup (GTK_WIDGET (confirm_dialog), "dontAskAgainToggle");
		const gchar *feed_title = (feedlist_get_root () == feedlist) ? _("all feeds"):node_get_title (feedlist);
		gchar *primary_message = g_strdup_printf (_("Mark %s as read ?"), feed_title);

		g_object_set (confirm_dialog, "text", primary_message, NULL);
		g_free (primary_message);
		gtk_message_dialog_format_secondary_text (confirm_dialog, _("Are you sure you want to mark all items in %s as read ?"), feed_title);

		conf_bind (CONFIRM_MARK_ALL_READ, dont_ask_toggle, "active", G_SETTINGS_BIND_DEFAULT | G_SETTINGS_BIND_INVERT_BOOLEAN);

		result = gtk_dialog_run (GTK_DIALOG (confirm_dialog));
		if (result != GTK_RESPONSE_OK)
			do_mark_read = FALSE;
		gtk_widget_destroy (GTK_WIDGET (confirm_dialog));
	}

	if (do_mark_read)
		feedlist_mark_all_read (feedlist);
}

void
on_menu_feed_new (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_type_request_interactive_add (feed_get_node_type ());
}

void
on_new_plugin_activate (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_type_request_interactive_add (node_source_get_node_type ());
}

void
on_new_newsbin_activate (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_type_request_interactive_add (newsbin_get_node_type ());
}

void
on_menu_folder_new (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_type_request_interactive_add (folder_get_node_type ());
}

void
on_new_vfolder_activate (GSimpleAction *menuitem, GVariant *parameter, gpointer user_data)
{
	node_type_request_interactive_add (vfolder_get_node_type ());
}

void
on_feedlist_view_mode_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	const gchar *str_new_mode = g_variant_get_string (parameter, NULL);
	enum feedlistViewMode new_mode = feed_list_view_mode_string_to_value (str_new_mode);

	GVariant *tmp = g_action_get_state (G_ACTION(action));
	const gchar *str_cur_mode = g_variant_get_string (tmp, NULL);
	enum feedlistViewMode cur_mode = feed_list_view_mode_string_to_value (str_cur_mode);
	g_variant_unref (tmp);

	if (new_mode != cur_mode) {
		feed_list_view_set_view_mode (new_mode);
		g_simple_action_set_state (action, g_variant_new_string (str_new_mode));
	}
}

void
on_titlefilter_entry_changed (GtkEditable *self, gpointer user_data)
{
	if (flv->view_mode == FEEDLIST_VIEW_MODE_REDUCED
	|| flv->view_mode == FEEDLIST_VIEW_MODE_FLAT) {
		g_free(flv->casefolded_title_str);
		flv->casefolded_title_str = g_utf8_casefold (gtk_entry_get_text (GTK_ENTRY (flv->titlefilter)), -1);
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (flv->filter));
	}
}


// Handling feed list nodes

GtkTreeIter *
feed_list_view_to_iter (const gchar *nodeId)
{
	if (!flv->flIterHash)
		return NULL;

	return (GtkTreeIter *)g_hash_table_lookup (flv->flIterHash, (gpointer)nodeId);
}

void
feed_list_view_update_iter (const gchar *nodeId, GtkTreeIter *iter)
{
	GtkTreeIter *old;

	if (!flv->flIterHash)
		return;

	old = (GtkTreeIter *)g_hash_table_lookup (flv->flIterHash, (gpointer)nodeId);
	if (old)
		*old = *iter;
}

static void
feed_list_view_add_iter (const gchar *nodeId, GtkTreeIter *iter)
{
	if (!flv->flIterHash)
		flv->flIterHash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

	g_hash_table_insert (flv->flIterHash, (gpointer)nodeId, (gpointer)iter);
}

/* Expansion & Collapsing */

gboolean
feed_list_view_is_expanded (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	gboolean 	expanded = FALSE;

	if (flv->view_mode != FEEDLIST_VIEW_MODE_NORMAL)
		return FALSE;

	iter = feed_list_view_to_iter (nodeId);
	if (iter) {
		GtkTreePath *path = gtk_tree_model_get_path (gtk_tree_view_get_model (flv->treeview), iter);
		expanded = gtk_tree_view_row_expanded (flv->treeview, path);
		gtk_tree_path_free (path);
	}

	return expanded;
}

void
feed_list_view_set_expansion (nodePtr folder, gboolean expanded)
{
	GtkTreeIter		*iter;
	GtkTreePath		*path;

	if (flv->view_mode != FEEDLIST_VIEW_MODE_NORMAL)
		return;

	iter = feed_list_view_to_iter (folder->id);
	if (!iter)
		return;

	path = gtk_tree_model_get_path (gtk_tree_view_get_model (flv->treeview), iter);
	if (expanded)
		gtk_tree_view_expand_row (flv->treeview, path, FALSE);
	else
		gtk_tree_view_collapse_row (flv->treeview, path);
	gtk_tree_path_free (path);
}

/* Folder expansion workaround using "empty" nodes */

void
feed_list_view_add_empty_node (GtkTreeIter *parent)
{
	GtkTreeIter	iter;

	gtk_tree_store_append (flv->feedstore, &iter, parent);
	gtk_tree_store_set (flv->feedstore, &iter,
	                    FS_LABEL, _("(Empty)"),
	                    FS_PTR, NULL,
	                    FS_UNREAD, 0,
			    FS_COUNT, "",
	                    -1);
}

void
feed_list_view_remove_empty_node (GtkTreeIter *parent)
{
	GtkTreeIter	iter;
	nodePtr		node;
	gboolean	valid;

	gtk_tree_model_iter_children (GTK_TREE_MODEL (flv->feedstore), &iter, parent);
	do {
		gtk_tree_model_get (GTK_TREE_MODEL (flv->feedstore), &iter, FS_PTR, &node, -1);

		if (!node) {
			gtk_tree_store_remove (flv->feedstore, &iter);
			return;
		}

		valid = gtk_tree_model_iter_next (GTK_TREE_MODEL (flv->feedstore), &iter);
	} while (valid);
}

/* this function is a workaround to the cant-drop-rows-into-emtpy-
   folders-problem, so we simply pack an "(empty)" entry into each
   empty folder like Nautilus does... */

static void
feed_list_view_check_if_folder_is_empty (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	int		count;

	debug (DEBUG_GUI, "folder empty check for node id \"%s\"", nodeId);

	/* this function does two things:

	1. add "(empty)" entry to an empty folder
	2. remove an "(empty)" entry from a non empty folder
	(this state is possible after a drag&drop action) */

	iter = feed_list_view_to_iter (nodeId);
	if (!iter)
		return;

	count = gtk_tree_model_iter_n_children (GTK_TREE_MODEL (flv->feedstore), iter);

	/* case 1 */
	if (0 == count) {
		feed_list_view_add_empty_node (iter);
		return;
	}

	if (1 == count)
		return;

	/* else we could have case 2 */
	feed_list_view_remove_empty_node (iter);
}

void
feed_list_view_add_node (nodePtr node)
{
	gint		position;
	GtkTreeIter	*iter, *parentIter = NULL;

	debug (DEBUG_GUI, "adding node \"%s\" as child of parent=\"%s\"", node_get_title(node), (NULL != node->parent)?node_get_title(node->parent):"feed list root");

	g_assert (NULL != node->parent);
	g_assert (NULL == feed_list_view_to_iter (node->id));

	/* if parent is NULL we have the root folder and don't create a new row! */
	iter = g_new0 (GtkTreeIter, 1);

	/* if reduced feedlist, show flat treeview */
	if (flv->view_mode != FEEDLIST_VIEW_MODE_NORMAL)
		parentIter = NULL;
	else if (node->parent != feedlist_get_root ())
		parentIter = feed_list_view_to_iter (node->parent->id);

	position = g_slist_index (node->parent->children, node);

	if ((flv->view_mode != FEEDLIST_VIEW_MODE_NORMAL) || position < 0)
		gtk_tree_store_append (flv->feedstore, iter, parentIter);
	else
		gtk_tree_store_insert (flv->feedstore, iter, parentIter, position);

	gtk_tree_store_set (flv->feedstore, iter, FS_PTR, node, -1);
	feed_list_view_add_iter (node->id, iter);
	feed_list_view_update_node (node->id);

	if (node->parent != feedlist_get_root ())
		feed_list_view_check_if_folder_is_empty (node->parent->id);

	if (IS_FOLDER (node))
		feed_list_view_check_if_folder_is_empty (node->id);
}

static void
feed_list_view_load_feedlist (nodePtr node)
{
	GSList		*iter;

	iter = node->children;
	while (iter) {
		node = (nodePtr)iter->data;
		feed_list_view_add_node (node);

		if (IS_FOLDER (node) || IS_NODE_SOURCE (node))
			feed_list_view_load_feedlist (node);

		iter = g_slist_next(iter);
	}
}

static void
feed_list_view_clear_feedlist ()
{
	gtk_tree_store_clear (flv->feedstore);
	g_hash_table_remove_all (flv->flIterHash);
}

void
feed_list_view_reload_feedlist ()
{
	feed_list_view_clear_feedlist ();
	feed_list_view_load_feedlist (feedlist_get_root ());
}

void
feed_list_view_remove_node (nodePtr node)
{
	GtkTreeIter	*iter;
	gboolean 	parentExpanded = FALSE;

	iter = feed_list_view_to_iter (node->id);
	if (!iter)
		return;	/* must be tolerant because of DnD handling */

	if (node->parent)
		parentExpanded = feed_list_view_is_expanded (node->parent->id); /* If the folder becomes empty, the folder would collapse */

	gtk_tree_store_remove (flv->feedstore, iter);
	g_hash_table_remove (flv->flIterHash, node->id);

	if (node->parent) {
		feed_list_view_check_if_folder_is_empty (node->parent->id);
		if (parentExpanded)
			feed_list_view_set_expansion (node->parent, TRUE);

		feed_list_view_update_node (node->parent->id);
	}
}

void
feed_list_view_update_node (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	gchar		*label, *count = NULL;
	guint		labeltype;
	nodePtr		node;

	static gchar	*countColor = NULL;

	node = node_from_id (nodeId);
	iter = feed_list_view_to_iter (nodeId);
	if (!iter)
		return;

	/* Initialize unread item color Pango CSS */
	if (!countColor) {
		const gchar *bg = NULL, *fg = NULL;

		bg = render_get_theme_color ("FEEDLIST_UNREAD_BG");
		fg = render_get_theme_color ("FEEDLIST_UNREAD_FG");
		if (fg && bg) {
			countColor = g_strdup_printf ("foreground='#%s' background='#%s'", fg, bg);
			debug (DEBUG_HTML, "Feed list unread CSS: %s", countColor);
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

	if (IS_VFOLDER (node) && node->data) {
		/* Extra message for search folder rebuilds */
		if (((vfolderPtr)node->data)->reloading) {
			gchar *tmp = label;
			label = g_strdup_printf (_("%s\n<i>Rebuilding</i>"), label);
			g_free (tmp);
		}
	}

	gtk_tree_store_set (flv->feedstore, iter,
	                    FS_LABEL, label,
	                    FS_UNREAD, node->unreadCount,
	                    FS_ICON, node->available?node_get_icon (node):icon_get (ICON_UNAVAILABLE),
	                    FS_COUNT, count,
	                    -1);
	g_free (label);
	g_free (count);

	if (node->parent)
		feed_list_view_update_node (node->parent->id);
}

/* node renaming dialog */

static void
on_nodenamedialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	nodePtr	node = (nodePtr)user_data;

	if (response_id == GTK_RESPONSE_OK) {
		node_set_title (node, (gchar *) gtk_entry_get_text (GTK_ENTRY (liferea_dialog_lookup (GTK_WIDGET (dialog), "nameentry"))));

		feed_list_view_update_node (node->id);
		feedlist_schedule_save ();
	}

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
feed_list_view_rename_node (nodePtr node)
{
	GtkWidget	*nameentry, *dialog;

	dialog = liferea_dialog_new ("rename_node");

	nameentry = liferea_dialog_lookup (dialog, "nameentry");
	gtk_entry_set_text (GTK_ENTRY (nameentry), node_get_title (node));
	g_signal_connect (G_OBJECT (dialog), "response",
	                  G_CALLBACK (on_nodenamedialog_response), node);
	gtk_widget_show (dialog);
}

/* node deletion dialog */

static void
feed_list_view_remove_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	if (GTK_RESPONSE_ACCEPT == response_id)
		feedlist_remove_node ((nodePtr)user_data);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
feed_list_view_remove (nodePtr node)
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
	                  G_CALLBACK (feed_list_view_remove_cb), node);
}

static void
feed_list_view_add_duplicate_url_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	subscriptionPtr tempSubscription = (subscriptionPtr) user_data;

	if (GTK_RESPONSE_ACCEPT == response_id) {
		feedlist_add_subscription (
				subscription_get_source (tempSubscription),
				subscription_get_filter (tempSubscription),
				update_options_copy (tempSubscription->updateOptions),
				FEED_REQ_PRIORITY_HIGH
		);
	}

	subscription_free (tempSubscription);

	gtk_widget_destroy (GTK_WIDGET (dialog));
}

void
feed_list_view_add_duplicate_url_subscription (subscriptionPtr tempSubscription, nodePtr exNode)
{
	GtkWidget	*dialog;
	GtkWindow	*mainwindow;
	gchar		*text;

	text = g_strdup_printf (
			_("Are you sure that you want to add a new subscription with URL \"%s\"? Another subscription with the same URL already exists (\"%s\")."),
			tempSubscription->source,
			node_get_title (exNode)
	);

	mainwindow = GTK_WINDOW (liferea_shell_get_window ());
	dialog = gtk_message_dialog_new (mainwindow,
									 GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL,
									 GTK_MESSAGE_QUESTION,
									 GTK_BUTTONS_NONE,
									 "%s", text);
	gtk_dialog_add_buttons (GTK_DIALOG (dialog),
							_("_Cancel"), GTK_RESPONSE_CANCEL,
							_("_Add"), GTK_RESPONSE_ACCEPT,
							NULL);
	gtk_window_set_title (GTK_WINDOW (dialog), _("Adding Duplicate Subscription Confirmation"));
	gtk_window_set_transient_for (GTK_WINDOW (dialog), mainwindow);

	g_free (text);

	gtk_widget_show_all (dialog);

	g_signal_connect (G_OBJECT (dialog), "response",
					  G_CALLBACK (feed_list_view_add_duplicate_url_cb), tempSubscription);
}
