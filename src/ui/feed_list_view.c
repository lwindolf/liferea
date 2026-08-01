/**
 * @file feed_list_view.c  the feed list in a GtkListView
 *
 * Copyright (C) 2004-2026 Lars Windolf <lars.windolf@gmx.de>
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
#include "ui/browser_tabs.h"
#include "ui/icons.h"
#include "ui/ui_common.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"
#include "ui/subscription_dialog.h"
#include "ui/ui_dnd.h"

struct _FeedListView {
	GObject			parentInstance;

	GtkListView		*listview;
	FeedList		*feedlist;
	GListStore		*tree_root_model;
	GtkTreeListModel	*tree_model;
	GtkSingleSelection	*selection_model;
	GtkListItemFactory	*factory;

	gboolean		suppress_selection;
};

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static FeedListView *flv = NULL;
static guint feed_list_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (FeedListView, feed_list_view, G_TYPE_OBJECT);

typedef struct _FeedListNodeItem {
	GObject			parent_instance;
	Node			*node;
} FeedListNodeItem;

typedef struct _FeedListNodeItemClass {
	GObjectClass		parent_class;
} FeedListNodeItemClass;

G_DEFINE_TYPE (FeedListNodeItem, feed_list_node_item, G_TYPE_OBJECT);

static void
feed_list_node_item_class_init (FeedListNodeItemClass *klass)
{
	(void) klass;
}

static void
feed_list_node_item_init (FeedListNodeItem *self)
{
	self->node = NULL;
}

static FeedListNodeItem *
feed_list_node_item_new (Node *node)
{
	FeedListNodeItem *item;

	item = g_object_new (feed_list_node_item_get_type (), NULL);
	item->node = node;
	return item;
}

static gboolean
feed_list_view_is_expandable (Node *node)
{
	return IS_FOLDER (node) || IS_NODE_SOURCE (node);
}

static gboolean
feed_list_view_filter_visible_function (Node *node)
{
	(void) node;
	return TRUE;
}

static GListModel *
feed_list_view_create_child_model_cb (gpointer item, gpointer user_data)
{
	FeedListNodeItem *node_item = (FeedListNodeItem *)item;
	Node *node;
	GListStore *store;

	(void) user_data;

	if (!node_item)
		return NULL;

	node = node_item->node;
	if (!node || !feed_list_view_is_expandable (node))
		return NULL;

	store = g_list_store_new (feed_list_node_item_get_type ());
	for (GSList *iter = node->children; iter; iter = g_slist_next (iter)) {
		Node *child = (Node *)iter->data;

		if (!feed_list_view_filter_visible_function (child))
			continue;

		g_list_store_append (store, feed_list_node_item_new (child));
	}

	return G_LIST_MODEL (store);
}

static void
feed_list_view_rebuild_tree_model_cache (void)
{
	Node *root;

	if (!flv)
		return;

	g_clear_object (&flv->tree_model);
	g_clear_object (&flv->tree_root_model);

	flv->tree_root_model = g_list_store_new (feed_list_node_item_get_type ());
	root = feedlist_get_root ();
	if (root) {
		for (GSList *iter = root->children; iter; iter = g_slist_next (iter)) {
			Node *node = (Node *)iter->data;

			if (!feed_list_view_filter_visible_function (node))
				continue;

			g_list_store_append (flv->tree_root_model, feed_list_node_item_new (node));
		}
	}

	flv->tree_model = gtk_tree_list_model_new (G_LIST_MODEL (flv->tree_root_model),
	                                            FALSE,
	                                            FALSE,
	                                            feed_list_view_create_child_model_cb,
	                                            NULL,
	                                            NULL);
}

static void
feed_list_view_class_init (FeedListViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

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
	f->listview = NULL;
	f->feedlist = NULL;
	f->tree_root_model = NULL;
	f->tree_model = NULL;
	f->selection_model = NULL;
	f->factory = NULL;
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

static gchar *
feed_list_view_node_count_markup (Node *node)
{
	guint labeltype = NODE_PROVIDER (node)->capabilities;
	const gchar *count_color;

	labeltype &= (NODE_CAPABILITY_SHOW_UNREAD_COUNT |
	              NODE_CAPABILITY_SHOW_ITEM_COUNT);

	if (node->unreadCount == 0 && (labeltype & NODE_CAPABILITY_SHOW_UNREAD_COUNT))
		labeltype &= ~NODE_CAPABILITY_SHOW_UNREAD_COUNT;

	if (node->itemCount == 0 && (labeltype & NODE_CAPABILITY_SHOW_ITEM_COUNT))
		labeltype &= ~NODE_CAPABILITY_SHOW_ITEM_COUNT;

	if (IS_VFOLDER (node) && node->data && ((vfolderPtr)node->data)->totalCount)
		labeltype = NODE_CAPABILITY_SHOW_ITEM_COUNT;

	if (conf_get_dark_theme ())
		count_color = "foreground='#ddd' background='#444'";
	else
		count_color = "foreground='#fff' background='#aaa'";

	switch (labeltype) {
		case NODE_CAPABILITY_SHOW_UNREAD_COUNT | NODE_CAPABILITY_SHOW_ITEM_COUNT:
		case NODE_CAPABILITY_SHOW_UNREAD_COUNT:
			return g_strdup_printf ("<span weight='bold' %s> %u </span>", count_color, node->unreadCount);
		case NODE_CAPABILITY_SHOW_ITEM_COUNT:
			return g_strdup_printf ("<span weight='bold' %s> %u </span>", count_color, node->itemCount);
		default:
			return NULL;
	}
}

static gchar *
feed_list_view_node_label_markup (Node *node)
{
	gchar *label = g_markup_escape_text (node_get_title (node), -1);

	if (IS_VFOLDER (node) && node->data && ((vfolderPtr)node->data)->reloading) {
		gchar *tmp = label;
		label = g_strdup_printf (_("%s\n<i>Rebuilding</i>"), label);
		g_free (tmp);
	}

	return label;
}

static Node *
feed_list_view_tree_row_get_node (GtkTreeListRow *row)
{
	FeedListNodeItem *node_item;

	if (!row)
		return NULL;

	node_item = (FeedListNodeItem *)gtk_tree_list_row_get_item (row);
	if (!node_item)
		return NULL;

	return node_item->node;
}

static Node *
feed_list_view_get_current_view_selection (void)
{
	GtkTreeListRow *tree_row;
	Node *node;

	if (!flv || !flv->selection_model)
		return NULL;

	tree_row = GTK_TREE_LIST_ROW (gtk_single_selection_get_selected_item (flv->selection_model));
	if (!tree_row)
		return NULL;

	node = feed_list_view_tree_row_get_node (tree_row);
	g_object_unref (tree_row);
	return node;
}

static guint
feed_list_view_find_position_for_node (Node *node)
{
	guint n_items;

	if (!flv || !flv->tree_model || !node)
		return GTK_INVALID_LIST_POSITION;

	n_items = g_list_model_get_n_items (G_LIST_MODEL (flv->tree_model));
	for (guint i = 0; i < n_items; i++) {
		GtkTreeListRow *tree_row;
		Node *candidate;

		tree_row = GTK_TREE_LIST_ROW (g_list_model_get_item (G_LIST_MODEL (flv->tree_model), i));
		candidate = feed_list_view_tree_row_get_node (tree_row);
		g_object_unref (tree_row);

		if (candidate == node)
			return i;
	}

	return GTK_INVALID_LIST_POSITION;
}

static guint
feed_list_view_find_position_for_node_id (const gchar *nodeId)
{
	guint n_items;

	if (!flv || !flv->tree_model || !nodeId)
		return GTK_INVALID_LIST_POSITION;

	n_items = g_list_model_get_n_items (G_LIST_MODEL (flv->tree_model));
	for (guint i = 0; i < n_items; i++) {
		GtkTreeListRow *tree_row;
		Node *candidate;

		tree_row = GTK_TREE_LIST_ROW (g_list_model_get_item (G_LIST_MODEL (flv->tree_model), i));
		candidate = feed_list_view_tree_row_get_node (tree_row);
		g_object_unref (tree_row);

		if (candidate && candidate->id && g_str_equal (candidate->id, nodeId))
			return i;
	}

	return GTK_INVALID_LIST_POSITION;
}

static guint
feed_list_view_find_index_in_model (GListModel *model, Node *node)
{
	guint n_items;

	if (!model || !node)
		return GTK_INVALID_LIST_POSITION;

	n_items = g_list_model_get_n_items (model);
	for (guint i = 0; i < n_items; i++) {
		FeedListNodeItem *item = (FeedListNodeItem *)g_list_model_get_item (model, i);
		gboolean match = item && item->node == node;

		if (item)
			g_object_unref (item);
		if (match)
			return i;
	}

	return GTK_INVALID_LIST_POSITION;
}

static guint
feed_list_view_visible_sibling_index (Node *node)
{
	Node *parent;
	guint index = 0;

	if (!node || !node->parent)
		return GTK_INVALID_LIST_POSITION;

	parent = node->parent;
	for (GSList *iter = parent->children; iter; iter = g_slist_next (iter)) {
		Node *candidate = (Node *)iter->data;

		if (candidate == node)
			return index;
		if (feed_list_view_filter_visible_function (candidate))
			index++;
	}

	return GTK_INVALID_LIST_POSITION;
}

static GListStore *
feed_list_view_parent_store_for_node (Node *node)
{
	GtkTreeListRow *parent_row;
	GListModel *children;
	guint parent_position;

	if (!node || !node->parent)
		return NULL;

	if (node->parent == feedlist_get_root ())
		return flv->tree_root_model;

	parent_position = feed_list_view_find_position_for_node (node->parent);
	if (parent_position == GTK_INVALID_LIST_POSITION)
		return NULL;

	parent_row = GTK_TREE_LIST_ROW (g_list_model_get_item (G_LIST_MODEL (flv->tree_model), parent_position));
	if (!parent_row)
		return NULL;

	children = gtk_tree_list_row_get_children (parent_row);
	if (!children) {
		g_object_unref (parent_row);
		return NULL;
	}

	g_object_unref (parent_row);
	return G_LIST_STORE (children);
}

static gboolean
feed_list_view_insert_node_item (Node *node)
{
	GListStore *store;
	guint index;

	if (!node || !flv || !flv->tree_model)
		return FALSE;

	if (!feed_list_view_filter_visible_function (node))
		return TRUE;

	store = feed_list_view_parent_store_for_node (node);
	if (!store)
		return TRUE;

	index = feed_list_view_visible_sibling_index (node);
	if (index == GTK_INVALID_LIST_POSITION)
		index = g_list_model_get_n_items (G_LIST_MODEL (store));

	g_list_store_insert (store, index, feed_list_node_item_new (node));
	return TRUE;
}

static gboolean
feed_list_view_refresh_bound_row_widget (GtkWidget *widget, Node *node)
{
	for (GtkWidget *child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		if (feed_list_view_refresh_bound_row_widget (child, node))
			return TRUE;
	}

	if (g_object_get_data (G_OBJECT (widget), "node") != node)
		return FALSE;

	GtkWidget *icon = g_object_get_data (G_OBJECT (widget), "icon");
	GtkWidget *label = g_object_get_data (G_OBJECT (widget), "label");
	GtkWidget *count = g_object_get_data (G_OBJECT (widget), "count");
	gchar *label_markup = feed_list_view_node_label_markup (node);
	gchar *count_markup = feed_list_view_node_count_markup (node);

	if (icon)
		gtk_image_set_from_gicon (GTK_IMAGE (icon), (GIcon *)(node->available ? node_get_icon (node) : icon_get (ICON_UNAVAILABLE)));
	if (label)
		gtk_label_set_markup (GTK_LABEL (label), label_markup);
	if (count)
		gtk_label_set_markup (GTK_LABEL (count), count_markup ? count_markup : "");

	g_free (label_markup);
	g_free (count_markup);
	return TRUE;
}

static gboolean
feed_list_view_refresh_node_item (Node *node)
{
	guint position;

	if (!node || !flv || !flv->tree_model)
		return FALSE;

	position = feed_list_view_find_position_for_node (node);
	if (position == GTK_INVALID_LIST_POSITION)
		return feed_list_view_insert_node_item (node);

	feed_list_view_refresh_bound_row_widget (GTK_WIDGET (flv->listview), node);
	if (feed_list_view_is_expandable (node) && node->expanded) {
		GtkTreeListRow *row = GTK_TREE_LIST_ROW (g_list_model_get_item (G_LIST_MODEL (flv->tree_model), position));
		if (row) {
			gtk_tree_list_row_set_expanded (row, TRUE);
			g_object_unref (row);
		}
	}

	return TRUE;
}
static gboolean
feed_list_view_remove_node_item (const gchar *nodeId)
{
	GtkTreeListRow *row;
	GtkTreeListRow *parent_row;
	Node *node;
	GListModel *model;
	GListStore *store;
	guint position;
	guint index;

	if (!nodeId || !flv || !flv->tree_model)
		return FALSE;

	position = feed_list_view_find_position_for_node_id (nodeId);
	if (position == GTK_INVALID_LIST_POSITION)
		return TRUE;

	row = GTK_TREE_LIST_ROW (g_list_model_get_item (G_LIST_MODEL (flv->tree_model), position));
	if (!row)
		return FALSE;

	node = feed_list_view_tree_row_get_node (row);
	parent_row = gtk_tree_list_row_get_parent (row);
	model = parent_row ? gtk_tree_list_row_get_children (parent_row) : G_LIST_MODEL (flv->tree_root_model);
	if (!model || !node) {
		g_object_unref (row);
		return FALSE;
	}

	store = G_LIST_STORE (model);
	index = feed_list_view_find_index_in_model (model, node);
	if (index == GTK_INVALID_LIST_POSITION) {
		g_object_unref (row);
		return FALSE;
	}

	g_list_store_remove (store, index);
	g_object_unref (row);
	return TRUE;
}

static Node *
feed_list_view_find_node_at_coords (gdouble x, gdouble y)
{
	GtkWidget *picked;

	picked = gtk_widget_pick (GTK_WIDGET (flv->listview), x, y, GTK_PICK_DEFAULT);
	while (picked) {
		Node *node = g_object_get_data (G_OBJECT (picked), "node");

		if (node)
			return node;

		picked = gtk_widget_get_parent (picked);
	}

	return NULL;
}

static void
feed_list_view_tree_row_expanded_cb (GtkTreeListRow *tree_row, GParamSpec *pspec, gpointer user_data)
{
	Node *node = (Node *)user_data;

	(void) pspec;

	if (!node)
		return;

	node->expanded = gtk_tree_list_row_get_expanded (tree_row);
}

static void
feed_list_view_factory_setup_cb (GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *expander;
	GtkWidget *box;
	GtkWidget *icon;
	GtkWidget *label;
	GtkWidget *count;

	(void) factory;
	(void) user_data;

	expander = gtk_tree_expander_new ();
	box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	icon = gtk_image_new ();
	label = gtk_label_new (NULL);
	count = gtk_label_new (NULL);

	gtk_widget_set_margin_start (box, 6);
	gtk_widget_set_margin_end (box, 6);
	gtk_widget_set_margin_top (box, 0);
	gtk_widget_set_margin_bottom (box, 0);

	gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand (label, TRUE);
	gtk_label_set_xalign (GTK_LABEL (count), 1.0f);

	gtk_box_append (GTK_BOX (box), icon);
	gtk_box_append (GTK_BOX (box), label);
	gtk_box_append (GTK_BOX (box), count);
	gtk_tree_expander_set_child (GTK_TREE_EXPANDER (expander), box);
	gtk_list_item_set_child (list_item, expander);

	g_object_set_data (G_OBJECT (expander), "icon", icon);
	g_object_set_data (G_OBJECT (expander), "label", label);
	g_object_set_data (G_OBJECT (expander), "count", count);

	ui_dnd_setup_feedlist_row (expander);
}

static void
feed_list_view_factory_bind_cb (GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *expander;
	GtkTreeListRow *tree_row;
	GtkWidget *icon;
	GtkWidget *label;
	GtkWidget *count;
	Node *node;
	gchar *label_markup;
	gchar *count_markup;
	gulong expanded_handler;

	(void) factory;
	(void) user_data;

	expander = gtk_list_item_get_child (list_item);
	tree_row = GTK_TREE_LIST_ROW (gtk_list_item_get_item (list_item));
	icon = g_object_get_data (G_OBJECT (expander), "icon");
	label = g_object_get_data (G_OBJECT (expander), "label");
	count = g_object_get_data (G_OBJECT (expander), "count");
	node = feed_list_view_tree_row_get_node (tree_row);

	if (!node)
		return;

	gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (expander), tree_row);

	label_markup = feed_list_view_node_label_markup (node);
	count_markup = feed_list_view_node_count_markup (node);

	gtk_image_set_from_gicon (GTK_IMAGE (icon), (GIcon *)(node->available ? node_get_icon (node) : icon_get (ICON_UNAVAILABLE)));
	gtk_label_set_markup (GTK_LABEL (label), label_markup);
	gtk_label_set_markup (GTK_LABEL (count), count_markup ? count_markup : "");

	g_object_set_data (G_OBJECT (expander), "node", node);
	expanded_handler = g_signal_connect (tree_row,
	                                    "notify::expanded",
	                                    G_CALLBACK (feed_list_view_tree_row_expanded_cb),
	                                    node);
	g_object_set_data (G_OBJECT (expander), "expanded-row", g_object_ref (tree_row));
	g_object_set_data (G_OBJECT (expander), "expanded-handler", GSIZE_TO_POINTER ((gsize)expanded_handler));

	g_free (label_markup);
	g_free (count_markup);
}

static void
feed_list_view_factory_unbind_cb (GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *expander;
	GtkTreeListRow *expanded_row;
	gulong expanded_handler;

	(void) factory;
	(void) user_data;

	expander = gtk_list_item_get_child (list_item);
	expanded_row = g_object_get_data (G_OBJECT (expander), "expanded-row");
	expanded_handler = (gulong)GPOINTER_TO_SIZE (g_object_get_data (G_OBJECT (expander), "expanded-handler"));

	if (expanded_row && expanded_handler)
		g_signal_handler_disconnect (expanded_row, expanded_handler);
	if (expanded_row)
		g_object_unref (expanded_row);

	g_object_set_data (G_OBJECT (expander), "expanded-row", NULL);
	g_object_set_data (G_OBJECT (expander), "expanded-handler", NULL);
	g_object_set_data (G_OBJECT (expander), "node", NULL);
	gtk_tree_expander_set_list_row (GTK_TREE_EXPANDER (expander), NULL);
}

static void
feed_list_view_expand_path (Node *node)
{
	GPtrArray *path;

	if (!node)
		return;

	path = g_ptr_array_new ();
	for (Node *iter = node; iter && iter->parent; iter = iter->parent)
		g_ptr_array_add (path, iter);

	for (gint i = (gint)path->len - 1; i >= 0; i--) {
		Node *path_node = g_ptr_array_index (path, i);
		guint position;
		GtkTreeListRow *row;

		if (!feed_list_view_is_expandable (path_node))
			continue;

		path_node->expanded = TRUE;
		position = feed_list_view_find_position_for_node (path_node);
		if (position == GTK_INVALID_LIST_POSITION)
			continue;

		row = GTK_TREE_LIST_ROW (g_list_model_get_item (G_LIST_MODEL (flv->tree_model), position));
		if (!row)
			continue;

		gtk_tree_list_row_set_expanded (row, TRUE);
		g_object_unref (row);
	}

	g_ptr_array_free (path, TRUE);
}

static void
feed_list_view_selection_changed_cb (GtkSingleSelection *selection_model, GParamSpec *pspec, gpointer data)
{
	GtkTreeListRow *tree_row;
	Node *node;

	(void) pspec;
	(void) data;

	if (flv->suppress_selection)
		return;

	tree_row = GTK_TREE_LIST_ROW (gtk_single_selection_get_selected_item (selection_model));
	if (!tree_row)
		return;

	node = feed_list_view_tree_row_get_node (tree_row);
	if (!node)
		return;

	debug (DEBUG_GUI, "feed list selection changed to \"%s\"", node_get_title (node));

	feedlist_set_selected (node);
	browser_tabs_show_headlines ();
}

static void
feed_list_view_row_activated_cb (GtkListView *listview, guint position, gpointer data)
{
	GtkTreeListRow *row;
	Node *node;

	(void) listview;
	(void) data;

	row = GTK_TREE_LIST_ROW (g_list_model_get_item (G_LIST_MODEL (flv->tree_model), position));
	if (!row)
		return;

	node = feed_list_view_tree_row_get_node (row);

	if (node && feed_list_view_is_expandable (node)) {
		gboolean expanded = !gtk_tree_list_row_get_expanded (row);

		node->expanded = expanded;
		gtk_tree_list_row_set_expanded (row, expanded);
	}

	g_object_unref (row);
}

static GMenu *
feed_list_view_popup_menu (Node *node)
{
	GMenu		*menu_model = g_menu_new ();
	GMenu		*section = g_menu_new ();
	gboolean	writeableFeedlist, isRootSource, canAddChildren, validSelection;

	validSelection = (node != NULL);
	if (!node)
		node = feedlist_get_root ();

	if (node->parent)
		// FIXME: why do we check the parent's source and not the nodes own source?
		writeableFeedlist = NODE_SOURCE_TYPE (node->parent->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST;
	else
		writeableFeedlist = TRUE; // because root node

	isRootSource = NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_IS_ROOT;
	canAddChildren = NODE_PROVIDER (node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS;

	if (validSelection) {
		if (NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_UPDATE)
			g_menu_append (section, _("_Update"), "app.node-update");
		else if (NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_UPDATE_CHILDS)
			g_menu_append (section, _("_Update Folder"), "app.node-update");
	}

	if (writeableFeedlist) {
		if (canAddChildren) {
			GMenu *submenu;

			submenu = g_menu_new ();
			g_menu_append (submenu, _("New _Subscription..."), "app.new-subscription");
			g_menu_append (submenu, _("New _Folder..."), "app.new-folder");
			g_menu_append (submenu, _("New S_earch Folder..."), "app.new-vfolder");
			g_menu_append (submenu, _("New S_ource..."), "app.new-source");
			g_menu_append (submenu, _("New _News Bin..."), "app.new-newsbin");
			g_menu_append_submenu (section, _("_New"), G_MENU_MODEL (submenu));
			g_object_unref (submenu);
		}

		if (isRootSource && node->children) {
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
feed_list_view_pressed_cb (GtkGestureClick *gesture, gint n_press, gdouble x, gdouble y, gpointer data)
{
	Node *node;

	(void) data;
	if (n_press != 1)
		return FALSE;

	node = feed_list_view_find_node_at_coords (x, y);

	switch (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture))) {
		case GDK_BUTTON_PRIMARY:
			/* GtkTreeExpander handles primary-button expansion itself. */
			break;
		case GDK_BUTTON_SECONDARY: {
			GMenu *menu = feed_list_view_popup_menu (node);
			GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
			GtkWidget *anchor = gtk_widget_get_parent (GTK_WIDGET (flv->listview));
			GdkRectangle rect;
			graphene_point_t src = GRAPHENE_POINT_INIT ((float)x, (float)y);
			graphene_point_t dst;

			if (!anchor)
				anchor = GTK_WIDGET (flv->listview);

			gtk_widget_set_parent (popover, anchor);

			if ((anchor != GTK_WIDGET (flv->listview)) &&
			    gtk_widget_compute_point (GTK_WIDGET (flv->listview), anchor, &src, &dst)) {
				rect.x = (gint)dst.x;
				rect.y = (gint)dst.y;
			} else {
				rect.x = (gint)x;
				rect.y = (gint)y;
			}
			rect.width = 1;
			rect.height = 1;
			gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
			gtk_popover_popup (GTK_POPOVER (popover));
			g_object_unref (menu);
			return TRUE;
		}
		case GDK_BUTTON_MIDDLE:
			if (node) {
				/* Middle mouse click toggles read status (but do not select)... */
				gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
				g_action_group_activate_action (G_ACTION_GROUP (g_application_get_default ()), "mark-feed-as-read", g_variant_new_string (node->id));
				return TRUE;
			}
			break;
	}

	return FALSE;
}

static gboolean
feed_list_view_key_pressed_cb (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer data)
{
	guint default_modifiers = gtk_accelerator_get_default_mod_mask ();

	if ((keyval == GDK_KEY_Delete) ||
		(keyval == GDK_KEY_KP_Delete)) {
		Node *node = feedlist_get_selected ();

		if (!node)
			return FALSE;

		if (0 == (state & default_modifiers)) {
			feed_list_view_remove (node);
			return TRUE;
		} else if (state & GDK_SHIFT_MASK) {
			feedlist_remove_node (node);
			return TRUE;
		}
	}

	return FALSE;
}
static void
feed_list_view_node_changed (GObject *obj, gchar *nodeId, gpointer user_data)
{
	Node *node;

	(void) obj;
	(void) user_data;

	node = node_from_id (nodeId);
	if (node)
		feed_list_view_insert_node_item (node);
}

static void
feed_list_view_node_removed (GObject *obj, gchar *nodeId, gpointer user_data)
{
	(void) obj;
	(void) user_data;

	feed_list_view_remove_node_item (nodeId);
}

static void
feed_list_view_node_updated (GObject *obj, const gchar *nodeId, gpointer user_data)
{
	Node *node;

	(void) obj;
	(void) user_data;

	node = node_from_id (nodeId);
	if (!node)
		return;

	feed_list_view_refresh_node_item (node);
}

static void
feed_list_view_select (Node *node);

static void
feed_list_view_node_selected (GObject *obj, gchar *nodeId, gpointer user_data)
{
	Node *target;

	(void) obj;
	(void) user_data;

	if (!flv || flv->suppress_selection)
		return;

	target = node_from_id (nodeId);
	if (target == feed_list_view_get_current_view_selection ())
		return;

	feed_list_view_select (target);
}

FeedListView *
feed_list_view_create (GtkListView *listview, FeedList *feedlist)
{
	GtkEventController *controller;
	GtkGesture *primary_gesture;
	GtkGesture *popup_gesture;
	GtkGesture *middle_gesture;
	Node *selected;

	g_assert (NULL == flv);
	flv = FEED_LIST_VIEW (g_object_new (FEED_LIST_VIEW_TYPE, NULL));
	flv->listview = listview;
	flv->feedlist = feedlist;
	flv->selection_model = gtk_single_selection_new (NULL);
	flv->factory = gtk_signal_list_item_factory_new ();

	g_signal_connect (flv->factory, "setup", G_CALLBACK (feed_list_view_factory_setup_cb), flv);
	g_signal_connect (flv->factory, "bind", G_CALLBACK (feed_list_view_factory_bind_cb), flv);
	g_signal_connect (flv->factory, "unbind", G_CALLBACK (feed_list_view_factory_unbind_cb), flv);

	gtk_list_view_set_model (flv->listview, GTK_SELECTION_MODEL (flv->selection_model));
	gtk_list_view_set_factory (flv->listview, GTK_LIST_ITEM_FACTORY (flv->factory));
	gtk_list_view_set_single_click_activate (flv->listview, FALSE);

	controller = gtk_event_controller_key_new ();
	primary_gesture = gtk_gesture_click_new ();
	popup_gesture = gtk_gesture_click_new ();
	middle_gesture = gtk_gesture_click_new ();

	g_signal_connect (G_OBJECT (flv->listview), "activate", G_CALLBACK (feed_list_view_row_activated_cb), flv);
	g_signal_connect (flv->selection_model, "notify::selected-item", G_CALLBACK (feed_list_view_selection_changed_cb), flv);
	g_signal_connect (controller, "key-pressed", G_CALLBACK (feed_list_view_key_pressed_cb), flv);

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (primary_gesture), GDK_BUTTON_PRIMARY);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (primary_gesture), GTK_PHASE_CAPTURE);
	g_signal_connect (primary_gesture, "pressed", G_CALLBACK (feed_list_view_pressed_cb), flv);

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (middle_gesture), GDK_BUTTON_MIDDLE);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (middle_gesture), GTK_PHASE_CAPTURE);
	g_signal_connect (middle_gesture, "pressed", G_CALLBACK (feed_list_view_pressed_cb), flv);

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (popup_gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect (popup_gesture, "pressed", G_CALLBACK (feed_list_view_pressed_cb), flv);

	gtk_widget_add_controller (GTK_WIDGET (flv->listview), controller);
	gtk_widget_add_controller (GTK_WIDGET (flv->listview), GTK_EVENT_CONTROLLER (primary_gesture));
	gtk_widget_add_controller (GTK_WIDGET (flv->listview), GTK_EVENT_CONTROLLER (middle_gesture));
	gtk_widget_add_controller (GTK_WIDGET (flv->listview), GTK_EVENT_CONTROLLER (popup_gesture));

	/* Keep URL drops enabled; row reordering DnD is no longer tree-model based. */
	ui_dnd_setup_feedlist (GTK_WIDGET (flv->listview));

	/* For performance prevent selection signals when filling the feed list
	   will be enabled when LifereaShell setup is finished */
	gtk_widget_set_sensitive (GTK_WIDGET (flv->listview), FALSE);

	g_signal_connect (feedlist, "node-added", G_CALLBACK (feed_list_view_node_changed), flv);
	g_signal_connect (feedlist, "node-removed", G_CALLBACK (feed_list_view_node_removed), flv);
	g_signal_connect (feedlist, "node-updated", G_CALLBACK (feed_list_view_node_updated), flv);
	g_signal_connect (feedlist, "node-selected", G_CALLBACK (feed_list_view_node_selected), flv);

	selected = feedlist_get_selected ();
	flv->suppress_selection = TRUE;
	feed_list_view_rebuild_tree_model_cache ();
	if (flv->selection_model)
		gtk_single_selection_set_model (flv->selection_model, G_LIST_MODEL (flv->tree_model));
	flv->suppress_selection = FALSE;
	feed_list_view_select (selected);

	return flv;
}

static void
feed_list_view_select (Node *node)
{
	guint position;

	flv->suppress_selection = TRUE;

	if (node && node != feedlist_get_root ()) {
		if (node->parent)
			feed_list_view_expand_path (LIFEREA_NODE (node->parent));

		position = feed_list_view_find_position_for_node (node);
		if (position != GTK_INVALID_LIST_POSITION) {
			GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (flv->listview));
			GtkWidget *focus = root ? gtk_root_get_focus (root) : NULL;
			gboolean keep_feedlist_focus = focus &&
			    gtk_widget_is_ancestor (focus, GTK_WIDGET (flv->listview));

			gtk_single_selection_set_selected (flv->selection_model, position);

			/* Preserve feed list focus for feed-list navigation, but avoid stealing
			   focus from the item list during background node-updated refreshes. */
			if (keep_feedlist_focus)
				gtk_widget_grab_focus (GTK_WIDGET (flv->listview));
		}
	} else {
		gtk_single_selection_set_selected (flv->selection_model, GTK_INVALID_LIST_POSITION);
	}

	flv->suppress_selection = FALSE;
}

void
feed_list_view_move_cursor (FeedListView *view, gint step)
{
	guint selected;
	guint n_items;
	gint new_index;

	if (!view)
		return;

	n_items = g_list_model_get_n_items (G_LIST_MODEL (view->tree_model));
	if (n_items == 0)
		return;

	selected = gtk_single_selection_get_selected (view->selection_model);
	if (selected == GTK_INVALID_LIST_POSITION) {
		gtk_single_selection_set_selected (view->selection_model, 0);
		return;
	}

	new_index = (gint)selected + step;
	if (new_index < 0 || (guint)new_index >= n_items)
		return;

	gtk_single_selection_set_selected (view->selection_model, (guint)new_index);
}

/* Expansion & Collapsing */

gboolean
feed_list_view_is_expanded (const gchar *nodeId)
{
	Node *node;

	node = node_from_id (nodeId);
	return node ? node->expanded : FALSE;
}

void
feed_list_view_sort_folder (Node *folder)
{
	Node *selected;

	if (!folder)
		return;

	folder->children = g_slist_sort (folder->children, feed_list_view_sort_folder_compare);
	selected = feedlist_get_selected ();
	for (GSList *iter = folder->children; iter; iter = g_slist_next (iter)) {
		Node *child = (Node *)iter->data;

		g_signal_emit_by_name (flv->feedlist, "node-removed", child->id);
		g_signal_emit_by_name (flv->feedlist, "node-added", child->id);
	}
	feed_list_view_select (selected);

	feedlist_schedule_save ();
}

static void
on_nodenamedialog_response (GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog = GTK_WIDGET (user_data);
	Node *node = node_from_id (g_object_get_data (G_OBJECT (dialog), "node"));

	if (node) {
		node_set_title (node, liferea_dialog_entryrow_get (GTK_WIDGET (dialog), "nameEntry"));
		feed_list_view_refresh_node_item (node);
		feedlist_schedule_save ();
	}

	adw_dialog_close (ADW_DIALOG (dialog));
}

void
feed_list_view_rename_node (Node *node)
{
	GtkWidget *dialog = liferea_dialog_new ("rename_node");

	liferea_dialog_entryrow_set (dialog, "nameEntry", node_get_title (node));
	g_object_set_data (G_OBJECT (dialog), "node", node->id);
	g_signal_connect (liferea_dialog_lookup (dialog, "applyBtn"), "clicked", G_CALLBACK (on_nodenamedialog_response), dialog);
	g_signal_connect_swapped (liferea_dialog_lookup (dialog, "cancelBtn"), "clicked", G_CALLBACK (adw_dialog_close), dialog);
}

/* node deletion dialog */

static void
feed_list_view_confirm_remove_node_cb (gpointer user_data)
{
	feedlist_remove_node ((Node *)user_data);
}

void
feed_list_view_remove (Node *node)
{
	g_autofree gchar *text = NULL;

	g_assert (node == feedlist_get_selected ());

	text = g_strdup_printf (IS_FOLDER (node)?_("Are you sure that you want to delete \"%s\" and its contents?"):_("Are you sure that you want to delete \"%s\"?"), node_get_title (node));

	ui_confirm_box (
		_("Deletion Confirmation"),
		text,
		_("_Delete"),
		feed_list_view_confirm_remove_node_cb,
		NULL,
		node
	);
}
static void
feed_list_view_confirm_add_subscription_cb (gpointer user_data)
{
	feedlist_add_subscription ((subscriptionPtr)user_data);
}

static void
feed_list_view_confirm_free_subscription_cb (gpointer user_data)
{
	subscription_free ((subscriptionPtr)user_data);
}

void
feed_list_view_add_duplicate_url_subscription (subscriptionPtr tempSubscription, Node *exNode)
{
	g_autofree gchar *text = g_strdup_printf (
		_("Are you sure that you want to add a new subscription with URL \"%s\"? Another subscription with the same URL already exists (\"%s\")."),
		tempSubscription->source,
		node_get_title (exNode)
	);

	ui_confirm_box (
		_("Duplicate Subscription"),
		text,
		_("_Add"),
		feed_list_view_confirm_add_subscription_cb,
		feed_list_view_confirm_free_subscription_cb,
		tempSubscription
	);
}

void
feed_list_view_reparent (Node *node)
{
	Node *selected = feedlist_get_selected ();

	if (!node)
		return;

	g_signal_emit_by_name (flv->feedlist, "node-removed", node->id);
	g_signal_emit_by_name (flv->feedlist, "node-added", node->id);
	feed_list_view_select (selected);
}