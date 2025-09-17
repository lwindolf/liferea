/*
 * @file item_list_view.c  presenting items in a GtkTreeView
 *
 * Copyright (C) 2004-2025 Lars Windolf <lars.windolf@gmx.de>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <glib.h>
#include <gdk/gdkkeysyms.h>

#include "browser.h"
#include "common.h"
#include "conf.h"
#include "date.h"
#include "debug.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "item.h"
#include "itemlist.h"
#include "item_state.h"
#include "node_providers/newsbin.h"
#include "social.h"
#include "xml.h"
#include "ui/browser_tabs.h"
#include "ui/icons.h"
#include "ui/liferea_shell.h"
#include "ui/ui_common.h"

/*
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

/* Enumeration of the columns in the itemstore. */
enum is_columns {
	IS_TIME,		/*<< Time of item creation */
	IS_TIME_STR,		/*<< Time of item creation as a string*/
	IS_LABEL,		/*<< Displayed name */
	IS_STATEICON,		/*<< Pixbuf reference to the item's state icon */
	IS_NR,			/*<< Item id, to lookup item ptr from parent feed */
	IS_PARENT,		/*<< Parent node pointer */
	IS_FAVICON,		/*<< Pixbuf reference to the item's feed's icon */
	IS_SOURCE,		/*<< Source node pointer */
	IS_STATE,		/*<< Original item state (unread, flagged...) for sorting */
	ITEMSTORE_WEIGHT,	/*<< Flag whether weight is to be bold and "unread" icon is to be shown */
	ITEMSTORE_ALIGN,        /*<< How to align title (RTL support) */
	ITEMSTORE_LEN		/*<< Number of columns in the itemstore */
};

struct _ItemListView {
	GObject		parentInstance;

	GtkEventController *keypress;
	GtkGesture	*gesture;
	GtkGesture	*popup_gesture;
	GtkGesture	*middle_gesture;

	GtkTreeView	*treeview;
	GtkWidget 	*ilscrolledwindow;	/*<< The complete ItemListView widget */
	GSList		*item_ids;		/*<< list of all currently known item ids */

	gboolean	batch_mode;		/*<< TRUE if we are in batch adding mode */
	GtkTreeStore	*batch_itemstore;	/*<< GtkTreeStore prepared unattached and to be set on update() */

	GHashTable	*columns;               /*<< Named GtkTreeViewColumns */
	GtkCellRenderer *headlineRenderer;	/*<< Renderer for the headline column */

	gboolean	wideView;		/*<< TRUE for wide view mode */
};

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

enum {
	PROP_NONE,
	PROP_WIDE_VIEW
};

static guint item_list_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (ItemListView, item_list_view, G_TYPE_OBJECT);

static void
item_list_view_finalize (GObject *object)
{
	ItemListView *ilv = ITEM_LIST_VIEW (object);

	/* Disconnect the treeview signals to avoid spurious calls during teardown */
	g_signal_handlers_disconnect_by_data (G_OBJECT (ilv->treeview), object);

	g_hash_table_destroy (ilv->columns);

	g_slist_free (ilv->item_ids);
	if (ilv->batch_itemstore)
		g_object_unref (ilv->batch_itemstore);
	if (ilv->ilscrolledwindow)
		g_object_unref (ilv->ilscrolledwindow);

	g_object_unref (ilv->gesture);
	g_object_unref (ilv->popup_gesture);
	g_object_unref (ilv->middle_gesture);
}

static void
item_list_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
        ItemListView *ilv = ITEM_LIST_VIEW (object);

        switch (prop_id) {
	        case PROP_WIDE_VIEW:
			g_value_set_boolean (value, ilv->wideView);
			break;
	}
}

static void
item_list_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ItemListView *ilv = ITEM_LIST_VIEW (object);

	switch (prop_id) {
	        case PROP_WIDE_VIEW:
			ilv->wideView = g_value_get_boolean (value);

			GtkTreeViewColumn *state = g_hash_table_lookup (ilv->columns, "state");
			GtkTreeViewColumn *date = g_hash_table_lookup (ilv->columns, "date");
			GtkTreeViewColumn *favicon = g_hash_table_lookup (ilv->columns, "favicon");
			GtkTreeViewColumn *headline = g_hash_table_lookup (ilv->columns, "headline");

			gtk_tree_view_column_set_visible (state, !ilv->wideView);
			gtk_tree_view_column_set_visible (date, !ilv->wideView);
			gtk_tree_view_column_clear_attributes (headline, ilv->headlineRenderer);
			gtk_tree_view_column_add_attribute (headline, ilv->headlineRenderer, "markup", IS_LABEL);
			gtk_tree_view_column_add_attribute (headline, ilv->headlineRenderer, "xalign", ITEMSTORE_ALIGN);
			gtk_tree_view_column_add_attribute (headline, ilv->headlineRenderer, "weight", ITEMSTORE_WEIGHT);

			if (ilv->wideView) {				
				gtk_tree_view_set_grid_lines (ilv->treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
				gtk_tree_view_column_set_sizing (state, GTK_TREE_VIEW_COLUMN_FIXED);
				gtk_tree_view_column_set_sizing (date, GTK_TREE_VIEW_COLUMN_FIXED);
				gtk_tree_view_column_set_sizing (favicon, GTK_TREE_VIEW_COLUMN_FIXED);
				gtk_tree_view_column_set_sizing (headline, GTK_TREE_VIEW_COLUMN_FIXED);			

				gtk_tree_view_column_set_sort_column_id (headline, IS_TIME);				
				g_object_set (ilv->headlineRenderer, "ellipsize", PANGO_ELLIPSIZE_NONE, NULL);
				g_object_set (ilv->headlineRenderer, "wrap-mode", PANGO_WRAP_WORD, NULL);
				g_object_set (ilv->headlineRenderer, "wrap-width", 300, NULL);
			} else {
				
				gtk_tree_view_set_grid_lines (ilv->treeview, GTK_TREE_VIEW_GRID_LINES_NONE);
				gtk_tree_view_column_set_sizing (date, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
				gtk_tree_view_column_set_sizing (headline, GTK_TREE_VIEW_COLUMN_GROW_ONLY);

				gtk_tree_view_column_set_sort_column_id (headline, IS_LABEL);
				
				g_object_set (ilv->headlineRenderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
				g_object_set (ilv->headlineRenderer, "wrap-mode", PANGO_WRAP_NONE, NULL);
			}

			gtk_tree_view_set_fixed_height_mode (ilv->treeview, ilv->wideView);

			break;
	}
}

static void
item_list_view_class_init (ItemListViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = item_list_view_finalize;
	object_class->get_property = item_list_view_get_property;
	object_class->set_property = item_list_view_set_property;

	item_list_view_signals[SELECTION_CHANGED] =
		g_signal_new ("selection-changed",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__INT,
		G_TYPE_NONE,
		1,
		G_TYPE_INT);

	g_object_class_install_property (object_class,
		                         PROP_WIDE_VIEW,
		                         g_param_spec_boolean ("wide-view",
		                                               "Wide View",
		                                               "TRUE if wide mode rendering with more text and less columns is used",
		                                               FALSE,
		                                               G_PARAM_READWRITE));
}

/* helper functions for item <-> iter conversion */

static gulong
item_list_view_iter_to_id (ItemListView *ilv, GtkTreeIter *iter)
{
	gulong	id = 0;

	gtk_tree_model_get (gtk_tree_view_get_model (ilv->treeview), iter, IS_NR, &id, -1);
	return id;
}

gboolean
item_list_view_contains_id (ItemListView *ilv, gulong id)
{
	return (NULL != g_slist_find (ilv->item_ids, GUINT_TO_POINTER (id)));
}

static gboolean
item_list_view_id_to_iter (ItemListView *ilv, gulong id, GtkTreeIter *iter)
{
	gboolean        valid;
	GtkTreeIter		old_iter;
	GtkTreeModel    *model;

	/* Problem here is batch insertion using batch_itemstore
	   so the item can be in the GtkTreeView attached store or
	    in the batch_itemstore */

	if (item_list_view_contains_id (ilv, id)) {
		/* First search the tree view */
		model = gtk_tree_view_get_model (ilv->treeview);
		valid = gtk_tree_model_get_iter_first (model, &old_iter);
		while (valid) {
		        gulong current_id = item_list_view_iter_to_id (ilv, &old_iter);
	                if(current_id == id) {
				*iter = old_iter;
	                        return TRUE;
			}
	                valid = gtk_tree_model_iter_next (model, &old_iter);
		}

		/* Next search the batch store */
		model = GTK_TREE_MODEL (ilv->batch_itemstore);
		valid = gtk_tree_model_get_iter_first (model, &old_iter);
		while (valid) {
			gulong current_id;
			gtk_tree_model_get (model, &old_iter, IS_NR, &current_id, -1);
	                if(current_id == id) {
				*iter = old_iter;
	                        return TRUE;
			}
	                valid = gtk_tree_model_iter_next (model, &old_iter);
		}
	}
	return FALSE;
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
	Node	*node1, *node2;

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
			/* Some ugly switching here, because in wide view
			   we do sort headlines by date */
			if (ilv->wideView)
				sortColumn = IS_TIME;
			else
				sortColumn = IS_LABEL;
			break;
		case NODE_VIEW_SORT_BY_PARENT:
			sortColumn = IS_SOURCE;
			break;
		case NODE_VIEW_SORT_BY_STATE:
			sortColumn = IS_STATE;
			break;
		case NODE_VIEW_SORT_BY_TIME:
		default:
			sortColumn = IS_TIME;
			break;
	}

	gtk_tree_sortable_set_sort_column_id (GTK_TREE_SORTABLE (gtk_tree_view_get_model (ilv->treeview)),
	                                      sortColumn,
	                                      sortReversed?GTK_SORT_DESCENDING:GTK_SORT_ASCENDING);
}

/*
 * Creates a GtkTreeStore to be filled with ui_itemlist_set_items
 * and to be set with ui_itemlist_set_tree_store().
 */
static GtkTreeStore *
item_list_view_create_tree_store (void)
{
	return gtk_tree_store_new (ITEMSTORE_LEN,
	                    G_TYPE_INT64,	/* IS_TIME */
	                    G_TYPE_STRING, 	/* IS_TIME_STR */
	                    G_TYPE_STRING,	/* IS_LABEL */
	                    G_TYPE_ICON,	/* IS_STATEICON */
	                    G_TYPE_ULONG,	/* IS_NR */
	                    G_TYPE_POINTER,	/* IS_PARENT */
	                    G_TYPE_ICON,	/* IS_FAVICON */
	                    G_TYPE_POINTER,	/* IS_SOURCE */
	                    G_TYPE_UINT,	/* IS_STATE */
			    G_TYPE_INT,		/* ITEMSTORE_WEIGHT */
			    G_TYPE_FLOAT        /* ITEMSTORE_ALIGN */
	);
}

static void
on_itemlist_selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	GtkTreeIter 	iter;
	GtkTreeModel	*model;
	gulong		id = 0;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		id = item_list_view_iter_to_id (ITEM_LIST_VIEW (user_data), &iter);
		g_signal_emit_by_name (user_data, "selection-changed", id);
	}
}

static void
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
		case IS_STATE:
			nodeSort = NODE_VIEW_SORT_BY_STATE;
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

/*
 * Sets a GtkTreeView to the active GtkTreeView.
 */
static void
item_list_view_set_tree_store (ItemListView *ilv, GtkTreeStore *itemstore)
{
	GtkTreeModel    	*model;
	GtkTreeSelection	*select;

	/* drop old tree store */
	model = gtk_tree_view_get_model (ilv->treeview);
	gtk_tree_view_set_model (ilv->treeview, NULL);
	if (model)
		g_object_unref (model);

	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (itemstore), IS_TIME, item_list_view_date_sort_func, NULL, NULL);
	gtk_tree_sortable_set_sort_func (GTK_TREE_SORTABLE (itemstore), IS_SOURCE, item_list_view_favicon_sort_func, NULL, NULL);
	g_signal_connect (G_OBJECT (itemstore), "sort-column-changed", G_CALLBACK (itemlist_sort_column_changed_cb), NULL);

	gtk_tree_view_set_model (ilv->treeview, GTK_TREE_MODEL (itemstore));

	/* Setup the selection handler */
	select = gtk_tree_view_get_selection (ilv->treeview);
	gtk_tree_selection_set_mode (select, GTK_SELECTION_SINGLE);
	g_signal_connect (G_OBJECT (select), "changed", G_CALLBACK (on_itemlist_selection_changed), ilv);
}

static void
item_list_view_all_items_removed (GObject *obj, gpointer user_data)
{
	ItemListView	*ilv = ITEM_LIST_VIEW (user_data);
	GtkTreeModel	*model;
	GtkTreeSelection *select;

	debug (DEBUG_CACHE, "item_list_view_all_items_removed()");

	/* Unselect all items */
	select = gtk_tree_view_get_selection (ilv->treeview);
	gtk_tree_selection_unselect_all (select);

	/* Clear the item ids */
	if (ilv->item_ids)
		g_slist_free (ilv->item_ids);
	ilv->item_ids = NULL;

	/* Clear the tree store */
	model = gtk_tree_view_get_model (ilv->treeview);
	gtk_tree_store_clear (GTK_TREE_STORE (model));
}

static void
item_list_view_item_removed (GObject *obj, gulong id, gpointer user_data)
{
	ItemListView	*ilv = ITEM_LIST_VIEW (user_data);
	GtkTreeIter	iter;

	if (item_list_view_id_to_iter (ilv, id, &iter)) {
		/* Using the GtkTreeIter check if it is currently selected. If yes,
		   scroll down by one in the sorted GtkTreeView to ensure something
		   is selected after removing the GtkTreeIter */
		GtkTreePath	*path = gtk_tree_model_get_path (gtk_tree_view_get_model (ilv->treeview), &iter);
		GtkTreeSelection *select = gtk_tree_view_get_selection (ilv->treeview);
		GtkTreePath	*next = gtk_tree_path_copy (path);
		GtkTreeIter	nextIter;
		
		g_assert (select);
		g_assert (path);
		g_assert (next);
		gtk_tree_path_next (next);
		gtk_tree_model_get_iter (gtk_tree_view_get_model (ilv->treeview), &nextIter, next);
		gtk_tree_selection_select_iter (select, &nextIter);
		
		gtk_tree_store_remove (GTK_TREE_STORE (gtk_tree_view_get_model (ilv->treeview)), &iter);
	} else {
		g_warning ("Fatal: item to be removed not found in item id list!");
	}

	ilv->item_ids = g_slist_remove (ilv->item_ids, GUINT_TO_POINTER (id));
}

/* cleans up the item list, sets up the iter hash when called for the first time */
static void
item_list_view_item_batch_started (GObject *obj, gpointer user_data)
{
	ItemListView		*ilv = ITEM_LIST_VIEW (user_data);
	GtkAdjustment		*adj;
	GtkTreeStore		*itemstore;
	GtkTreeSelection	*select;

        itemstore = GTK_TREE_STORE (gtk_tree_view_get_model (ilv->treeview));

	/* unselecting all items is important to remove items
	   whose removal is deferred until unselecting */
	select = gtk_tree_view_get_selection (ilv->treeview);
	gtk_tree_selection_unselect_all (select);

	adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (ilv->treeview));
	gtk_adjustment_set_value (adj, 0.0);
	gtk_scrollable_set_vadjustment (GTK_SCROLLABLE (ilv->treeview), adj);

	/* Disconnect signal handler to be safe */
	g_signal_handlers_disconnect_by_func (G_OBJECT (select), G_CALLBACK (on_itemlist_selection_changed), ilv);

	if (itemstore)
		gtk_tree_store_clear (itemstore);
	if (ilv->item_ids)
		g_slist_free (ilv->item_ids);

	ilv->item_ids = NULL;

	/* enable batch mode for following item adds */
	ilv->batch_mode = TRUE;
	ilv->batch_itemstore = item_list_view_create_tree_store ();

	gtk_tree_sortable_set_default_sort_func (GTK_TREE_SORTABLE (ilv->batch_itemstore), NULL, NULL, NULL);
}

static void
item_list_view_item_batch_ended (GObject *obj, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);

	g_assert (ilv->batch_mode);
	item_list_view_set_tree_store (ilv, ilv->batch_itemstore);
	ilv->batch_mode = FALSE;
}

static gfloat
item_list_title_alignment (gchar *title)
{
	if (!title || strlen(title) == 0)
		return 0.;

	/* debug5 (DEBUG_HTML, "title ***%s*** first bytes %02hhx%02hhx%02hhx pango %d",
		title, title[0], title[1], title[2], pango_find_base_dir (title, -1)); */
	int txt_direction = common_find_base_dir (title, -1);
  	int app_direction = gtk_widget_get_default_direction ();
	if ((txt_direction == PANGO_DIRECTION_LTR &&
	     app_direction == GTK_TEXT_DIR_LTR) ||
	    (txt_direction == PANGO_DIRECTION_RTL &&
	     app_direction == GTK_TEXT_DIR_RTL))
		return 0.; /* same direction, regular ("left") alignment */
	else
		return 1.;
}

static void
item_list_view_update_item_internal (ItemListView *ilv, itemPtr item, GtkTreeIter *iter, Node *node)
{
	GtkTreeStore	*itemstore;
	gchar		*title, *time_str, *tmp = NULL;
	const GIcon	*state_icon;
	gint		state = 0;
	gboolean	noTitle = FALSE;
        int fontWeight = PANGO_WEIGHT_BOLD;

	if (item->flagStatus)
		state += 2;
	if (!item->readStatus)
		state += 1;

	time_str = (0 != item->time) ? date_format ((time_t)item->time, NULL) : g_strdup ("");

	if(item->title && strlen (item->title)) {
		title = item->title;
		title = g_strstrip (g_markup_escape_text (title, -1));
	} else {
		// when there is no title use the teaser
		tmp = item_get_teaser (item);
		title = g_strdup_printf("%s…", tmp);
		g_free (tmp);
		noTitle = TRUE;
	}

	if (ilv->wideView) {
		const gchar *important = _(" <span background='red' color='black'> important </span> ");
		gchar *teaser = NULL;
		if(!noTitle)
			teaser = item_get_teaser (item);

		tmp = title;
		title = g_strdup_printf ("<span weight='%s' size='larger'>%s</span>%s\n<span weight='%s'>%s%s</span><span size='smaller' weight='ultralight'> — %s</span>",
		                         item->readStatus?"normal":"ultrabold",
		                         title,
		                         item->flagStatus?important:"",
		                         item->readStatus?"ultralight":"light",
		                         teaser?teaser:"",
		                         teaser?"…":"",
					 time_str);
		g_free (tmp);
		g_free (teaser);
	}

	state_icon = item->flagStatus ? icon_get (ICON_FLAG) :
	             !item->readStatus ? icon_get (ICON_UNREAD) :
		     NULL;

        if (item->readStatus)
                fontWeight = item->isHidden ? PANGO_WEIGHT_ULTRALIGHT : PANGO_WEIGHT_NORMAL;

	if (ilv->batch_mode)
		itemstore = ilv->batch_itemstore;
	else
		itemstore = GTK_TREE_STORE (gtk_tree_view_get_model (ilv->treeview));

        if (NULL == node) {
                gtk_tree_store_set (itemstore, iter,
		            IS_LABEL, title,
	                    IS_TIME, item->time,
			    IS_TIME_STR, time_str,
                            IS_NR, item->id,
			    IS_STATEICON, state_icon,
			    ITEMSTORE_ALIGN, item_list_title_alignment (title),
		            IS_STATE, state,
	                    ITEMSTORE_WEIGHT, fontWeight,
			    -1);
        } else {
                gtk_tree_store_set (itemstore, iter,
		            IS_LABEL, title,
                            IS_TIME, item->time,
			    IS_TIME_STR, time_str,
                            IS_NR, item->id,
			    IS_STATEICON, state_icon,
                            IS_PARENT, node,
                            IS_FAVICON, node_get_icon (node),
                            IS_SOURCE, node,
                            IS_STATE, state,
	                    ITEMSTORE_WEIGHT, fontWeight,
                            -1);
        }

	g_free (time_str);
	g_free (title);
}

void
item_list_view_update_item (ItemListView *ilv, itemPtr item)
{
	GtkTreeIter	iter;

	if (!item_list_view_id_to_iter (ilv, item->id, &iter))
		return;

        item_list_view_update_item_internal (ilv, item, &iter, NULL);
}

static void
item_list_view_item_updated (GObject *obj, gint itemId, gpointer user_data)
{
	itemPtr item = item_load (itemId);
	item_list_view_update_item (ITEM_LIST_VIEW (user_data), item);
	item_unload (item);
}

static void
item_list_view_update_all_items (GObject *obj, gpointer user_data)
{
	ItemListView	*ilv = ITEM_LIST_VIEW (user_data);
        gboolean        valid;
        GtkTreeIter     iter;
        gulong          id;
	GtkTreeModel    *model;

	model = gtk_tree_view_get_model (ilv->treeview);
        valid = gtk_tree_model_get_iter_first (model, &iter);
	while (valid) {
		gtk_tree_model_get (model, &iter, IS_NR, &id, -1);
                itemPtr	item = item_load (id);
                item_list_view_update_item (ilv, item);
                item_unload (item);
                valid = gtk_tree_model_iter_next (model, &iter);
	}
}

static gboolean
on_item_list_view_key_pressed_event (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);

	switch (keyval) {
		case GDK_KEY_Delete:
		case GDK_KEY_KP_Delete:
			itemlist_remove_item (itemlist_get_selected ());
			return TRUE;
		case GDK_KEY_space:
			itemlist_toggle_read_status (itemlist_get_selected ());
			return TRUE;
		case GDK_KEY_Return:
		case GDK_KEY_KP_Enter:
			browser_launch_item (itemlist_get_selected (), BROWSER_LAUNCH_DEFAULT);
			return TRUE;
		default:
			return FALSE;
	}
}

static GMenu *
item_list_view_popup_menu (ItemListView *ilv, itemPtr item)
{
	GMenu		*menu = g_menu_new();
	GMenuItem	*menu_item;
	gchar		*text, *item_link;
	const		gchar *author;

	item_link = item_make_link (item);
	author = item_get_author (item);

	GMenu *section = g_menu_new ();
	menu_item = g_menu_item_new (NULL, NULL);

	g_menu_item_set_label(menu_item, _("Open In _Tab"));
	g_menu_item_set_action_and_target(menu_item, "app.open-item-in-tab", "t", (guint64) item->id);
	g_menu_append_item(section, menu_item);

	g_menu_item_set_label(menu_item, _("_Open In Browser"));
	g_menu_item_set_action_and_target(menu_item, "app.open-item-in-browser", "t", (guint64) item->id);
	g_menu_append_item(section, menu_item);

	g_menu_item_set_label(menu_item, _("Open In _External Browser"));
	g_menu_item_set_action_and_target(menu_item, "app.open-item-in-external-browser", "t", (guint64) item->id);
	g_menu_append_item(section, menu_item);

	if(author){
		g_menu_item_set_label(menu_item, _("Email The Author"));
		g_menu_item_set_action_and_target(menu_item, "app.email-the-author", "t", (guint64) item->id);
		g_menu_append_item(section, menu_item);
	}

	g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
	g_object_unref(section);

	GSList *iter = newsbin_get_list();
	if (iter) {
		GMenu *submenu;
		guint32 i = 0;

		section = g_menu_new();
		submenu = g_menu_new();

		while (iter) {
			Node *node = (Node *)iter->data;
			g_menu_item_set_label(menu_item, node_get_title(node));
			g_menu_item_set_action_and_target(menu_item, "app.copy-item-to-newsbin", "(ut)", i, (guint64) item->id);
			g_menu_append_item(submenu, menu_item);
			iter = g_slist_next(iter);
			i++;
		}

		g_menu_append_submenu(section, _("Copy to News Bin"), G_MENU_MODEL(submenu));
		g_object_unref(submenu);
		g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
		g_object_unref(section);
	}

	section = g_menu_new();

	text = g_strdup_printf(_("_Bookmark at %s"), social_get_bookmark_site());
	g_menu_item_set_label(menu_item, text);
	g_menu_item_set_action_and_target(menu_item, "app.social-bookmark-link", "(ss)", item_link, item_get_title(item));
	g_menu_append_item(section, menu_item);
	g_free(text);

	g_menu_item_set_label(menu_item, _("Copy Item _Location"));
	g_menu_item_set_action_and_target(menu_item, "app.copy-link-to-clipboard", "s", item_link);
	g_menu_append_item(section, menu_item);

	g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
	g_object_unref(section);

	section = g_menu_new();

	g_menu_item_set_label(menu_item, _("Toggle _Read Status"));
	g_menu_item_set_action_and_target(menu_item, "app.toggle-item-read-status", "t", (guint64) item->id);
	g_menu_append_item(section, menu_item);

	g_menu_item_set_label(menu_item, _("Toggle Item _Flag"));
	g_menu_item_set_action_and_target(menu_item, "app.toggle-item-flag", "t", (guint64) item->id);
	g_menu_append_item(section, menu_item);

	g_menu_item_set_label(menu_item, _("R_emove Item"));
	g_menu_item_set_action_and_target(menu_item, "app.remove-item", "t", (guint64) item->id);
	g_menu_append_item(section, menu_item);

	g_menu_append_section(menu, NULL, G_MENU_MODEL(section));
	g_object_unref(section);

	g_object_unref(menu_item);
	g_free(item_link);

	return menu;
}

static gboolean
on_item_list_view_pressed_event (GtkGestureClick *gesture, guint n_press, gdouble x, gdouble y, gpointer user_data)
{
	ItemListView		*ilv = ITEM_LIST_VIEW (user_data);
	GtkTreePath		*path;
	GtkTreeIter		iter;
	GtkTreeViewColumn	*column;
	itemPtr			item = NULL;
	gboolean		result = FALSE;
	gint			bx, by;

	gtk_tree_view_convert_widget_to_bin_window_coords (ilv->treeview, (int)x, (int)y, &bx, &by);

	if (gtk_tree_view_get_path_at_pos (ilv->treeview, (gint)bx, (gint)by, &path, &column, NULL, NULL)) {
		if (gtk_tree_model_get_iter (gtk_tree_view_get_model (ilv->treeview), &iter, path))
			item = item_load (item_list_view_iter_to_id (ilv, &iter));
		gtk_tree_path_free (path);
	}

	if (item) {
		if (n_press == 1) {
			switch (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture))) {
				case GDK_BUTTON_PRIMARY:
					if (column == g_hash_table_lookup (ilv->columns, "favicon") ||
					    column == g_hash_table_lookup (ilv->columns, "state")) {
						itemlist_toggle_flag (item);
						result = TRUE;
					}
					break;
				case GDK_BUTTON_MIDDLE:
					/* Middle mouse click toggles read status... */
					itemlist_toggle_read_status (item);
					result = TRUE;
					break;
				case GDK_BUTTON_SECONDARY:
					/* Create a context menu */
					GMenu *menu = item_list_view_popup_menu (ilv, item);
					GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL(menu));
					gtk_widget_set_parent (popover, GTK_WIDGET (ilv->treeview));
					GdkRectangle rect;
					rect.x = (int)x;
					rect.y = (int)y;
					rect.width = 1;
					rect.height = 1;
					gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
					gtk_popover_popup (GTK_POPOVER (popover));
					g_object_unref(menu);
					result = TRUE;
					break;
			}
		}
		item_unload (item);
	}

	return result;
} 

static void
on_item_list_row_activated (GtkTreeView *treeview,
                           GtkTreePath *path,
			   GtkTreeViewColumn *column,
			   gpointer user_data)
{
	GtkTreeModel *model = gtk_tree_view_get_model (treeview);
	GtkTreeIter iter;

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		itemPtr item = item_load (item_list_view_iter_to_id (ITEM_LIST_VIEW (user_data), &iter));
		browser_launch_item (item, BROWSER_LAUNCH_DEFAULT);
		item_unload (item);
	}
}

static void
on_item_list_view_columns_changed (GtkTreeView *treeview, ItemListView *ilv)
{
	gint i = 0;
	GList *columns;
	GHashTableIter iter;
	gpointer colname, colptr;
	gchar *strv[5];

	/* This handler is only used for drag and drop reordering, so it
	   should not be hooked up with less than the full number of columns
	   eg: on item_list_view creation or teardown */
	if (gtk_tree_view_get_n_columns(treeview) != 4)
		return;

	columns = gtk_tree_view_get_columns (treeview);
	for (GList *li = columns; li; li = li->next) {
		g_hash_table_iter_init (&iter, ilv->columns);
		while (g_hash_table_iter_next (&iter, &colname, &colptr)) {
			if (li->data == colptr) {
				strv[i++] = colname;
				strv[i] = NULL;
				break;
			}
		}
	}
	conf_set_strv_value (LIST_VIEW_COLUMN_ORDER, (const gchar **)strv);

	g_list_free (columns);
}

GtkWidget *
item_list_view_get_widget (ItemListView *ilv)
{
	return ilv->ilscrolledwindow;
}

void
item_list_view_move_cursor (ItemListView *ilv, int step)
{
	ui_common_treeview_move_cursor (ilv->treeview, step);
}

void
item_list_view_move_cursor_to_first (ItemListView *ilv)
{
	ui_common_treeview_move_cursor_to_first (ilv->treeview);
}

static void
item_list_view_add_item_to_tree_store (ItemListView *ilv, GtkTreeStore *itemstore, itemPtr item)
{
	Node		*node;
	GtkTreeIter	iter;
	gboolean	exists = FALSE;

	node = node_from_id (item->nodeId);
	if(!node)
		return;	/* comment items do cause this... maybe filtering them earlier would be a good idea... */

        if (!ilv->batch_mode)
            exists = item_list_view_id_to_iter (ilv, item->id, &iter);

	if (!exists) {
		gtk_tree_store_prepend (itemstore, &iter, NULL);
		ilv->item_ids = g_slist_prepend (ilv->item_ids, GUINT_TO_POINTER (item->id));
	}

	item_list_view_update_item_internal (ilv, item, &iter, node);
}

static void
item_list_view_item_added (GObject *obj, gint itemId, gpointer userdata)
{
	ItemListView	*ilv = ITEM_LIST_VIEW (userdata);
	GtkTreeStore	*itemstore;
	itemPtr item = item_load (itemId);

	if (ilv->batch_mode) {
		/* either merge to new unattached GtkTreeStore */
		item_list_view_add_item_to_tree_store (ilv, ilv->batch_itemstore, item);
	} else {
		/* or merge to visible item store */
		itemstore = GTK_TREE_STORE (gtk_tree_view_get_model (ilv->treeview));
		item_list_view_add_item_to_tree_store (ilv, itemstore, item);
	}
}

static void
item_list_view_select (GObject *obj, gint id, gpointer user_data)
{
	ItemListView		*ilv = ITEM_LIST_VIEW (user_data);
	GtkTreeView		*treeview = ilv->treeview;
	GtkTreeSelection	*selection;
	GtkTreeIter		iter;

	if (!treeview)
		return;

	selection = gtk_tree_view_get_selection (treeview);

	if (id) {
		if (item_list_view_id_to_iter (ilv, id, &iter)) {
			GtkTreePath	*path = NULL;

			path = gtk_tree_model_get_path (gtk_tree_view_get_model (treeview), &iter);
			if (path) {
				gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
				gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.0, 0.0);
				gtk_tree_path_free (path);
			}
		} else {
			g_warning ("item_list_view_select : attempt to select an item which is not present in the view.");
			gtk_tree_selection_unselect_all (selection);
		}
	} else {
		gtk_tree_selection_unselect_all (selection);
	}
}

static void
item_list_view_init (ItemListView *ilv)
{
}

ItemListView *
item_list_view_create (FeedList *feedlist, ItemList *itemlist)
{
	ItemListView		*ilv;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column, *headline_column;
	gchar			**conf_column_order;

	ilv = g_object_new (ITEM_LIST_VIEW_TYPE, NULL);
	ilv->wideView = FALSE;

	ilv->columns = g_hash_table_new (g_str_hash, g_str_equal);

	ilv->keypress = gtk_event_controller_key_new ();
	ilv->gesture = gtk_gesture_click_new ();
	ilv->popup_gesture = gtk_gesture_click_new ();
	ilv->middle_gesture = gtk_gesture_click_new ();

	ilv->ilscrolledwindow = gtk_scrolled_window_new ();
	gtk_widget_set_vexpand (ilv->ilscrolledwindow, TRUE);
	g_object_ref_sink (ilv->ilscrolledwindow);
	gtk_widget_show (ilv->ilscrolledwindow);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ilv->ilscrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	ilv->treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
g_print("ilv->treeview = %p\n", ilv->treeview);
	gtk_tree_view_set_enable_search (ilv->treeview, FALSE);
	gtk_tree_view_set_show_expanders (ilv->treeview, FALSE);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (ilv->ilscrolledwindow), GTK_WIDGET (ilv->treeview));
	gtk_widget_show (GTK_WIDGET (ilv->treeview));
	gtk_widget_set_name (GTK_WIDGET (ilv->treeview), "itemlist");

	item_list_view_set_tree_store (ilv, item_list_view_create_tree_store ());

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "gicon", IS_STATEICON, NULL);
	
	//g_object_set (renderer, "stock-size", wide?GTK_ICON_SIZE_LARGE:GTK_ICON_SIZE_NORMAL, NULL);
	g_hash_table_insert (ilv->columns, "state", column);
	gtk_tree_view_column_set_sort_column_id (column, IS_STATE);

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "gicon", IS_FAVICON, NULL);
	//g_object_set (renderer, "stock-size", wide?GTK_ICON_SIZE_LARGE:GTK_ICON_SIZE_NORMAL, NULL);
	gtk_tree_view_column_set_sort_column_id (column, IS_SOURCE);
	g_hash_table_insert (ilv->columns, "favicon", column);

	ilv->headlineRenderer = gtk_cell_renderer_text_new ();
	headline_column = gtk_tree_view_column_new_with_attributes (_("Headline"), ilv->headlineRenderer,
	                                                   "markup", IS_LABEL,
							   "xalign", ITEMSTORE_ALIGN,
							   NULL);
	gtk_tree_view_column_set_expand (headline_column, TRUE);
	g_hash_table_insert (ilv->columns, "headline", headline_column);
	g_object_set (headline_column, "resizable", TRUE, NULL);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Date"), renderer,
		                                           "text", IS_TIME_STR,
	                                                   "weight", ITEMSTORE_WEIGHT,
							   NULL);
	gtk_tree_view_column_set_sizing (column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	g_hash_table_insert (ilv->columns, "date", column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TIME);

	conf_get_strv_value (LIST_VIEW_COLUMN_ORDER, &conf_column_order);
	for (gchar **li = conf_column_order; *li; li++) {
		column = g_hash_table_lookup (ilv->columns, *li);
		if (GTK_IS_TREE_VIEW_COLUMN (column)) {
			g_object_set (column, "reorderable", TRUE, NULL);
			gtk_tree_view_append_column (ilv->treeview, column);
		}
	}
	g_strfreev (conf_column_order);

	/* And connect signals */
	g_signal_connect (G_OBJECT (ilv->treeview), "columns_changed", G_CALLBACK (on_item_list_view_columns_changed), ilv);
	g_signal_connect (G_OBJECT (ilv->treeview), "row_activated", G_CALLBACK (on_item_list_row_activated), ilv);

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (ilv->middle_gesture), GDK_BUTTON_MIDDLE);
	g_signal_connect (ilv->middle_gesture, "pressed", G_CALLBACK (on_item_list_view_pressed_event), ilv);
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (ilv->popup_gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect (ilv->popup_gesture, "pressed", G_CALLBACK (on_item_list_view_pressed_event), ilv);
	g_signal_connect (ilv->gesture, "pressed", G_CALLBACK (on_item_list_view_pressed_event), ilv);
	g_signal_connect (ilv->keypress, "key-pressed", G_CALLBACK (on_item_list_view_key_pressed_event), ilv);
	
	gtk_widget_add_controller (GTK_WIDGET (ilv->treeview), GTK_EVENT_CONTROLLER (ilv->middle_gesture));
	gtk_widget_add_controller (GTK_WIDGET (ilv->treeview), GTK_EVENT_CONTROLLER (ilv->popup_gesture));
	gtk_widget_add_controller (GTK_WIDGET (ilv->treeview), GTK_EVENT_CONTROLLER(ilv->gesture));
	gtk_widget_add_controller (GTK_WIDGET (ilv->treeview), ilv->keypress);

	g_signal_connect (feedlist, "items-updated", G_CALLBACK (item_list_view_update_all_items), ilv);
	g_signal_connect (itemlist, "item-batch-start", G_CALLBACK (item_list_view_item_batch_started), ilv);
	g_signal_connect (itemlist, "item-batch-end", G_CALLBACK (item_list_view_item_batch_ended), ilv);
	g_signal_connect (itemlist, "item-added", G_CALLBACK (item_list_view_item_added), ilv);
	g_signal_connect (itemlist, "all-items-removed", G_CALLBACK (item_list_view_all_items_removed), ilv);
	g_signal_connect (itemlist, "item-removed", G_CALLBACK (item_list_view_item_removed), ilv);
	g_signal_connect (itemlist, "item-updated", G_CALLBACK (item_list_view_item_updated), ilv);
	g_signal_connect (itemlist, "item-selected", G_CALLBACK (item_list_view_select), ilv);
	
	g_signal_connect (ilv, "selection-changed", G_CALLBACK (itemlist_selection_changed), itemlist);

	return ilv;
}

void
item_list_view_enable_favicon_column (ItemListView *ilv, gboolean enabled)
{
	gtk_tree_view_column_set_visible (g_hash_table_lookup(ilv->columns, "favicon"), enabled);

	// In wide view we want to save vertical space and hide the state column
	if (ilv->wideView)
		gtk_tree_view_column_set_visible (g_hash_table_lookup(ilv->columns, "state"), !enabled);
}

itemPtr
item_list_view_find_unread_item (ItemListView *ilv, gulong startId)
{
	GtkTreeIter		iter;
	GtkTreeModel		*model;
	gboolean		valid = TRUE;

	model = gtk_tree_view_get_model (ilv->treeview);

	if (startId)
		valid = item_list_view_id_to_iter (ilv, startId, &iter);
	else
		valid = gtk_tree_model_get_iter_first (model, &iter);

	while (valid) {
		itemPtr	item = item_load (item_list_view_iter_to_id (ilv, &iter));
		if (item) {
			/* Skip the selected item */
			if (!item->readStatus && item->id != startId)
				return item;
			item_unload (item);
		}
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return NULL;
}

void
on_popup_copy_URL_clipboard (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr         item;

	item = itemlist_get_selected ();
	if (item) {
		g_autofree gchar *link = item_make_link (item);
		liferea_shell_copy_to_clipboard (link);
		item_unload (item);
	} else {
		liferea_shell_set_important_status_bar (_("No item has been selected"));
	}
}

void
on_popup_social_bm_item_selected (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item;

	item = itemlist_get_selected ();
	if (item) {
		social_add_bookmark (item);
		item_unload (item);
	}
	else
		liferea_shell_set_important_status_bar (_("No item has been selected"));
}
