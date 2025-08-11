/**
 * @file feed_list_view.c  the feed list in a GtkTreeView
 *
 * Copyright (C) 2004-2024 Lars Windolf <lars.windolf@gmx.de>
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
#include "feedlist.h"
#include "net_monitor.h"
#include "node_provider.h"
#include "node_providers/feed.h"
#include "node_providers/folder.h"
#include "node_providers/newsbin.h"
#include "node_providers/vfolder.h"
#include "node_source.h"
#include "ui/icons.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/subscription_dialog.h"
#include "ui/ui_dnd.h"

struct _FeedListView {
	GObject			parentInstance;

	GtkEventController	*controller;
	GtkGesture		*popup_gesture;
	GtkGesture		*middle_gesture;
	GtkTreeView		*treeview;
	GtkTreeModel		*filter;
	GtkTreeStore		*feedstore;

	GHashTable		*flIterHash;				/**< hash table used for fast node id <-> tree iter lookup */

	gboolean		feedlist_reduced_unread;	/**< TRUE when feed list is in reduced mode (no folders, only unread feeds) */
};

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static FeedListView *flv = NULL;	// singleton

static guint feed_list_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (FeedListView, feed_list_view, G_TYPE_OBJECT);

static void feed_list_view_reload_feedlist ();
static void feed_list_view_select (Node *node);

static void
feed_list_view_finalize (GObject *object)
{
	FeedListView *flv = FEED_LIST_VIEW (object);

	g_object_unref (flv->controller);
	g_object_unref (flv->popup_gesture);
	g_object_unref (flv->middle_gesture);
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
	f->flIterHash = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);
}

// Handling feed list nodes

GtkTreeIter *
feed_list_view_to_iter (const gchar *nodeId)
{
	return (GtkTreeIter *)g_hash_table_lookup (flv->flIterHash, (gpointer)nodeId);
}

void
feed_list_view_update_iter (const gchar *nodeId, GtkTreeIter *iter)
{
	GtkTreeIter *old = (GtkTreeIter *)g_hash_table_lookup (flv->flIterHash, (gpointer)nodeId);
	if (old)
		*old = *iter;
}

static void
feed_list_view_add_iter (const gchar *nodeId, GtkTreeIter *iter)
{
	g_hash_table_insert (flv->flIterHash, (gpointer)nodeId, (gpointer)iter);
}

/* Folder expansion workaround using "empty" nodes */

static void
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

static void
feed_list_view_remove_empty_node (GtkTreeIter *parent)
{
	GtkTreeIter	iter;
	Node		*node;
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

static void
feed_list_view_set_expansion (Node *folder, gboolean expanded)
{
	GtkTreeIter		*iter;
	GtkTreePath		*path;

	if (flv->feedlist_reduced_unread)
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


static void
feed_list_view_row_changed_cb (GtkTreeModel *model, GtkTreePath *path, GtkTreeIter *iter)
{
	Node *node;

	gtk_tree_model_get (model, iter, FS_PTR, &node, -1);
	if (node)
		feed_list_view_update_iter (node->id, iter);
}

static void
feed_list_view_selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
	GtkTreeIter	iter;
	GtkTreeModel	*model;
	Node		*node;

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
	Node		*node;

	gtk_tree_model_get_iter (gtk_tree_view_get_model (tv), &iter, path);
	gtk_tree_model_get (gtk_tree_view_get_model (tv), &iter, FS_PTR, &node, -1);
	if (node && IS_FOLDER (node)) {
		if (gtk_tree_view_row_expanded (tv, path))
			gtk_tree_view_collapse_row (tv, path);
		else
			gtk_tree_view_expand_row (tv, path, FALSE);
	}

}

static GMenu *
feed_list_view_popup_menu (Node *node)
{
	GMenu		*menu_model = g_menu_new ();
	GMenu		*section = g_menu_new ();
	GMenuItem 	*menu_item;
	gboolean	writeableFeedlist, isRoot, addChildren, validSelection;

	if (node->parent) {
		writeableFeedlist = NODE_SOURCE_TYPE (node->parent->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST;
		isRoot = NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_IS_ROOT;
		addChildren = NODE_PROVIDER (node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS;
	} else {
		/* if we have no parent then we have the root node... */
		writeableFeedlist = TRUE;
		isRoot = TRUE;
		addChildren = TRUE;
	}
	validSelection = (node != NULL);

	if (validSelection) {
		if (NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_UPDATE)
			g_menu_append (section, _("_Update"), "app.node-update");
		else if (NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_UPDATE_CHILDS)
			g_menu_append (section, _("_Update Folder"), "app.node-update");
	}

	if (writeableFeedlist) {
		if (addChildren) {
			GMenu *submenu;

			submenu = g_menu_new ();

			if (node_can_add_child_feed (node))
				g_menu_append (submenu, _("New _Subscription..."), "app.new-subscription");

			if (node_can_add_child_folder (node))
				g_menu_append (submenu, _("New _Folder..."), "app.new-folder");

			if (isRoot) {
				g_menu_append (submenu, _("New S_earch Folder..."), "app.new-vfolder");
				g_menu_append (submenu, _("New S_ource..."), "app.new-source");
				g_menu_append (submenu, _("New _News Bin..."), "app.new-newsbin");
			}

			g_menu_append_submenu (section, _("_New"), G_MENU_MODEL (submenu));
			g_object_unref (submenu);
		}

		if (isRoot && node->children) {
			/* Ending section and starting a new one to get a separator : */
			g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
			g_object_unref (section);
			section = g_menu_new ();
			g_menu_append (section, _("Sort Feeds"), "app.node-sort-feeds");
		}
	}

	if (validSelection) {
		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
		section = g_menu_new ();
		g_menu_append (section, _("_Mark All As Read"), "app.node-mark-all-read");
		if (NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_EXPORT_ITEMS) {
			g_menu_append (section, _("_Export Items To File"), "app.node-export-items-to-file");
		}
	}

	if (IS_VFOLDER (node)) {
		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
		section = g_menu_new ();
		g_menu_append (section, _("_Rebuild"), "app.node-rebuild-vfolder");
	}

	if (validSelection) {
		if (writeableFeedlist) {
			g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
			g_object_unref (section);
			section = g_menu_new ();
			g_menu_append (section, _("_Delete"), "app.node-delete");
			g_menu_append (section, _("_Properties"), "app.node-properties");
		}

		if (IS_NODE_SOURCE (node) && NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL) {
			g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
			g_object_unref (section);
			section = g_menu_new ();
			g_menu_append (section, _("Convert To Local Subscriptions..."), "app.node-convert-to-local");
		}
	}

	g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	return menu_model;
}

static gboolean
feed_list_view_pressed_cb (GtkGestureClick *gesture, gdouble x, gdouble y, guint n_press, gpointer data)
{
	GtkTreeView *treeview = GTK_TREE_VIEW (flv->treeview);
	GtkTreePath *path;
	GtkTreeIter iter;
	Node *node;
g_print("Pressed at %f, %f with %d presses\n", x, y, n_press);
	if (n_press != 1)
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos (treeview, (int)x, (int)y, &path, NULL, NULL, NULL))
		return FALSE;

	if (!gtk_tree_model_get_iter (gtk_tree_view_get_model (treeview), &iter, path)) {
		gtk_tree_path_free (path);
		return FALSE;
	}

	gtk_tree_model_get (gtk_tree_view_get_model (treeview), &iter, FS_PTR, &node, -1);
	gtk_tree_path_free (path);
g_print("Pressed on node %s\n", node ? node_get_title(node) : "NULL");
	if (!node)
		return FALSE;

	if (n_press == 1) {
		switch (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture))) {
			case GDK_BUTTON_SECONDARY:
				/* Create a context menu */
				GMenu *menu = feed_list_view_popup_menu (node);
				GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL(menu));
				gtk_widget_set_parent (popover, GTK_WIDGET (treeview));
				GdkRectangle rect;
				rect.x = (int)x;
				rect.y = (int)y;
				rect.width = 1;
				rect.height = 1;
				gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
				gtk_popover_popup (GTK_POPOVER (popover));
				g_object_unref(menu);
				return TRUE;
			case GDK_BUTTON_MIDDLE:
				/* Middle mouse click toggles read status... */
				feedlist_mark_all_read (node);
				return TRUE;
		}
	}

	return FALSE;
}

static gboolean
feed_list_view_key_pressed_cb (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
	if (state & GDK_CONTROL_MASK) {
		if (keyval == GDK_KEY_Delete) {
			Node *node = feedlist_get_selected ();

			if (node) {
				if (state & GDK_SHIFT_MASK)
					feedlist_remove_node (node);
				else
					feed_list_view_remove (node);
				return TRUE;
			}
		}
	}

	return FALSE;
}

static gboolean
feed_list_view_filter_visible_function (GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
	gint			count;
	Node			*node;

	if (!flv->feedlist_reduced_unread)
		return TRUE;

	gtk_tree_model_get (model, iter, FS_PTR, &node, FS_UNREAD, &count, -1);
	if (!node)
		return FALSE;

	if (IS_NEWSBIN(node) && node->data && ((feedPtr)node->data)->alwaysShowInReduced)
		return TRUE;

	if (IS_FOLDER (node) || IS_NODE_SOURCE (node))
		return FALSE;

	if (IS_VFOLDER (node))
		return TRUE;

	/* Do not hide in any case if the node is selected, otherwise
	   the last unread item of a feed causes the feed to vanish
	   when clicking it */
	if (feedlist_get_selected () == node)
		return TRUE;

	if (count > 0)
		return TRUE;

	return FALSE;
}

static void
feed_list_view_expand (Node *node)
{
	if (node->parent)
		feed_list_view_expand (LIFEREA_NODE (node->parent));

	feed_list_view_set_expansion (node, TRUE);
}

static void
feed_list_view_restore_folder_expansion (Node *node)
{
	if (node->expanded)
		feed_list_view_expand (node);

	node_foreach_child (node, feed_list_view_restore_folder_expansion);
}

static void
feed_list_view_reduce_mode_changed (void)
{
	if (flv->feedlist_reduced_unread) {
		gtk_tree_view_set_reorderable (flv->treeview, FALSE);
		gtk_tree_view_set_model (flv->treeview, GTK_TREE_MODEL (flv->filter));
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (flv->filter));
	} else {
		gtk_tree_view_set_reorderable (flv->treeview, TRUE);
		gtk_tree_model_filter_refilter (GTK_TREE_MODEL_FILTER (flv->filter));
		gtk_tree_view_set_model (flv->treeview, GTK_TREE_MODEL (flv->feedstore));

		feedlist_foreach (feed_list_view_restore_folder_expansion);
	}
}

void
feed_list_view_set_reduce_mode (gboolean newReduceMode)
{
	flv->feedlist_reduced_unread = newReduceMode;
	conf_set_bool_value (REDUCED_FEEDLIST, flv->feedlist_reduced_unread);
	feed_list_view_reduce_mode_changed ();
	feed_list_view_reload_feedlist ();
}

static gint
feed_list_view_sort_folder_compare (gconstpointer a, gconstpointer b)
{
	Node *n1 = (Node *)a;
	Node *n2 = (Node *)b;

	gchar *s1 = g_utf8_casefold (n1->title, -1);
	gchar *s2 = g_utf8_casefold (n2->title, -1);

	gint result = strcmp (s1, s2);

	g_free (s1);
	g_free (s2);

	return result;
}

void
feed_list_view_sort_folder (Node *folder)
{
	GtkTreeView             *treeview;

	treeview = GTK_TREE_VIEW (liferea_shell_lookup ("feedlist"));
	/* Unset the model from the view before clearing it and rebuilding it.*/
	gtk_tree_view_set_model (treeview, NULL);
	folder->children = g_slist_sort (folder->children, feed_list_view_sort_folder_compare);
	feed_list_view_reload_feedlist ();
	/* Reduce mode didn't actually change but we need to set the
	 * correct model according to the setting in the same way : */
	feed_list_view_reduce_mode_changed ();
	feedlist_foreach (feed_list_view_restore_folder_expansion);
	feedlist_schedule_save ();
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

static void
feed_list_view_node_update (FeedListView *flv, Node *node)
{
	GtkTreeIter	*iter;
	gchar		*label, *count = NULL;
	guint		labeltype;
	static		gchar *countColor = NULL;

	/* Until GTK3 we used real theme colors here. Nowadays GTK simply knows
	   that we do not need to know about them and helpfully prevents us from
	   accessing them. So we use hard-coded colors that hopefully fit all the 
	   themes out there. 
	
	   And yes of course with to much time on my hand I could implement
	   my own renderer widget... */
	if (conf_get_dark_theme ())
		countColor = "foreground='#ddd' background='#444'";
	else
		countColor = "foreground='#fff' background='#aaa'";

	iter = feed_list_view_to_iter (node->id);
	if (!iter)
		return;

	labeltype = NODE_PROVIDER (node)->capabilities;
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
			count = g_strdup_printf ("<span weight='bold' %s> %u </span>", countColor, node->unreadCount);
			break;
		case NODE_CAPABILITY_SHOW_ITEM_COUNT:
			count = g_strdup_printf ("<span weight='bold' %s> %u </span>", countColor, node->itemCount);
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
		feed_list_view_node_update (flv, node->parent);
}

static void
feed_list_view_node_updated (GObject *obj, const gchar *nodeId, gpointer user_data)
{
	feed_list_view_node_update (FEED_LIST_VIEW (user_data), node_from_id (nodeId));
}

static void
feed_list_view_node_remove (FeedListView *flv, Node *node)
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
		feed_list_view_node_update (flv, node);
	}
}

static void
feed_list_view_node_removed (GObject *obj, gchar *nodeId, gpointer user_data)
{
	feed_list_view_node_remove (FEED_LIST_VIEW (user_data), node_from_id (nodeId));
}

static void
feed_list_view_node_add (FeedListView *flv, Node *node)
{
	gint		position;
	GtkTreeIter	*iter, *parentIter = NULL;

	debug (DEBUG_GUI, "adding node \"%s\" as child of parent=\"%s\"", node_get_title(node), (NULL != node->parent)?node_get_title(node->parent):"feed list root");

	g_assert (NULL != node->parent);
	g_assert (NULL == feed_list_view_to_iter (node->id));

	/* if parent is NULL we have the root folder and don't create a new row! */
	iter = g_new0 (GtkTreeIter, 1);

	/* if reduced feedlist, show flat treeview */
	if (flv->feedlist_reduced_unread)
		parentIter = NULL;
	else if (node->parent != feedlist_get_root ())
		parentIter = feed_list_view_to_iter (node->parent->id);

	position = g_slist_index (node->parent->children, node);

	if (flv->feedlist_reduced_unread || position < 0)
		gtk_tree_store_append (flv->feedstore, iter, parentIter);
	else
		gtk_tree_store_insert (flv->feedstore, iter, parentIter, position);

	gtk_tree_store_set (flv->feedstore, iter, FS_PTR, node, -1);
	feed_list_view_add_iter (node->id, iter);
	feed_list_view_node_update (flv, node);

	if (node->parent != feedlist_get_root ())
		feed_list_view_check_if_folder_is_empty (node->parent->id);

	if (IS_FOLDER (node))
		feed_list_view_check_if_folder_is_empty (node->id);
}

static void
feed_list_view_node_added (GObject *obj, gchar *nodeId, gpointer user_data)
{
	feed_list_view_node_add (FEED_LIST_VIEW (user_data), node_from_id (nodeId));
}

static void
feed_list_view_node_selected (GObject *obj, gchar *nodeId, gpointer user_data)
{
	feed_list_view_select (node_from_id (nodeId));
}

FeedListView *
feed_list_view_create (GtkTreeView *treeview, FeedList *feedlist)
{
	GtkCellRenderer		*titleRenderer, *countRenderer;
	GtkCellRenderer		*iconRenderer;
	GtkTreeViewColumn 	*column, *column2;
	GtkTreeSelection	*select;

	/* Set up store */
	g_assert (NULL == flv);
	flv = FEED_LIST_VIEW (g_object_new (FEED_LIST_VIEW_TYPE, NULL));

	flv->controller = gtk_event_controller_key_new ();
	flv->popup_gesture = gtk_gesture_click_new ();
	flv->middle_gesture = gtk_gesture_click_new ();
	flv->treeview = treeview;
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
	g_signal_connect (flv->controller, "key-pressed", G_CALLBACK (feed_list_view_key_pressed_cb), flv);
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (flv->middle_gesture), GDK_BUTTON_MIDDLE);
	g_signal_connect (flv->middle_gesture, "pressed", G_CALLBACK (feed_list_view_pressed_cb), flv);
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (flv->popup_gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect (flv->popup_gesture, "pressed", G_CALLBACK (feed_list_view_pressed_cb), flv);

	gtk_widget_add_controller (GTK_WIDGET (flv->treeview), flv->controller);
	gtk_widget_add_controller (GTK_WIDGET (flv->treeview), GTK_EVENT_CONTROLLER (flv->middle_gesture));
	gtk_widget_add_controller (GTK_WIDGET (flv->treeview), GTK_EVENT_CONTROLLER (flv->popup_gesture));

	select = gtk_tree_view_get_selection (flv->treeview);
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);

	g_signal_connect (G_OBJECT (select), "changed",
	                  G_CALLBACK (feed_list_view_selection_changed_cb),
                	  flv);

	conf_get_bool_value (REDUCED_FEEDLIST, &flv->feedlist_reduced_unread);
	if (flv->feedlist_reduced_unread)
		feed_list_view_reduce_mode_changed ();	/* before menu setup for reduced mode check box to be correct */

	ui_dnd_setup_feedlist (flv->feedstore);

	/* For performance prevent selection signals when filling the feed list
	   will be enabled when LifereaShell setup is finished */
	gtk_widget_set_sensitive (GTK_WIDGET (flv->treeview), FALSE);

	g_signal_connect (feedlist, "node-added", G_CALLBACK (feed_list_view_node_added), flv);
	g_signal_connect (feedlist, "node-removed", G_CALLBACK (feed_list_view_node_removed), flv);
	g_signal_connect (feedlist, "node-selected", G_CALLBACK (feed_list_view_node_selected), flv);
	g_signal_connect (feedlist, "node-updated", G_CALLBACK (feed_list_view_node_updated), flv);

	g_signal_connect (flv, "selection-changed", G_CALLBACK (feedlist_selection_changed), feedlist);

	return flv;
}

static void
feed_list_view_select (Node *node)
{
	GtkTreeModel *model = gtk_tree_view_get_model (flv->treeview);

	if (model && node && node != feedlist_get_root ()) {
		GtkTreePath *path = NULL;

		/* in filtered mode we need to convert the iterator */
		if (flv->feedlist_reduced_unread) {
			GtkTreeIter iter;
			gboolean valid = gtk_tree_model_filter_convert_child_iter_to_iter (GTK_TREE_MODEL_FILTER (flv->filter), &iter, feed_list_view_to_iter (node->id));
			if (valid)
				path = gtk_tree_model_get_path (model, &iter);
		} else {
			GtkTreeIter *iter = feed_list_view_to_iter (node->id);
			if (iter)
				path = gtk_tree_model_get_path (model, iter);
		}

		if (node->parent)
			feed_list_view_expand (LIFEREA_NODE (node->parent));

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

/* Expansion & Collapsing */

gboolean
feed_list_view_is_expanded (const gchar *nodeId)
{
	GtkTreeIter	*iter;
	gboolean 	expanded = FALSE;

	if (flv->feedlist_reduced_unread)
		return FALSE;

	iter = feed_list_view_to_iter (nodeId);
	if (iter) {
		GtkTreePath *path = gtk_tree_model_get_path (gtk_tree_view_get_model (flv->treeview), iter);
		expanded = gtk_tree_view_row_expanded (flv->treeview, path);
		gtk_tree_path_free (path);
	}

	return expanded;
}

static void
feed_list_view_load_feedlist (Node *node)
{
	GSList *iter = node->children;
	while (iter) {
		node = (Node *)iter->data;

		feed_list_view_node_added (NULL, node->id, flv);

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

static void
feed_list_view_reload_feedlist ()
{
	feed_list_view_clear_feedlist ();
	feed_list_view_load_feedlist (feedlist_get_root ());
}

/* node renaming dialog */

static void
on_nodenamedialog_response (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	Node	*node = (Node *)user_data;

	if (response_id == GTK_RESPONSE_OK) {
		node_set_title (node, liferea_dialog_entry_get(GTK_WIDGET (dialog), "nameentry"));

		feed_list_view_node_update (flv, node);
		feedlist_schedule_save ();
	}
}

void
feed_list_view_rename_node (Node *node)
{
	GtkWidget *dialog = liferea_dialog_new ("rename_node");

	liferea_dialog_entry_set (dialog, "nameentry", node_get_title (node));
	g_signal_connect (G_OBJECT (dialog), "response",
	                  G_CALLBACK (on_nodenamedialog_response), node);
	gtk_widget_show (dialog);
}

/* node deletion dialog */

static void
feed_list_view_remove_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	if (GTK_RESPONSE_ACCEPT == response_id)
		feedlist_remove_node ((Node *)user_data);
}

void
feed_list_view_remove (Node *node)
{
	GtkWidget	*dialog;
	GtkWindow	*mainwindow;
	g_autofree gchar	*text;

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

	g_signal_connect (G_OBJECT (dialog), "response",
	                  G_CALLBACK (feed_list_view_remove_cb), node);
}

static void
feed_list_view_add_duplicate_url_cb (GtkDialog *dialog, gint response_id, gpointer user_data)
{
	subscriptionPtr tempSubscription = (subscriptionPtr) user_data;

	if (response_id == GTK_RESPONSE_ACCEPT)
		feedlist_add_subscription (tempSubscription->source, NULL, NULL, 0);
	else
		subscription_free (tempSubscription);
}

void
feed_list_view_add_duplicate_url_subscription (subscriptionPtr tempSubscription, Node *exNode)
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

	g_signal_connect (G_OBJECT (dialog), "response",
					  G_CALLBACK (feed_list_view_add_duplicate_url_cb), tempSubscription);
}

void
feed_list_view_reparent (Node *node) {
	feed_list_view_node_remove (flv, node);
	feed_list_view_node_add (flv, node);
}