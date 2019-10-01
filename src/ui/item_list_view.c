/*
 * @file item_list_view.c  presenting items in a GtkTreeView
 *
 * Copyright (C) 2004-2018 Lars Windolf <lars.windolf@gmx.de>
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
#include "feed.h"
#include "feedlist.h"
#include "item.h"
#include "itemlist.h"
#include "item_state.h"
#include "itemview.h"
#include "newsbin.h"
#include "social.h"
#include "xml.h"
#include "ui/browser_tabs.h"
#include "ui/icons.h"
#include "ui/liferea_shell.h"
#include "ui/popup_menu.h"
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
	IS_ENCICON,		/*<< Pixbuf reference to the item's enclosure icon */
	IS_ENCLOSURE,		/*<< Flag whether enclosure is attached or not */
	IS_SOURCE,		/*<< Source node pointer */
	IS_STATE,		/*<< Original item state (unread, flagged...) for sorting */
	ITEMSTORE_WEIGHT,	/*<< Flag whether weight is to be bold and "unread" icon is to be shown */
	ITEMSTORE_ALIGN,        /*<< How to align title (RTL support) */
	ITEMSTORE_LEN		/*<< Number of columns in the itemstore */
};

typedef enum {
	DEFAULT,
	INTERNAL,
	TAB,
	EXTERNAL
} open_link_target_type;

static void
launch_item (itemPtr item, open_link_target_type open_link_target)
{
	if (item) {
		gchar *link = item_make_link (item);

		if (link) {
			switch (open_link_target)
			{
			case DEFAULT:
				itemview_launch_URL (link, FALSE);
				break;
			case INTERNAL:
				itemview_launch_URL (link, TRUE);
				break;
			case TAB:
				browser_tabs_add_new (link, link, FALSE);
				break;
			case EXTERNAL:
				browser_launch_URL_external (link);
				break;
			}

			item_set_read_state (item, TRUE);
			g_free (link);
		} else
			ui_show_error_box (_("This item has no link specified!"));

	}
}

struct _ItemListView {
	GObject		parentInstance;

	GtkTreeView	*treeview;
	GtkWidget 	*ilscrolledwindow;	/*<< The complete ItemListView widget */
	GSList      *item_ids;		/*<< list of all currently known item ids */

	gboolean	batch_mode;		/*<< TRUE if we are in batch adding mode */
	GtkTreeStore	*batch_itemstore;	/*<< GtkTreeStore prepared unattached and to be set on update() */

	GHashTable	*columns;               /*<< Named GtkTreeViewColumns */

	gboolean	wideView;		/*<< TRUE if date has to be rendered into headline column (because date column is invisible) */
};

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
}

static void
item_list_view_class_init (ItemListViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = item_list_view_finalize;
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
	                    G_TYPE_ICON,	/* IS_ENCICON */
	                    G_TYPE_BOOLEAN,	/* IS_ENCLOSURE */
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
	itemPtr		item = NULL;

	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gulong id = item_list_view_iter_to_id (ITEM_LIST_VIEW (user_data), &iter);
		if (id != itemlist_get_selected_id ()) {
			item = item_load (id);
			liferea_shell_update_item_menu (NULL != item);
			if (item)
				itemlist_selection_changed (item);
		}
	} else {
		liferea_shell_update_item_menu (FALSE);
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
	g_signal_connect (G_OBJECT (select), "changed",
	                  G_CALLBACK (on_itemlist_selection_changed), ilv);
}

void
item_list_view_remove_item (ItemListView *ilv, itemPtr item)
{
	GtkTreeIter	iter;

	g_assert (NULL != item);
	if (item_list_view_id_to_iter (ilv, item->id, &iter)) {
		/* Using the GtkTreeIter check if it is currently selected. If yes,
		   scroll down by one in the sorted GtkTreeView to ensure something
		   is selected after removing the GtkTreeIter */
		if (gtk_tree_selection_iter_is_selected (gtk_tree_view_get_selection (ilv->treeview), &iter))
			ui_common_treeview_move_cursor (ilv->treeview, 1);

		gtk_tree_store_remove (GTK_TREE_STORE (gtk_tree_view_get_model (ilv->treeview)), &iter);
	} else {
		g_warning ("Fatal: item to be removed not found in item id list!");
	}

	ilv->item_ids = g_slist_remove (ilv->item_ids, GUINT_TO_POINTER (item->id));
}

/* cleans up the item list, sets up the iter hash when called for the first time */
void
item_list_view_clear (ItemListView *ilv)
{
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
}

static gfloat
item_list_title_alignment (gchar *title)
{
	if (!title || strlen(title) == 0)
		return 0.;

	/* debug5 (DEBUG_HTML, "title ***%s*** first bytes %02hhx%02hhx%02hhx pango %d",
		title, title[0], title[1], title[2], pango_find_base_dir (title, -1)); */
	int txt_direction = pango_find_base_dir (title, -1);
  	int app_direction = gtk_widget_get_default_direction ();
	if ((txt_direction == PANGO_DIRECTION_LTR &&
	     app_direction == GTK_TEXT_DIR_LTR) ||
	    (txt_direction == PANGO_DIRECTION_RTL &&
	     app_direction == GTK_TEXT_DIR_RTL))
		return 0.; /* same direction, regular ("left") alignment */
	else
		return 1.;
}

void
item_list_view_update_item (ItemListView *ilv, itemPtr item)
{
	GtkTreeStore	*itemstore;
	GtkTreeIter	iter;
	gchar		*title, *time_str;
	const GIcon	*state_icon;

	if (!item_list_view_id_to_iter (ilv, item->id, &iter))
		return;

	time_str = (0 != item->time) ? date_format ((time_t)item->time, NULL) : g_strdup ("");

	title = item->title && strlen (item->title) ? item->title : _("*** No title ***");
	title = g_strstrip (g_markup_escape_text (title, -1));

	if (ilv->wideView) {
		const gchar *important = _(" <span background='red' color='black'> important </span> ");
		gchar *teaser = item_get_teaser (item);
		gchar *tmp = title;

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

	if (ilv->batch_mode)
		itemstore = ilv->batch_itemstore;
	else
		itemstore = GTK_TREE_STORE (gtk_tree_view_get_model (ilv->treeview));

	gtk_tree_store_set (itemstore,
	                    &iter,
		            IS_LABEL, title,
	                    IS_TIME, item->time,
			    IS_TIME_STR, time_str,
			    IS_STATEICON, state_icon,
			    ITEMSTORE_ALIGN, item_list_title_alignment (title),
	                    ITEMSTORE_WEIGHT, item->readStatus ? PANGO_WEIGHT_NORMAL : PANGO_WEIGHT_BOLD,
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

void
item_list_view_update (ItemListView *ilv, gboolean hasEnclosures)
{
	/* we depend on the fact that the third column is the enclosure icon column!!! */
	gtk_tree_view_column_set_visible (g_hash_table_lookup(ilv->columns, "enclosure"), hasEnclosures);

	if (ilv->batch_mode) {
		item_list_view_set_tree_store (ilv, ilv->batch_itemstore);
		ilv->batch_mode = FALSE;
	} else {
		/* Nothing to do in non-batch mode as items were added
		   and updated one-by-one in ui_itemlist_add_item() */
	}
}

static gboolean
on_item_list_view_key_press_event (GtkWidget *widget, GdkEventKey *event, gpointer data)
{
	if ((event->type == GDK_KEY_PRESS) && (event->state == 0)
	    && (event->keyval == GDK_KEY_Delete))
		on_action_remove_item(NULL, NULL, NULL);

	return FALSE;
}

/* Show tooltip when headline's column text (IS_LABEL) is truncated. */

static gint
get_cell_renderer_width (GtkWidget *widget, GtkCellRenderer *cell, const gchar *text, gint weight)
{
	PangoLayout	*layout = gtk_widget_create_pango_layout (widget, text);
	PangoAttrList	*attrbs = pango_attr_list_new();
	PangoRectangle	rect;
	gint		xpad = 0;

	pango_attr_list_insert (attrbs, pango_attr_weight_new (weight));
	pango_layout_set_attributes (layout, attrbs);
	pango_attr_list_unref (attrbs);
	pango_layout_get_pixel_extents (layout, NULL, &rect);
	g_object_unref (G_OBJECT (layout));

	gtk_cell_renderer_get_padding (cell, &xpad, NULL);
	return (xpad * 2) + rect.x + rect.width;
}

static gboolean
on_item_list_view_query_tooltip (GtkWidget *widget, gint x, gint y, gboolean keyboard_mode, GtkTooltip *tooltip, GtkTreeViewColumn *headline_column)
{
	GtkTreeView *view = GTK_TREE_VIEW (widget);
	GtkTreeModel *model; GtkTreePath *path; GtkTreeIter iter;
	gboolean ret = FALSE;

	if (gtk_tree_view_get_tooltip_context (view, &x, &y, keyboard_mode, &model, &path, &iter)) {
		GtkTreeViewColumn *column;
		gint bx, by;
		gtk_tree_view_convert_widget_to_bin_window_coords (view, x, y, &bx, &by);
		gtk_tree_view_get_path_at_pos (view, x, y, NULL, &column, NULL, NULL);

		if (column == headline_column) {
			GtkCellRenderer *cell;
			GList *renderers = gtk_cell_layout_get_cells (GTK_CELL_LAYOUT (column));
			cell = GTK_CELL_RENDERER (renderers->data);
			g_list_free (renderers);

			gchar *text;
			gint weight;
			gtk_tree_model_get (model, &iter, IS_LABEL, &text, ITEMSTORE_WEIGHT, &weight, -1);

			gint full_width = get_cell_renderer_width (widget, cell, text, weight);
			gint column_width = gtk_tree_view_column_get_width (column);
			if (full_width > column_width) {
				gtk_tooltip_set_text (tooltip, text);
				ret = TRUE;
			}
			g_free (text);
		}

		gtk_tree_view_set_tooltip_row (view, tooltip, path);
		gtk_tree_path_free (path);
	}
	return ret;
}

static gboolean
on_item_list_view_button_press_event (GtkWidget *treeview, GdkEvent *event, gpointer user_data)
{
	ItemListView		*ilv = ITEM_LIST_VIEW (user_data);
	GtkTreePath		*path;
	GtkTreeIter		iter;
	GtkTreeViewColumn	*column;
	GdkEventButton 		*eb;
	itemPtr			item = NULL;
	gboolean		result = FALSE;

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	eb = (GdkEventButton*) event;

	/* avoid handling header clicks */
	if (eb->window != gtk_tree_view_get_bin_window (ilv->treeview))
		return FALSE;

	if (!gtk_tree_view_get_path_at_pos (ilv->treeview, (gint)eb->x, (gint)eb->y, &path, &column, NULL, NULL))
		return FALSE;

	if (gtk_tree_model_get_iter (gtk_tree_view_get_model (ilv->treeview), &iter, path))
		item = item_load (item_list_view_iter_to_id (ilv, &iter));

	gtk_tree_path_free (path);

	if (item) {
		switch (eb->button) {
			case 1:
				if (column == g_hash_table_lookup(ilv->columns, "favicon") ||
				    column == g_hash_table_lookup(ilv->columns, "state")) {
					itemlist_toggle_flag (item);
					result = TRUE;
				}
				break;
			case 2:
				/* Middle mouse click toggles read status... */
				itemlist_toggle_read_status (item);
				result = TRUE;
				break;
			case 3:
				ui_popup_item_menu (item, event);
				result = TRUE;
				break;
			default:
				/* Do nothing on buttons >= 4 */
				break;
		}
		item_unload (item);
	}

	return result;
}

static gboolean
on_item_list_view_popup_menu (GtkWidget *widget, gpointer user_data)
{
	GtkTreeView	*treeview = GTK_TREE_VIEW (widget);
	GtkTreeModel	*model;
	GtkTreeIter	iter;

	if (gtk_tree_selection_get_selected (gtk_tree_view_get_selection (treeview), &model, &iter)) {
		itemPtr item = item_load (item_list_view_iter_to_id (ITEM_LIST_VIEW (user_data), &iter));
		ui_popup_item_menu (item, NULL);
		item_unload (item);
		return TRUE;
	}

	return FALSE;
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
		launch_item (item, DEFAULT);
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
	gchar *strv[6];

	/* This handler is only used for drag and drop reordering, so it
	   should not be hooked up with less than the full number of columns
	   eg: on item_list_view creation or teardown */
	if (gtk_tree_view_get_n_columns(treeview) != 5)
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
item_list_view_init (ItemListView *ilv)
{
}

ItemListView *
item_list_view_create (gboolean wide)
{
	ItemListView		*ilv;
	GtkCellRenderer		*renderer;
	GtkTreeViewColumn 	*column, *headline_column;
	gchar			**conf_column_order;

	ilv = g_object_new (ITEM_LIST_VIEW_TYPE, NULL);
	ilv->wideView = wide;

	ilv->columns = g_hash_table_new (g_str_hash, g_str_equal);

	ilv->ilscrolledwindow = gtk_scrolled_window_new (NULL, NULL);
	g_object_ref_sink (ilv->ilscrolledwindow);
	gtk_widget_show (ilv->ilscrolledwindow);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ilv->ilscrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (ilv->ilscrolledwindow), GTK_SHADOW_IN);

	ilv->treeview = GTK_TREE_VIEW (gtk_tree_view_new ());
	if (wide) {
		gtk_tree_view_set_fixed_height_mode (ilv->treeview, FALSE);
		gtk_tree_view_set_grid_lines (ilv->treeview, GTK_TREE_VIEW_GRID_LINES_HORIZONTAL);
	}
	gtk_container_add (GTK_CONTAINER (ilv->ilscrolledwindow), GTK_WIDGET (ilv->treeview));
	gtk_widget_show (GTK_WIDGET (ilv->treeview));
	gtk_widget_set_name (GTK_WIDGET (ilv->treeview), "itemlist");

	item_list_view_set_tree_store (ilv, item_list_view_create_tree_store ());

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "gicon", IS_STATEICON, NULL);
	g_object_set (renderer, "stock-size", wide?GTK_ICON_SIZE_LARGE_TOOLBAR:GTK_ICON_SIZE_SMALL_TOOLBAR, NULL);
	g_hash_table_insert (ilv->columns, "state", column);
	gtk_tree_view_column_set_sort_column_id (column, IS_STATE);
	if (wide)
		gtk_tree_view_column_set_visible (column, FALSE);

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "gicon", IS_FAVICON, NULL);
	g_object_set (renderer, "stock-size", wide?GTK_ICON_SIZE_DIALOG:GTK_ICON_SIZE_SMALL_TOOLBAR, NULL);

	gtk_tree_view_column_set_sort_column_id (column, IS_SOURCE);
	g_hash_table_insert (ilv->columns, "favicon", column);

	renderer = gtk_cell_renderer_text_new ();
	headline_column = gtk_tree_view_column_new_with_attributes (_("Headline"), renderer,
	                                                   "markup", IS_LABEL,
							   "xalign", ITEMSTORE_ALIGN,
							   NULL);
	gtk_tree_view_column_set_expand (headline_column, TRUE);
	g_hash_table_insert (ilv->columns, "headline", headline_column);
	g_object_set (headline_column, "resizable", TRUE, NULL);
	if (wide) {
		gtk_tree_view_column_set_sort_column_id (headline_column, IS_TIME);
		g_object_set (renderer, "wrap-mode", PANGO_WRAP_WORD, NULL);
		g_object_set (renderer, "wrap-width", 300, NULL);
	} else {
		gtk_tree_view_column_set_sort_column_id (headline_column, IS_LABEL);
		g_object_set (renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
		gtk_tree_view_column_add_attribute (headline_column, renderer, "weight", ITEMSTORE_WEIGHT);
	}

	renderer = gtk_cell_renderer_pixbuf_new ();
	column = gtk_tree_view_column_new_with_attributes ("", renderer, "gicon", IS_ENCICON, NULL);
	g_hash_table_insert (ilv->columns, "enclosure", column);

	renderer = gtk_cell_renderer_text_new ();
	column = gtk_tree_view_column_new_with_attributes (_("Date"), renderer,
		                                           "text", IS_TIME_STR,
	                                                   "weight", ITEMSTORE_WEIGHT,
							   NULL);
	gtk_tree_view_column_set_sizing(column, GTK_TREE_VIEW_COLUMN_GROW_ONLY);
	g_hash_table_insert (ilv->columns, "date", column);
	gtk_tree_view_column_set_sort_column_id(column, IS_TIME);
	if (wide)
		gtk_tree_view_column_set_visible (column, FALSE);

	conf_get_strv_value (LIST_VIEW_COLUMN_ORDER, &conf_column_order);
	for (gchar **li = conf_column_order; *li; li++) {
		column = g_hash_table_lookup (ilv->columns, *li);
		g_object_set (column, "reorderable", TRUE, NULL);
		gtk_tree_view_append_column (ilv->treeview, column);
	}
	g_strfreev (conf_column_order);

	/* And connect signals */
	g_signal_connect (G_OBJECT (ilv->treeview), "button_press_event", G_CALLBACK (on_item_list_view_button_press_event), ilv);
	g_signal_connect (G_OBJECT (ilv->treeview), "columns_changed", G_CALLBACK (on_item_list_view_columns_changed), ilv);
	g_signal_connect (G_OBJECT (ilv->treeview), "row_activated", G_CALLBACK (on_item_list_row_activated), ilv);
	g_signal_connect (G_OBJECT (ilv->treeview), "key-press-event", G_CALLBACK (on_item_list_view_key_press_event), ilv);
	g_signal_connect (G_OBJECT (ilv->treeview), "popup_menu", G_CALLBACK (on_item_list_view_popup_menu), ilv);

	if (!wide) {
		gtk_widget_set_has_tooltip (GTK_WIDGET (ilv->treeview), TRUE);
		g_signal_connect (G_OBJECT (ilv->treeview), "query-tooltip", G_CALLBACK (on_item_list_view_query_tooltip), headline_column);
	}

	return ilv;
}

static void
item_list_view_add_item_to_tree_store (ItemListView *ilv, GtkTreeStore *itemstore, itemPtr item)
{
	gint		state = 0;
	nodePtr		node;
	GtkTreeIter	iter;
	gboolean	exists;

	if (item->flagStatus)
		state += 2;
	if (!item->readStatus)
		state += 1;

	node = node_from_id (item->nodeId);
	if(!node)
		return;	/* comment items do cause this... maybe filtering them earlier would be a good idea... */

	exists = item_list_view_id_to_iter (ilv, item->id, &iter);

	if (!exists)
	{
		gtk_tree_store_prepend (itemstore, &iter, NULL);
		ilv->item_ids = g_slist_append (ilv->item_ids, GUINT_TO_POINTER (item->id));
	}

	gtk_tree_store_set (itemstore, &iter,
		                       IS_TIME, item->time,
		                       IS_NR, item->id,
				       IS_PARENT, node,
		                       IS_FAVICON, node_get_icon (node),
		                       IS_ENCICON, item->hasEnclosure?icon_get (ICON_ENCLOSURE):NULL,
				       IS_ENCLOSURE, item->hasEnclosure,
				       IS_SOURCE, node,
				       IS_STATE, state,
		                       -1);
}

void
item_list_view_add_item (ItemListView *ilv, itemPtr item)
{
	GtkTreeStore	*itemstore;

	if (ilv->batch_mode) {
		/* either merge to new unattached GtkTreeStore */
		item_list_view_add_item_to_tree_store (ilv, ilv->batch_itemstore, item);
	} else {
		/* or merge to visible item store */
		itemstore = GTK_TREE_STORE (gtk_tree_view_get_model (ilv->treeview));
		item_list_view_add_item_to_tree_store (ilv, itemstore, item);
	}

	item_list_view_update_item (ilv, item);
}

void
item_list_view_enable_favicon_column (ItemListView *ilv, gboolean enabled)
{
	gtk_tree_view_column_set_visible (g_hash_table_lookup(ilv->columns, "favicon"), enabled);

	// In wide view we want to save vertical space and hide the state column
	if (ilv->wideView)
		gtk_tree_view_column_set_visible (g_hash_table_lookup(ilv->columns, "state"), !enabled);
}

void
on_action_launch_item_in_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
	      launch_item (item, INTERNAL);
	      item_unload (item);
	}
}

void
on_action_launch_item_in_tab (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
	      launch_item (item, TAB);
	      item_unload (item);
	}
}

void
on_action_launch_item_in_external_browser (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
	      launch_item (item, EXTERNAL);
	      item_unload (item);
	}
}

/* menu callbacks */

void
on_toggle_item_flag (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
		itemlist_toggle_flag (item);
		item_unload (item);
	}
}

void
on_toggle_unread_status (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
		itemlist_toggle_read_status (item);
		item_unload (item);
	}
}

void
on_remove_items_activate (GSimpleAction *action, GVariant *parameter, gpointer user_data)
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
on_action_remove_item (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item = NULL;
	if (parameter)
		item = item_load (g_variant_get_uint64 (parameter));
	else
		item = itemlist_get_selected ();

	if (item) {
		itemview_select_item (NULL);
		itemlist_remove_item (item);
	} else {
		liferea_shell_set_important_status_bar (_("No item has been selected"));
	}
}

void
item_list_view_select (ItemListView *ilv, itemPtr item)
{
	GtkTreeView		*treeview = ilv->treeview;
	GtkTreeSelection	*selection;
	GtkTreeIter		iter;

	selection = gtk_tree_view_get_selection (treeview);

	if (item && item_list_view_id_to_iter(ilv, item->id, &iter)){
		GtkTreePath	*path = NULL;

		path = gtk_tree_model_get_path (gtk_tree_view_get_model (treeview), &iter);
		if (path) {
			gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
			gtk_tree_view_scroll_to_cell (treeview, path, NULL, FALSE, 0.0, 0.0);
			gtk_tree_path_free (path);
		}
	} else {
		if (item)
			g_warning ("item_list_view_select : attempt to select an item which is not present in the view.");
		gtk_tree_selection_unselect_all (selection);
	}
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
			if (!item->readStatus)
				return item;
			item_unload (item);
		}
		valid = gtk_tree_model_iter_next (model, &iter);
	}

	return NULL;
}

void
on_next_unread_item_activate (GSimpleAction *menuitem, GVariant*parameter, gpointer user_data)
{
	itemlist_select_next_unread ();
}

void
on_popup_copy_URL_clipboard (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr         item;

	item = itemlist_get_selected ();
	if (item) {
		gchar *link = item_make_link (item);

		gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_PRIMARY), link, -1);
		gtk_clipboard_set_text (gtk_clipboard_get (GDK_SELECTION_CLIPBOARD), link, -1);

		g_free (link);
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
