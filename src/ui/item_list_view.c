/**
 * @file item_list_view.c presenting items in a GtkTreeView
 *  
 * Copyright (C) 2004-2009 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "ui/item_list_view.h"

#include <string.h>
#include <gdk/gdkkeysyms.h>

#include "browser.h"
#include "common.h"
#include "date.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "item.h"
#include "itemlist.h"
#include "itemview.h"
#include "newsbin.h"
#include "social.h"
#include "ui/browser_tabs.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"
#include "ui/ui_popup.h"

/**
 * Important performance considerations: Early versions had performance problems
 * with the item list loading because of the following two problems:
 *
 * 1.) Mass-adding items to a sorting enabled tree store.
 * 2.) Mass-loading items to an attached tree store.
 *
 * To avoid both problems we merge against a visible tree store only for single
 * items that are added/removed by background updates and load complete feeds or
 * collections of feeds only by adding items to a new unattached tree store.
 */

/** Enumeration of the columns in the itemstore. */
enum is_columns {
	IS_TIME,		/**< Time of item creation */
	IS_TIME_STR,		/**< Time of item creation as a string*/
	IS_LABEL,		/**< Displayed name */
	IS_STATEICON,		/**< Pixbuf reference to the item's state icon */
	IS_NR,			/**< Item id, to lookup item ptr from parent feed */
	IS_PARENT,		/**< Parent node pointer */
	IS_FAVICON,		/**< Pixbuf reference to the item's feed's icon */
	IS_ENCICON,		/**< Pixbuf reference to the item's enclosure icon */
	IS_ENCLOSURE,		/**< Flag wether enclosure is attached or not */
	IS_SOURCE,		/**< Source node pointer */
	IS_STATE,		/**< Original item state (unread, flagged...) for sorting */
	ITEMSTORE_UNREAD,	/**< Flag wether "unread" icon is to be shown */
	ITEMSTORE_LEN		/**< Number of columns in the itemstore */
};

static void item_list_view_class_init	(ItemListViewClass *klass);
static void item_list_view_init		(ItemListView *ilv);

#define ITEM_LIST_VIEW_GET_PRIVATE(object)(G_TYPE_INSTANCE_GET_PRIVATE ((object), ITEM_LIST_VIEW_TYPE, ItemListViewPrivate))

struct ItemListViewPrivate {
	GtkTreeView	*treeview;
	
	GHashTable 	*item_id_to_iter;	/**< hash table used for fast item id->tree iter lookup */

	gboolean	batch_mode;		/**< TRUE if we are in batch adding mode */
	GtkTreeStore	*batch_itemstore;	/**< GtkTreeStore prepared unattached and to be set on update() */
};

static GObjectClass *parent_class = NULL;

GType
item_list_view_get_type (void) 
{
	static GType type = 0;

	if (G_UNLIKELY (type == 0)) 
	{
		static const GTypeInfo our_info = 
		{
			sizeof (ItemListViewClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) item_list_view_class_init,
			NULL,
			NULL, /* class_data */
			sizeof (ItemListView),
			0, /* n_preallocs */
			(GInstanceInitFunc) item_list_view_init,
			NULL /* value_table */
		};

		type = g_type_register_static (G_TYPE_OBJECT,
					       "ItemListView",
					       &our_info, 0);
	}

	return type;
}

static void
item_list_view_finalize (GObject *object)
{
	ItemListViewPrivate *priv = ITEM_LIST_VIEW_GET_PRIVATE (object);

	if (priv->item_id_to_iter)
		g_hash_table_destroy (priv->item_id_to_iter);
	if (priv->batch_itemstore)
		g_object_unref (priv->batch_itemstore);

	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
item_list_view_class_init (ItemListViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	parent_class = g_type_class_peek_parent (klass);

	object_class->finalize = item_list_view_finalize;

	g_type_class_add_private (object_class, sizeof(ItemListViewPrivate));
}

/* helper functions for item <-> iter conversion */

gboolean
item_list_view_contains_id (ItemListView *ilv, gulong id)
{
	return (NULL != g_hash_table_lookup (ilv->priv->item_id_to_iter, GUINT_TO_POINTER (id)));
}

static gulong
item_list_view_iter_to_id (ItemListView *ilv, GtkTreeIter *iter)
{
	gulong	id = 0;
	
	gtk_tree_model_get (gtk_tree_view_get_model (ilv->priv->treeview), iter, IS_NR, &id, -1);
	return id;
}

static gboolean
item_list_view_id_to_iter (ItemListView *ilv, gulong id, GtkTreeIter *iter)
{
	GtkTreeIter *old_iter;

	old_iter = g_hash_table_lookup (ilv->priv->item_id_to_iter, GUINT_TO_POINTER (id));
	if (!old_iter)
		return FALSE;
	
	*iter = *old_iter;
	return TRUE;
}

static gint
item_list_view_date_sort_func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	guint64	timea, timeb;
	double	diff;

	gtk_tree_model_get (model, a, IS_TIME, &timea, -1);
	gtk_tree_model_get (model, b, IS_TIME, &timeb, -1);
	diff = difftime ((time_t)timeb, (time_t)timea);
	
	if (diff < 0)
		return 1;
		
	if (diff > 0)
		return -1;
	
	return 0;
}

static gint
item_list_view_favicon_sort_func (GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b, gpointer user_data)
{
	nodePtr	node1, node2;
	
	gtk_tree_model_get (model, a, IS_SOURCE, &node1, -1);
	gtk_tree_model_get (model, b, IS_SOURCE, &node2, -1);
	
	if (!node1->id || !node2->id)
		return 0;
		
	return strcmp (node1->id, node2->id);
}

void
item_list_view_set_sort_column (ItemListView *ilv, nodeViewSortType sortType, gboolean sortReversed)
{
	gint sortColumn;
	
	switch (sortType) {
		case NODE_VIEW_SORT_BY_TITLE:
			sortColumn = IS_LABEL;
			break;
		case NODE_VIEW_SORT_BY_PARENT:
			sortColumn = IS_PARENT;
			break;
		case NODE_VIEW_SORT_BY_TIME:
		default:
			sortColumn = IS_TIME;
			break;
	}
	
	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (gtk_tree_view_get_model (ilv->priv->treeview)),
	                                      sortColumn, 
	                                      sortReversed?GTK_SORT_DESCENDING:GTK_SORT_ASCENDING);
}

/**
 * Creates a GtkTreeStore to be filled with ui_itemlist_set_items
 * and to be set with ui_itemlist_set_tree_store().
 */
static GtkTreeStore *
item_list_view_create_tree_store (void)
{
	return gtk_tree_store_new (ITEMSTORE_LEN,
	                    G_TYPE_UINT64,	/* IS_TIME */
	                    G_TYPE_STRING, 	/* IS_TIME_STR */
	                    G_TYPE_STRING,	/* IS_LABEL */
	                    GDK_TYPE_PIXBUF,	/* IS_STATEICON */
	                    G_TYPE_ULONG,	/* IS_NR */
	                    G_TYPE_POINTER,	/* IS_PARENT */
	                    GDK_TYPE_PIXBUF,	/* IS_FAVICON */
	                    GDK_TYPE_PIXBUF,	/* IS_ENCICON */
	                    G_TYPE_BOOLEAN,	/* IS_ENCLOSURE */
	                    G_TYPE_POINTER,	/* IS_SOURCE */
	                    G_TYPE_UINT,	/* IS_STATE */
	                    G_TYPE_INT		/* ITEMSTORE_UNREAD */
	);
}

/**
 * Sets a GtkTreeView to the active GtkTreeView.
 */
static void
item_list_view_set_tree_store (ItemListView *ilv, GtkTreeStore *itemstore)
{
	GtkTreeModel    *model;

	/* drop old tree store */
	model = gtk_tree_view_get_model (ilv->priv->treeview);
	gtk_tree_view_set_model (ilv->priv->treeview, NULL);
	if (model)
		g_object_unref (model);
	
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (itemstore), IS_TIME, item_list_view_date_sort_func, NULL, NULL);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (itemstore), IS_SOURCE, item_list_view_favicon_sort_func, NULL, NULL);
	g_signal_connect (G_OBJECT (itemstore), "sort-column-changed", G_CALLBACK (itemlist_sort_column_changed_cb), NULL);
	
	gtk_tree_view_set_model (ilv->priv->treeview, GTK_TREE_MODEL (itemstore));

	item_list_view_prefocus (ilv);
}

void
item_list_view_remove_item (ItemListView *ilv, itemPtr item)
{
	GtkTreeIter	*iter;

	g_assert (NULL != item);
	iter = g_hash_table_lookup (ilv->priv->item_id_to_iter, GUINT_TO_POINTER (item->id));
	if (iter) {
		gtk_tree_store_remove (GTK_TREE_STORE (gtk_tree_view_get_model (ilv->priv->treeview)), iter);
		g_hash_table_remove (ilv->priv->item_id_to_iter, GUINT_TO_POINTER (item->id));
	} else {
		g_warning ("Fatal: item to be removed not found in iter lookup hash!");
	}
}

/* cleans up the item list, sets up the iter hash when called for the first time */
void
item_list_view_clear (ItemListView *ilv)
{
	GtkAdjustment		*adj;
	GtkTreeStore		*itemstore;

	itemstore = GTK_TREE_STORE (gtk_tree_view_get_model (ilv->priv->treeview));
	
	/* unselecting all items is important to remove items
	   whose removal is deferred until unselecting */
	gtk_tree_selection_unselect_all (gtk_tree_view_get_selection (ilv->priv->treeview));
	
	adj = gtk_tree_view_get_vadjustment (ilv->priv->treeview);
	gtk_adjustment_set_value (adj, 0.0);
	gtk_tree_view_set_vadjustment (ilv->priv->treeview, adj);

	if (itemstore)
		gtk_tree_store_clear (itemstore);
	if (ilv->priv->item_id_to_iter)
		g_hash_table_destroy (ilv->priv->item_id_to_iter);
	
	ilv->priv->item_id_to_iter = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
	
	/* enable batch mode for following item adds */
	ilv->priv->batch_mode = TRUE;
	ilv->priv->batch_itemstore = item_list_view_create_tree_store ();
}

void
item_list_view_update_item (ItemListView *ilv, itemPtr item)
{
	GtkTreeStore	*itemstore;
	GtkTreeIter	iter;
	gchar		*title, *time_str;
	const gchar 	*direction_marker;
	guint		state_icon;
	
	if (!item_list_view_id_to_iter (ilv, item->id, &iter))
		return;
	
	time_str = (0 != item->time) ? date_format ((time_t)item->time, NULL) : g_strdup ("");

	direction_marker = common_get_direction_mark (node_from_id (item->nodeId)->title);	

	title = item->title && strlen (item->title) ? item->title : _("*** No title ***");
	title = g_strstrip (g_strdup_printf ("%s%s", direction_marker, title));

	state_icon = item->flagStatus ? ICON_FLAG :
	             !item->readStatus ? ICON_UNREAD :
		     ICON_READ;

	if (ilv->priv->batch_mode)
		itemstore = ilv->priv->batch_itemstore;
	else
		itemstore = GTK_TREE_STORE (gtk_tree_view_get_model (ilv->priv->treeview));
		
	gtk_tree_store_set (itemstore,
	                    &iter,
		            IS_LABEL, title,
			    IS_TIME_STR, time_str,
			    IS_STATEICON, icons[state_icon],
			    ITEMSTORE_UNREAD, item->readStatus ? PANGO_WEIGHT_NORMAL : PANGO_WEIGHT_BOLD,
			    -1);

	g_free (time_str);
	g_free (title);
}

static void
item_list_view_update_item_foreach (gpointer key,
                                    gpointer value,
                                    gpointer user_data)
{
	itemPtr 	item;
	
	item = item_load (GPOINTER_TO_UINT (key) /* id */);
	if (!item)
		return;

	item_list_view_update_item (ITEM_LIST_VIEW (user_data), item);
	
	item_unload (item);
}

void 
item_list_view_update_all_items (ItemListView *ilv) 
{
	g_hash_table_foreach (ilv->priv->item_id_to_iter, item_list_view_update_item_foreach, (gpointer)ilv);
}

void
item_list_view_update (ItemListView *ilv, gboolean hasEnclosures)
{
	/* we depend on the fact that the third column is the enclosure icon column!!! */
	gtk_tree_view_column_set_visible (gtk_tree_view_get_column (ilv->priv->treeview, 1), hasEnclosures);

	if (ilv->priv->batch_mode) {
		item_list_view_set_tree_store (ilv, ilv->priv->batch_itemstore);
		ilv->priv->batch_mode = FALSE;
	} else {
		/* Nothing to do in non-batch mode as items were added
		   and updated one-by-one in ui_itemlist_add_item() */
	}
}

static gboolean
on_item_list_view_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data) 
{
	if ((event->type == GDK_KEY_PRESS) && (event->state == 0) && (event->keyval == GDK_Delete)) 
		on_remove_item_activate(NULL, NULL);

	return FALSE;
}

GtkTreeView *
item_list_view_get_widget (ItemListView *ilv)
{
	return ITEM_LIST_VIEW_GET_PRIVATE (ilv)->treeview;
}

static void
item_list_view_init (ItemListView *ilv)
{
	ilv->priv = ITEM_LIST_VIEW_GET_PRIVATE (ilv);
	ilv->priv->item_id_to_iter = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_free);
}

ItemListView *
item_list_view_create (GtkWidget *window) 
{
	ItemListView		*ilv;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column;
	GtkTreeSelection	*select;
	GtkWidget 		*ilscrolledwindow;

	ilv = g_object_new (ITEM_LIST_VIEW_TYPE, NULL);
		
	ilscrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	gtk_widget_show (ilscrolledwindow);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ilscrolledwindow), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (ilscrolledwindow), GTK_SHADOW_IN);

	ilv->priv->treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
	gtk_container_add (GTK_CONTAINER (ilscrolledwindow), GTK_WIDGET (ilv->priv->treeview));
	gtk_widget_show (GTK_WIDGET (ilv->priv->treeview));
	gtk_widget_set_name (GTK_WIDGET (ilv->priv->treeview), "itemlist");
	gtk_tree_view_set_rules_hint (ilv->priv->treeview, TRUE);
	
	g_object_set_data (G_OBJECT (window), "itemlist", ilv->priv->treeview);

	item_list_view_set_tree_store (ilv, item_list_view_create_tree_store ());

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "pixbuf", IS_STATEICON, NULL);
	gtk_tree_view_append_column (ilv->priv->treeview, column);
	gtk_tree_view_column_set_sort_column_id (column, IS_STATE);	
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "pixbuf", IS_ENCICON, NULL);
	gtk_tree_view_append_column (ilv->priv->treeview, column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Date"), renderer, 
	                                                   "text", IS_TIME_STR,
							   "weight", ITEMSTORE_UNREAD,
							   NULL);
	gtk_tree_view_append_column (ilv->priv->treeview, column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TIME);
	g_object_set (column, "resizable", TRUE, NULL);
	
	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "pixbuf", IS_FAVICON, NULL);
	gtk_tree_view_column_set_sort_column_id (column, IS_SOURCE);
	gtk_tree_view_append_column (ilv->priv->treeview, column);
	
	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Headline"), renderer, 
	                                                   "text", IS_LABEL,
							   "weight", ITEMSTORE_UNREAD,					  
							   NULL);
	gtk_tree_view_append_column (ilv->priv->treeview, column);
	gtk_tree_view_column_set_sort_column_id (column, IS_LABEL);
	g_object_set (column, "resizable", TRUE, NULL);
	g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

	/* And connect signals */	
	g_signal_connect (G_OBJECT (ilv->priv->treeview), "key-press-event", G_CALLBACK (on_item_list_view_key_press_event), NULL);
	
	/* Setup the selection handler */
	select = gtk_tree_view_get_selection (ilv->priv->treeview);
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (select), "changed",
	                  G_CALLBACK (on_itemlist_selection_changed), ilv);
	g_signal_connect ((gpointer)ilv->priv->treeview, "button_press_event",
	                  G_CALLBACK (on_itemlist_button_press_event), ilv);
	g_signal_connect ((gpointer)ilv->priv->treeview, "row_activated",
	                  G_CALLBACK (on_Itemlist_row_activated), ilv);
		  
	return ilv;
}

/* typically called when filling the item tree view */
void 
item_list_view_prefocus (ItemListView *ilv)
{
	GtkWidget		*focus_widget;
	GtkTreeSelection	*itemselection;
	
	/* the following is important to prevent setting the unread
	   flag for the first item in the item list when the user does
	   the first click into the treeview, if we don't do a focus and
	   unselect, GTK would always (exception: clicking on first item)
	   generate two selection-change events (one for the clicked and
	   one for the selected item)!!! */

	/* we need to restore the focus after we temporarily select the itemlist */
	focus_widget = gtk_window_get_focus (GTK_WINDOW (liferea_shell_get_window ()));

	/* prevent marking as unread before focussing, which leads to a selection */
	gtk_widget_grab_focus (GTK_WIDGET (ilv->priv->treeview));

	itemselection = gtk_tree_view_get_selection (ilv->priv->treeview);
	if (itemselection)
		gtk_tree_selection_unselect_all (itemselection);
	
	if (focus_widget)
		gtk_widget_grab_focus (focus_widget);
}

static void
item_list_view_add_item_to_tree_store (ItemListView *ilv, GtkTreeStore *itemstore, itemPtr item)
{
	gint		state = 0;
	nodePtr		node;
	GtkTreeIter	*iter;
	GtkTreeIter	old_iter;
	gboolean	exists;
		
	if (item->flagStatus)
		state += 2;
	if (!item->readStatus)
		state += 1;
		
	node = node_from_id (item->nodeId);
	if(!node)
		return;	/* comment items do cause this... maybe filtering them earlier would be a good idea... */
		
	exists = item_list_view_id_to_iter (ilv, item->id, &old_iter);
	iter = &old_iter;
	
	if (!exists) 
	{
		iter = g_new0 (GtkTreeIter, 1);
		gtk_tree_store_prepend (itemstore, iter, NULL);
		g_hash_table_insert (ilv->priv->item_id_to_iter, GUINT_TO_POINTER (item->id), (gpointer)iter);
	}

	gtk_tree_store_set (itemstore, iter,
		                       IS_TIME, (guint64)item->time,
		                       IS_NR, item->id,
				       IS_PARENT, node,
		                       IS_FAVICON, node->icon,
		                       IS_ENCICON, item->hasEnclosure?icons[ICON_ENCLOSURE]:NULL,
				       IS_ENCLOSURE, item->hasEnclosure,
				       IS_SOURCE, node,
				       IS_STATE, state,
		                       -1);		                       
}

void 
item_list_view_add_item (ItemListView *ilv, itemPtr item)
{
	GtkTreeStore	*itemstore;
	
	if (ilv->priv->batch_mode) {
		/* either merge to new unattached GtkTreeStore */
		item_list_view_add_item_to_tree_store (ilv, ilv->priv->batch_itemstore, item);
	} else {
		/* or merge to visible item store */	
		itemstore = GTK_TREE_STORE (gtk_tree_view_get_model (ilv->priv->treeview));
		item_list_view_add_item_to_tree_store (ilv, itemstore, item);
	}
	
	item_list_view_update_item (ilv, item);
}

void
item_list_view_enable_favicon_column (ItemListView *ilv, gboolean enabled)
{
	/* we depend on the fact that the second column is the favicon column!!! 
	   if we are in search mode or have a folder or vfolder we show the favicon 
	   column to give a hint where the item comes from ... */
	gtk_tree_view_column_set_visible (gtk_tree_view_get_column (ilv->priv->treeview, 3), enabled);
}

void
on_popup_launchitem_selected (void) 
{
	itemPtr		item;

	item = itemlist_get_selected ();
	if (item) {
		itemview_launch_URL (item_get_source (item), FALSE);
				       
		item_unload (item);
	} else {
		liferea_shell_set_important_status_bar (_("No item has been selected"));
	}
}

void
on_popup_launchitem_in_tab_selected (void) 
{
	itemPtr		item;
	const gchar	*link;

	item = itemlist_get_selected ();
	if (item) {
		link = item_get_source (item);
		if (link)
			browser_tabs_add_new (link, link, FALSE);
		else
			ui_show_error_box (_("This item has no link specified!"));
			
		item_unload (item);
	} else {
		liferea_shell_set_important_status_bar (_("No item has been selected"));
	}
}

void 
on_Itemlist_row_activated (GtkTreeView *treeview,
                           GtkTreePath *path,
			   GtkTreeViewColumn *column,
			   gpointer user_data) 
{

	on_popup_launchitem_selected ();
}

/* menu callbacks */

void
on_toggle_item_flag (GtkMenuItem *menuitem, gpointer user_data) 
{
	itemPtr		item;

	item = itemlist_get_selected ();
	if (item) {
		itemlist_toggle_flag (item);
		item_unload (item);
	}
}

void 
on_popup_toggle_flag (void)
{
	on_toggle_item_flag (NULL, NULL);
}

void 
on_toggle_unread_status (GtkMenuItem *menuitem, gpointer user_data) 
{
	itemPtr		item;

	item = itemlist_get_selected ();
	if (item) {
		itemlist_toggle_read_status (item);
		item_unload (item);
	}
}

void
on_popup_toggle_read (void)
{
	on_toggle_unread_status (NULL, NULL);
}

void
on_remove_items_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	nodePtr		node;
	
	node = feedlist_get_selected ();
	// FIXME: use node type capability check
	if (node && (IS_FEED (node) || IS_NEWSBIN (node)))
		itemlist_remove_all_items (node);
	else
		ui_show_error_box (_("You must select a feed to delete its items!"));
}

void
on_remove_item_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	itemPtr		item;
	
	item = itemlist_get_selected ();
	if (item) {
		ui_common_treeview_move_cursor (ITEM_LIST_VIEW (user_data)->priv->treeview, 1);
		itemlist_remove_item (item);
	} else {
		liferea_shell_set_important_status_bar (_("No item has been selected"));
	}
}

void
on_popup_remove_selected (void)
{
	on_remove_item_activate (NULL, NULL);
}

void
item_list_view_select (ItemListView *ilv, itemPtr item)
{
	GtkTreeView		*treeview = ilv->priv->treeview;
	GtkTreeSelection	*selection;
	
	selection = gtk_tree_view_get_selection (treeview);
	
	if (item) {
		GtkTreeIter		iter;
		GtkTreePath		*path;
		
		if (!item_list_view_id_to_iter(ilv, item->id, &iter))
			/* This is an evil hack to fix SF #1870052: crash
			   upon hitting <enter> when no headline selected.
			   FIXME: This code is rotten! Rewrite it! Now! */
			itemlist_selection_changed (NULL);

		path = gtk_tree_model_get_path (gtk_tree_view_get_model (treeview), &iter);
		gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.0, 0.0);
		gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
		gtk_tree_path_free (path);
	} else {
		gtk_tree_selection_unselect_all (selection);
	}
}

itemPtr
item_list_view_find_unread_item (ItemListView *ilv, gulong startId)
{
	GtkTreeIter		iter;
	GtkTreeModel		*model;
	gboolean		valid = TRUE;
	
	model = gtk_tree_view_get_model (ilv->priv->treeview);
	
	if (startId)
		valid = item_list_view_id_to_iter (ilv, startId, &iter);
	else
		valid = gtk_tree_model_get_iter_first (model, &iter);
	
	while (valid) {
		itemPtr	item = item_load (item_list_view_iter_to_id (ilv, &iter));
		if (item) {
			if (!item->readStatus)
				return item;
			item_unload (item);
		}
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return NULL;
}

void
on_next_unread_item_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	itemlist_select_next_unread ();
}

void
on_popup_next_unread_item_selected (gpointer callback_data, guint callback_action, GtkWidget *widget)
{
	itemlist_select_next_unread ();
}

void
on_nextbtn_clicked (GtkButton *button, gpointer user_data)
{
	itemlist_select_next_unread ();
}

gboolean
on_itemlist_button_press_event (GtkWidget *treeview, GdkEventButton *event, gpointer user_data)
{
	ItemListView		*ilv = ITEM_LIST_VIEW (user_data);
	GtkTreeViewColumn	*column;
	GtkTreePath		*path;
	GtkTreeIter		iter;
	itemPtr			item = NULL;
	gboolean		result = FALSE;
	
	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	/* avoid handling header clicks */
	if (event->window != gtk_tree_view_get_bin_window (ilv->priv->treeview))
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos (ilv->priv->treeview, (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL))
		return FALSE;

	if (gtk_tree_model_get_iter (gtk_tree_view_get_model (ilv->priv->treeview), &iter, path))
		item = item_load (item_list_view_iter_to_id (ilv, &iter));
		
	gtk_tree_path_free (path);
	
	if (item) {
		GdkEventButton *eb = (GdkEventButton*)event; 
		switch (eb->button) {
			case 1:
				column = gtk_tree_view_get_column (ilv->priv->treeview, 0);
				if (column) {
					/* Allow flag toggling when left clicking in the flagging column.
					   We depend on the fact that the state column is the first!!! */
					if (event->x <= column->width) {
						itemlist_toggle_flag (item);
						result = TRUE;
					}
				}
				break;
			case 2:
				/* Middle mouse click toggles read status... */
				itemlist_toggle_read_status (item);
				result = TRUE;
				break;
			case 3:
				item_list_view_select (ilv, item);
				ui_popup_item_menu (item, eb->button, eb->time);
				result = TRUE;
				break;
		}
		item_unload (item);
	}
		
	return result;
}

void
on_popup_copy_URL_clipboard (void)
{
	itemPtr         item;

	item = itemlist_get_selected ();
	if (item) {
		gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY), item_get_source (item), -1);
		gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), item_get_source (item), -1);
		item_unload (item);
	} else {
		liferea_shell_set_important_status_bar (_("No item has been selected"));
	}
}

void
ui_itemlist_search_item_link (itemPtr item)
{
	gchar *url = social_get_link_search_url (item_get_source (item));
	itemview_launch_URL (url, FALSE);
	g_free (url);
}

void
ui_itemlist_add_item_bookmark (itemPtr item)
{
	gchar *url = social_get_bookmark_url (item_get_source (item), item_get_title (item));
	(void)browser_launch_URL_external (url);
	g_free (url);
}

void
on_popup_social_bm_item_selected (void)
{
	itemPtr item;

	item = itemlist_get_selected ();
	if (item) {
		ui_itemlist_add_item_bookmark (item);
		item_unload (item);
	}
	else
		liferea_shell_set_important_status_bar (_("No item has been selected"));
}

void
on_itemlist_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeIter 	iter;
	GtkTreeModel	*model;
	itemPtr		item = NULL;

	if (gtk_tree_selection_get_selected (selection, &model, &iter))
		item = item_load (item_list_view_iter_to_id (ITEM_LIST_VIEW (user_data), &iter));

	liferea_shell_update_item_menu (NULL != item);
	if (item)
		itemlist_selection_changed (item);
}

void
itemlist_sort_column_changed_cb (GtkTreeSortable *treesortable, gpointer user_data)
{
	gint		sortColumn, nodeSort;
	GtkSortType	sortType;
	gboolean	sorted, changed;
	
	if (feedlist_get_selected () == NULL)
		return;
	
	sorted = gtk_tree_sortable_get_sort_column_id (treesortable, &sortColumn, &sortType);
	if (!sorted)
		return;
		
	switch (sortColumn) {
		case IS_TIME:
		default:
			nodeSort = NODE_VIEW_SORT_BY_TIME;
			break;
		case IS_LABEL:
			nodeSort = NODE_VIEW_SORT_BY_TITLE;
			break;
		case IS_PARENT:
		case IS_SOURCE:
			nodeSort = NODE_VIEW_SORT_BY_PARENT;
			break;
	}

	changed = node_set_sort_column (feedlist_get_selected (), nodeSort, sortType == GTK_SORT_DESCENDING);
	if (changed)
		feedlist_schedule_save ();
}

/* needed because switching does sometimes returns to the tree 
   view with a very disturbing horizontal scrolling state */
void
item_list_view_scroll_left (ItemListView *ilv)
{
	GtkTreeViewColumn 	*column;
	GtkTreePath		*path;

	if (2 != itemlist_get_view_mode ()) {
		gtk_tree_view_get_cursor (ilv->priv->treeview, &path, &column);
		if (path) {
			column = gtk_tree_view_get_column (ilv->priv->treeview, 1);
			gtk_tree_view_set_cursor (ilv->priv->treeview, path, column, FALSE);
			gtk_tree_path_free (path);
		}
	}
}
