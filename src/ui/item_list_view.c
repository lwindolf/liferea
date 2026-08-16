/*
 * @file item_list_view.c  presenting items in a GtkListView
 *
 * Copyright (C) 2004-2026 Lars Windolf <lars.windolf@gmx.de>
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

/* ItemListEntry only ever caches the lightweight metadata needed for sorting
 * (id, time, state, casefolded title, source node). It never caches rendered
 * markup or teasers: all visual content is rendered ad-hoc in the list item
 * factory's bind callback (and on explicit refresh of a bound row), so that
 * expensive per-item rendering (e.g. teaser extraction) only ever happens for
 * items that are actually visible on screen. */
typedef struct _ItemListEntry {
	GObject		parent_instance;

	gulong		id;
	guint64		time;
	guint		state;
	gchar		*sort_label;	/*<< casefolded item title, NULL if item has no title */
	Node		*source;	/*<< not owned */
} ItemListEntry;

typedef struct _ItemListEntryClass {
	GObjectClass	parent_class;
} ItemListEntryClass;

G_DEFINE_TYPE (ItemListEntry, item_list_entry, G_TYPE_OBJECT);

static void
item_list_entry_finalize (GObject *object)
{
	ItemListEntry *self = (ItemListEntry *)object;

	g_free (self->sort_label);

	G_OBJECT_CLASS (item_list_entry_parent_class)->finalize (object);
}

static void
item_list_entry_class_init (ItemListEntryClass *klass)
{
	G_OBJECT_CLASS (klass)->finalize = item_list_entry_finalize;
}

static void
item_list_entry_init (ItemListEntry *self)
{
}

static ItemListEntry *
item_list_entry_new (gulong id)
{
	ItemListEntry *entry = g_object_new (item_list_entry_get_type (), NULL);
	entry->id = id;
	return entry;
}

struct _ItemListView {
	GObject		parentInstance;

	GtkEventController *keypress;
	GtkGesture	*gesture;
	GtkGesture	*popup_gesture;
	GtkGesture	*middle_gesture;

	GtkListView	*listview;
	GtkWidget 	*ilscrolledwindow;	/*<< The complete ItemListView widget */

	GListStore		*base_model;		/*<< unsorted store of ItemListEntry, one per known item */
	GtkSortListModel	*sort_model;		/*<< sorted view over base_model */
	GtkSorter		*sorter;		/*<< custom sorter comparing cached ItemListEntry fields */
	GtkSingleSelection	*selection_model;
	GtkListItemFactory	*factory;

	GHashTable	*entries_by_id;		/*<< gulong id -> ItemListEntry* (borrowed, owned by base_model) */

	gboolean	batch_mode;
	gboolean	wideView;

	nodeViewSortType	sort_type;
	gboolean	sort_reversed;
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

static ItemListEntry *
item_list_view_id_to_entry (ItemListView *ilv, gulong id)
{
	return g_hash_table_lookup (ilv->entries_by_id, GUINT_TO_POINTER (id));
}

static gfloat
item_list_title_alignment (gchar *title)
{
	if (!title || strlen (title) == 0)
		return 0.;

	int txt_direction = common_find_base_dir (title, -1);
	int app_direction = gtk_widget_get_default_direction ();
	if ((txt_direction == PANGO_DIRECTION_LTR &&
	     app_direction == GTK_TEXT_DIR_LTR) ||
	    (txt_direction == PANGO_DIRECTION_RTL &&
	     app_direction == GTK_TEXT_DIR_RTL))
		return 0.;
	else
		return 1.;
}

static gchar *
item_list_truncate_utf8 (const gchar *text, guint max_chars)
{
	if (!text)
		return g_strdup ("");

	gsize len = g_utf8_strlen (text, -1);
	if (len <= max_chars)
		return g_strdup (text);

	const gchar *end = g_utf8_offset_to_pointer (text, max_chars);
	gchar *truncated = g_strndup (text, end - text);
	gchar *result = g_strconcat (truncated, "...", NULL);
	g_free (truncated);
	return result;
}

/**
 * item_list_view_render_row:
 *
 * Ad-hoc renders a single bound row widget from a freshly loaded item.
 * This is intentionally never precomputed/cached: it is only ever called
 * from the list item factory bind callback (when a row scrolls into view)
 * or when explicitly refreshing a currently visible row, so that expensive
 * per-item work (teaser extraction, markup building) only ever happens for
 * items that are actually on screen.
 */
static void
item_list_view_render_row (ItemListView *ilv, GtkWidget *box, itemPtr item, Node *node)
{
	GtkWidget *state_image = g_object_get_data (G_OBJECT (box), "state_image");
	GtkWidget *favicon_image = g_object_get_data (G_OBJECT (box), "favicon_image");
	GtkWidget *headline_label = g_object_get_data (G_OBJECT (box), "headline_label");
	GtkWidget *preview_label = g_object_get_data (G_OBJECT (box), "preview_label");
	GtkWidget *date_label = g_object_get_data (G_OBJECT (box), "date_label");
	Node *selected = feedlist_get_selected ();
	gchar *plain_title;
	gchar *escaped_title;
	gchar *preview_markup;
	gchar *time_str;
	gchar *time_str_escaped;
	gchar *headline_markup;
	gchar *title_limited;
	gchar *title_limited_escaped;
	gchar *tmp = NULL;
	const GIcon *state_icon;
	gboolean no_title = FALSE;

	time_str = (0 != item->time) ? date_format ((time_t)item->time, NULL) : g_strdup ("");
	time_str_escaped = g_markup_escape_text (time_str, -1);

	if (item->title && strlen (item->title)) {
		escaped_title = g_markup_escape_text (item->title, -1);
		g_strstrip (escaped_title);
		plain_title = g_strdup (escaped_title);
		title_limited = item_list_truncate_utf8 (item->title, 100);
		title_limited_escaped = g_markup_escape_text (title_limited, -1);
		g_strstrip (title_limited_escaped);
	} else {
		tmp = item_get_teaser (item);
		plain_title = g_strdup_printf ("%s...", tmp ? tmp : "");
		escaped_title = g_markup_escape_text (plain_title, -1);
		title_limited = item_list_truncate_utf8 (plain_title, 100);
		title_limited_escaped = g_markup_escape_text (title_limited, -1);
		g_free (tmp);
		no_title = TRUE;
	}

	if (ilv->wideView) {
		const gchar *important = item->flagStatus ? _(" <span background='red' color='white'> important </span> ") : "";
		gchar *teaser = NULL;
		gchar *teaser_markup = NULL;

		if (!no_title)
			teaser = item_get_teaser (item);
		if (teaser)
			teaser_markup = g_markup_escape_text (teaser, -1);

		headline_markup = g_strdup_printf (
			"<span weight='%s' size='larger'>%s</span>",
			item->readStatus ? "normal" : "bold",
			title_limited_escaped);

		preview_markup = g_strdup_printf (
			"<span weight='%s'>%s%s</span>%s<span size='smaller' weight='ultralight'> — %s</span>",
			item->readStatus ? "ultralight" : "light",
			teaser_markup ? teaser_markup : "",
			teaser_markup ? "..." : "",
			important,
			time_str_escaped);
		g_free (teaser_markup);
		g_free (teaser);
	} else {
		headline_markup = g_strdup_printf ("<span weight='%s'>%s</span>",
		                               item->readStatus ? "normal" : "bold",
		                               escaped_title);
		preview_markup = g_strdup ("");
	}

	state_icon = item->flagStatus ? icon_get (ICON_FLAG) :
	             !item->readStatus ? icon_get (ICON_UNREAD) :
	             NULL;

	gtk_label_set_markup (GTK_LABEL (headline_label), headline_markup);
	gtk_label_set_markup (GTK_LABEL (preview_label), preview_markup);
	gtk_label_set_xalign (GTK_LABEL (headline_label), item_list_title_alignment (plain_title));
	gtk_label_set_xalign (GTK_LABEL (preview_label), item_list_title_alignment (plain_title));
	gtk_label_set_text (GTK_LABEL (date_label), time_str);

	if (state_icon)
		gtk_image_set_from_gicon (GTK_IMAGE (state_image), (GIcon *)state_icon);
	else
		gtk_image_clear (GTK_IMAGE (state_image));

	if (node)
		gtk_image_set_from_gicon (GTK_IMAGE (favicon_image), node_get_icon (node));
	else
		gtk_image_clear (GTK_IMAGE (favicon_image));

	if (ilv->wideView) {
		gtk_image_set_icon_size (GTK_IMAGE (favicon_image), GTK_ICON_SIZE_LARGE);
		gtk_widget_set_margin_start (favicon_image, 6);
		gtk_widget_set_margin_end (favicon_image, 6);
		gtk_widget_set_margin_top (box, 6);
		gtk_widget_set_margin_bottom (box, 6);
		gtk_label_set_wrap (GTK_LABEL (headline_label), TRUE);
		gtk_label_set_wrap_mode (GTK_LABEL (headline_label), PANGO_WRAP_WORD_CHAR);
		gtk_label_set_ellipsize (GTK_LABEL (headline_label), PANGO_ELLIPSIZE_NONE);

		gtk_label_set_wrap (GTK_LABEL (preview_label), TRUE);
		gtk_label_set_wrap_mode (GTK_LABEL (preview_label), PANGO_WRAP_WORD_CHAR);
		gtk_label_set_ellipsize (GTK_LABEL (preview_label), PANGO_ELLIPSIZE_NONE);
	} else {
		gtk_image_set_icon_size (GTK_IMAGE (favicon_image), GTK_ICON_SIZE_NORMAL);
		gtk_widget_set_margin_start (favicon_image, 6);
		gtk_widget_set_margin_end (favicon_image, 6);
		gtk_widget_set_margin_top (box, 2);
		gtk_widget_set_margin_bottom (box, 2);
		gtk_label_set_wrap (GTK_LABEL (headline_label), FALSE);
		gtk_label_set_wrap_mode (GTK_LABEL (headline_label), PANGO_WRAP_NONE);
		gtk_label_set_ellipsize (GTK_LABEL (headline_label), PANGO_ELLIPSIZE_END);

		gtk_label_set_wrap (GTK_LABEL (preview_label), FALSE);
		gtk_label_set_wrap_mode (GTK_LABEL (preview_label), PANGO_WRAP_NONE);
		gtk_label_set_ellipsize (GTK_LABEL (preview_label), PANGO_ELLIPSIZE_NONE);
	}

	gtk_widget_set_visible (favicon_image, !(selected && !selected->children && !IS_VFOLDER(selected)));
	gtk_widget_set_visible (date_label, !ilv->wideView);
	gtk_widget_set_visible (preview_label, ilv->wideView);
	gtk_widget_set_visible (state_image, !ilv->wideView);

	gtk_widget_set_tooltip_text (headline_label, plain_title);

	g_free (headline_markup);
	g_free (preview_markup);
	g_free (title_limited_escaped);
	g_free (title_limited);
	g_free (time_str_escaped);
	g_free (escaped_title);
	g_free (plain_title);
	g_free (time_str);
}

static gboolean
item_list_view_refresh_bound_row (ItemListView *ilv, GtkWidget *widget, gulong id)
{
	for (GtkWidget *child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child)) {
		if (item_list_view_refresh_bound_row (ilv, child, id))
			return TRUE;
	}

	gpointer data = g_object_get_data (G_OBJECT (widget), "item-id");
	if (!data || (gulong) GPOINTER_TO_SIZE (data) != id)
		return FALSE;

	itemPtr item = item_load (id);
	if (item) {
		ItemListEntry *entry = item_list_view_id_to_entry (ilv, id);
		item_list_view_render_row (ilv, widget, item, entry ? entry->source : NULL);
		item_unload (item);
	}

	return TRUE;
}

static void
item_list_view_refresh_all_visible_rows (ItemListView *ilv, GtkWidget *widget)
{
	gpointer data = g_object_get_data (G_OBJECT (widget), "item-id");

	if (data) {
		gulong id = (gulong) GPOINTER_TO_SIZE (data);
		itemPtr item = item_load (id);

		if (item) {
			ItemListEntry *entry = item_list_view_id_to_entry (ilv, id);
			item_list_view_render_row (ilv, widget, item, entry ? entry->source : NULL);
			item_unload (item);
		}
	}

	for (GtkWidget *child = gtk_widget_get_first_child (widget); child; child = gtk_widget_get_next_sibling (child))
		item_list_view_refresh_all_visible_rows (ilv, child);
}

static nodeViewSortType
item_list_view_effective_sort_type (ItemListView *ilv)
{
	if (ilv->sort_type == NODE_VIEW_SORT_BY_TITLE && ilv->wideView)
		return NODE_VIEW_SORT_BY_TIME;

	return ilv->sort_type;
}

static gint
item_list_view_cmp_entries (ItemListView *ilv, const ItemListEntry *a, const ItemListEntry *b)
{
	nodeViewSortType sort_type = item_list_view_effective_sort_type (ilv);
	gint cmp = 0;

	switch (sort_type) {
		case NODE_VIEW_SORT_BY_TITLE:
			cmp = g_strcmp0 (a->sort_label, b->sort_label);
			break;
		case NODE_VIEW_SORT_BY_PARENT:
			if (!a->source || !a->source->id || !b->source || !b->source->id)
				cmp = 0;
			else
				cmp = strcmp (a->source->id, b->source->id);
			break;
		case NODE_VIEW_SORT_BY_STATE:
			cmp = (gint)a->state - (gint)b->state;
			break;
		case NODE_VIEW_SORT_BY_TIME:
		default:
			if (a->time > b->time)
				cmp = 1;
			else if (a->time < b->time)
				cmp = -1;
			else
				cmp = 0;
			break;
	}

	if (cmp == 0) {
		if (a->time > b->time)
			cmp = 1;
		else if (a->time < b->time)
			cmp = -1;
		else
			cmp = (a->id < b->id) ? 1 : (a->id > b->id ? -1 : 0);
	}

	if (ilv->sort_reversed)
		cmp = -cmp;

	return cmp;
}

static gint
item_list_view_sort_func (gconstpointer a, gconstpointer b, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);

	return item_list_view_cmp_entries (ilv, (const ItemListEntry *)a, (const ItemListEntry *)b);
}

static void
item_list_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ItemListView *ilv = ITEM_LIST_VIEW (object);

	switch (prop_id) {
		case PROP_WIDE_VIEW:
			ilv->wideView = g_value_get_boolean (value);
			item_list_view_refresh_all_visible_rows (ilv, GTK_WIDGET (ilv->listview));
			gtk_sorter_changed (ilv->sorter, GTK_SORTER_CHANGE_DIFFERENT);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
item_list_view_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
	ItemListView *ilv = ITEM_LIST_VIEW (object);

	switch (prop_id) {
		case PROP_WIDE_VIEW:
			g_value_set_boolean (value, ilv->wideView);
			break;
		default:
			G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
			break;
	}
}

static void
item_list_view_finalize (GObject *object)
{
	ItemListView *ilv = ITEM_LIST_VIEW (object);

	g_signal_handlers_disconnect_by_data (G_OBJECT (ilv->selection_model), object);
	g_signal_handlers_disconnect_by_data (G_OBJECT (ilv->listview), object);
	g_signal_handlers_disconnect_by_data (G_OBJECT (ilv->factory), object);

	g_hash_table_destroy (ilv->entries_by_id);

	g_clear_object (&ilv->selection_model);
	g_clear_object (&ilv->sort_model);
	g_clear_object (&ilv->sorter);
	g_clear_object (&ilv->base_model);
	g_clear_object (&ilv->factory);

	if (ilv->ilscrolledwindow)
		g_object_unref (ilv->ilscrolledwindow);

	g_object_unref (ilv->gesture);
	g_object_unref (ilv->popup_gesture);
	g_object_unref (ilv->middle_gesture);
	g_object_unref (ilv->keypress);

	G_OBJECT_CLASS (item_list_view_parent_class)->finalize (object);
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

static void
item_list_view_clear_rows (ItemListView *ilv)
{
	gtk_single_selection_set_selected (ilv->selection_model, GTK_INVALID_LIST_POSITION);
	g_list_store_remove_all (ilv->base_model);
	g_hash_table_remove_all (ilv->entries_by_id);
}

static void
on_itemlist_selection_changed (GtkSingleSelection *selection_model, GParamSpec *pspec, gpointer user_data)
{
	ItemListEntry *entry = (ItemListEntry *) gtk_single_selection_get_selected_item (selection_model);
	gulong id = entry ? entry->id : 0;

	g_signal_emit_by_name (user_data, "selection-changed", id);
}

void
item_list_view_set_sort_column (ItemListView *ilv, nodeViewSortType sortType, gboolean sortReversed)
{
	ilv->sort_type = sortType;
	ilv->sort_reversed = sortReversed;
	gtk_sorter_changed (ilv->sorter, GTK_SORTER_CHANGE_DIFFERENT);
}

static guint
item_list_view_find_view_position (ItemListView *ilv, gulong id)
{
	guint n_items = g_list_model_get_n_items (G_LIST_MODEL (ilv->sort_model));

	for (guint i = 0; i < n_items; i++) {
		ItemListEntry *entry = g_list_model_get_item (G_LIST_MODEL (ilv->sort_model), i);
		gboolean match = entry && entry->id == id;

		if (entry)
			g_object_unref (entry);
		if (match)
			return i;
	}

	return GTK_INVALID_LIST_POSITION;
}

static void
item_list_view_select_id (ItemListView *ilv, gulong id)
{
	guint position = item_list_view_find_view_position (ilv, id);

	if (position == GTK_INVALID_LIST_POSITION) {
		debug (DEBUG_GUI, "item_list_view_select: ignore missing item id %lu in current view", id);
		return;
	}

	gtk_single_selection_set_selected (ilv->selection_model, position);
	gtk_list_view_scroll_to (ilv->listview, position, GTK_LIST_SCROLL_FOCUS, NULL);
}

static void
item_list_view_all_items_removed (GObject *obj, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);

	debug (DEBUG_CACHE, "item_list_view_all_items_removed()");

	item_list_view_clear_rows (ilv);
}

static void
item_list_view_item_removed (GObject *obj, gulong id, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	ItemListEntry *entry = item_list_view_id_to_entry (ilv, id);
	guint view_position;
	gboolean was_selected;
	gulong next_id = 0;
	guint index;

	if (!entry) {
		debug (DEBUG_GUI, "item id %lu to be removed not found in item id list!", id);
		return;
	}

	view_position = item_list_view_find_view_position (ilv, id);
	was_selected = (view_position != GTK_INVALID_LIST_POSITION) &&
	               (gtk_single_selection_get_selected (ilv->selection_model) == view_position);

	if (was_selected) {
		guint n_items = g_list_model_get_n_items (G_LIST_MODEL (ilv->sort_model));
		guint neighbor_pos = view_position + 1;

		if (neighbor_pos >= n_items)
			neighbor_pos = (view_position > 0) ? view_position - 1 : GTK_INVALID_LIST_POSITION;

		if (neighbor_pos != GTK_INVALID_LIST_POSITION) {
			ItemListEntry *neighbor = g_list_model_get_item (G_LIST_MODEL (ilv->sort_model), neighbor_pos);

			if (neighbor) {
				next_id = neighbor->id;
				g_object_unref (neighbor);
			}
		}
	}

	if (g_list_store_find (ilv->base_model, entry, &index))
		g_list_store_remove (ilv->base_model, index);

	g_hash_table_remove (ilv->entries_by_id, GUINT_TO_POINTER (id));

	if (was_selected) {
		if (next_id)
			item_list_view_select_id (ilv, next_id);
		else
			gtk_single_selection_set_selected (ilv->selection_model, GTK_INVALID_LIST_POSITION);
	}
}

static void
item_list_view_item_batch_started (GObject *obj, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);

	item_list_view_clear_rows (ilv);

	ilv->batch_mode = TRUE;
}

static void
item_list_view_item_batch_ended (GObject *obj, gpointer *n, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	Node *node = (Node *)n;
	guint n_items;

	g_assert (ilv->batch_mode);

	item_list_view_set_sort_column (ilv, node->sortColumn, node->sortReversed);
	ilv->batch_mode = FALSE;

	n_items = g_list_model_get_n_items (G_LIST_MODEL (ilv->sort_model));
	if (n_items > 0)
		gtk_list_view_scroll_to (ilv->listview, 0, GTK_LIST_SCROLL_NONE, NULL);
}

static void
item_list_view_entry_update_fields (ItemListEntry *entry, itemPtr item, Node *node)
{
	guint state = 0;

	if (item->flagStatus)
		state += 2;
	if (!item->readStatus)
		state += 1;

	entry->time = item->time;
	entry->state = state;
	if (node)
		entry->source = node;

	g_free (entry->sort_label);
	entry->sort_label = NULL;
	if (item->title && strlen (item->title)) {
		gchar *stripped = g_strdup (item->title);
		g_strstrip (stripped);
		entry->sort_label = g_utf8_casefold (stripped, -1);
		g_free (stripped);
	}
}

void
item_list_view_update_item (ItemListView *ilv, itemPtr item)
{
	ItemListEntry *entry;

	if (!item)
		return;

	entry = item_list_view_id_to_entry (ilv, item->id);
	if (!entry)
		return;

	item_list_view_entry_update_fields (entry, item, entry->source);

	if (!ilv->batch_mode)
		gtk_sorter_changed (ilv->sorter, GTK_SORTER_CHANGE_DIFFERENT);

	item_list_view_refresh_bound_row (ilv, GTK_WIDGET (ilv->listview), item->id);
}

static void
item_list_view_item_updated (GObject *obj, gint itemId, gpointer user_data)
{
	itemPtr item = item_load (itemId);
	item_list_view_update_item (ITEM_LIST_VIEW (user_data), item);
	item_unload (item);
}

static void
item_list_view_update_all_items (GObject *obj, const gchar *nodeId, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	guint n_items = g_list_model_get_n_items (G_LIST_MODEL (ilv->base_model));

	for (guint i = 0; i < n_items; i++) {
		ItemListEntry *entry = g_list_model_get_item (G_LIST_MODEL (ilv->base_model), i);
		itemPtr item;

		if (!entry)
			continue;

		item = item_load (entry->id);
		if (item) {
			item_list_view_entry_update_fields (entry, item, entry->source);
			item_unload (item);
		}

		g_object_unref (entry);
	}

	item_list_view_refresh_all_visible_rows (ilv, GTK_WIDGET (ilv->listview));
	gtk_sorter_changed (ilv->sorter, GTK_SORTER_CHANGE_DIFFERENT);
}

static gboolean
on_item_list_view_key_pressed_event (GtkEventControllerKey *controller, guint keyval, guint keycode, GdkModifierType state, gpointer user_data)
{
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
	GMenu *menu = g_menu_new ();
	GMenuItem *menu_item;
	gchar *text, *item_link;
	const gchar *author;

	item_link = item_make_link (item);
	author = item_get_author (item);

	GMenu *section = g_menu_new ();
	menu_item = g_menu_item_new (NULL, NULL);

	g_menu_item_set_label (menu_item, _("Open In _Tab"));
	g_menu_item_set_action_and_target (menu_item, "app.open-item-in-tab", "t", (guint64)item->id);
	g_menu_append_item (section, menu_item);

	g_menu_item_set_label (menu_item, _("_Open In Browser"));
	g_menu_item_set_action_and_target (menu_item, "app.open-item-in-browser", "t", (guint64)item->id);
	g_menu_append_item (section, menu_item);

	g_menu_item_set_label (menu_item, _("Open In _External Browser"));
	g_menu_item_set_action_and_target (menu_item, "app.open-item-in-external-browser", "t", (guint64)item->id);
	g_menu_append_item (section, menu_item);

	if (author) {
		g_menu_item_set_label (menu_item, _("Email The Author"));
		g_menu_item_set_action_and_target (menu_item, "app.email-the-author", "t", (guint64)item->id);
		g_menu_append_item (section, menu_item);
	}

	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	GSList *iter = newsbin_get_list ();
	if (iter) {
		GMenu *submenu;
		guint32 i = 0;

		section = g_menu_new ();
		submenu = g_menu_new ();

		while (iter) {
			Node *node = (Node *)iter->data;
			g_menu_item_set_label (menu_item, node_get_title (node));
			g_menu_item_set_action_and_target (menu_item, "app.copy-item-to-newsbin", "(ut)", i, (guint64)item->id);
			g_menu_append_item (submenu, menu_item);
			iter = g_slist_next (iter);
			i++;
		}

		g_menu_append_submenu (section, _("Copy to News Bin"), G_MENU_MODEL (submenu));
		g_object_unref (submenu);
		g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
	}

	section = g_menu_new ();

	text = g_strdup_printf (_("_Bookmark at %s"), social_get_bookmark_site ());
	g_menu_item_set_label (menu_item, text);
	g_menu_item_set_action_and_target (menu_item, "app.social-bookmark-link", "(ss)", item_link, item_get_title (item));
	g_menu_append_item (section, menu_item);
	g_free (text);

	g_menu_item_set_label (menu_item, _("Copy Item _Location"));
	g_menu_item_set_action_and_target (menu_item, "app.copy-link-to-clipboard", "s", item_link);
	g_menu_append_item (section, menu_item);

	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	section = g_menu_new ();

	g_menu_item_set_label (menu_item, _("Toggle _Read Status"));
	g_menu_item_set_action_and_target (menu_item, "app.toggle-item-read-status", "t", (guint64)item->id);
	g_menu_append_item (section, menu_item);

	g_menu_item_set_label (menu_item, _("Toggle Item _Flag"));
	g_menu_item_set_action_and_target (menu_item, "app.toggle-item-flag", "t", (guint64)item->id);
	g_menu_append_item (section, menu_item);

	g_menu_item_set_label (menu_item, _("R_emove Item"));
	g_menu_item_set_action_and_target (menu_item, "app.remove-item", "t", (guint64)item->id);
	g_menu_append_item (section, menu_item);

	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	section = g_menu_new ();

	GMenu *sort_submenu = g_menu_new ();
	g_menu_append (sort_submenu, _("By _Date"), "app.sort-items-by-time");
	g_menu_append (sort_submenu, _("By _Title"), "app.sort-items-by-title");
	g_menu_append (sort_submenu, _("By _Feed"), "app.sort-items-by-source");
	g_menu_append (sort_submenu, _("By _Status"), "app.sort-items-by-state");
	g_menu_append (sort_submenu, _("_Reverse Order"), "app.sort-items-reverse");
	g_menu_append_submenu (section, _("_Sort"), G_MENU_MODEL (sort_submenu));
	g_object_unref (sort_submenu);

	g_menu_append_section (menu, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	g_object_unref (menu_item);
	g_free (item_link);

	return menu;
}

static gboolean
item_list_view_widget_is_descendant (GtkWidget *widget, GtkWidget *ancestor)
{
	for (GtkWidget *iter = widget; iter; iter = gtk_widget_get_parent (iter)) {
		if (iter == ancestor)
			return TRUE;
	}

	return FALSE;
}

static GtkWidget *
item_list_view_find_bound_box_at_coords (ItemListView *ilv, gdouble x, gdouble y, gulong *id_out)
{
	GtkWidget *picked = gtk_widget_pick (GTK_WIDGET (ilv->listview), x, y, GTK_PICK_DEFAULT);

	for (GtkWidget *iter = picked; iter; iter = gtk_widget_get_parent (iter)) {
		gpointer data = g_object_get_data (G_OBJECT (iter), "item-id");
		if (data) {
			if (id_out)
				*id_out = (gulong) GPOINTER_TO_SIZE (data);
			return iter;
		}
	}

	return NULL;
}

static void
on_item_list_view_pressed_event (GtkGestureClick *gesture, guint n_press, gdouble x, gdouble y, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	gulong id = 0;
	GtkWidget *box = item_list_view_find_bound_box_at_coords (ilv, x, y, &id);
	itemPtr item;

	if (!box)
		return;

	item = item_load (id);
	if (!item)
		return;

	if (n_press == 1) {
		switch (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture))) {
			case GDK_BUTTON_PRIMARY: {
				GtkWidget *picked = gtk_widget_pick (GTK_WIDGET (ilv->listview), x, y, GTK_PICK_DEFAULT);
				GtkWidget *favicon_image = g_object_get_data (G_OBJECT (box), "favicon_image");
				GtkWidget *state_image = g_object_get_data (G_OBJECT (box), "state_image");
				if (picked &&
				    (item_list_view_widget_is_descendant (picked, favicon_image) ||
				     item_list_view_widget_is_descendant (picked, state_image))) {
                                        gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
					itemlist_toggle_flag (item);
				}
				break;
			}
			case GDK_BUTTON_MIDDLE:
				gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
				itemlist_toggle_read_status (item);
				break;
			case GDK_BUTTON_SECONDARY: {
				GMenu *menu = item_list_view_popup_menu (ilv, item);
				GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
				GtkWidget *anchor = gtk_widget_get_parent (GTK_WIDGET (ilv->listview));
				GdkRectangle rect;
				graphene_point_t src = GRAPHENE_POINT_INIT ((float)x, (float)y);
				graphene_point_t dst;

				if (!anchor)
					anchor = GTK_WIDGET (ilv->listview);

				gtk_widget_set_parent (popover, anchor);

				if (anchor != GTK_WIDGET (ilv->listview) &&
				    gtk_widget_compute_point (GTK_WIDGET (ilv->listview), anchor, &src, &dst)) {
					rect.x = (int)dst.x;
					rect.y = (int)dst.y;
				} else {
					rect.x = (int)x;
					rect.y = (int)y;
				}
				rect.width = 1;
				rect.height = 1;
				gtk_popover_set_pointing_to (GTK_POPOVER (popover), &rect);
				gtk_popover_popup (GTK_POPOVER (popover));
				g_object_unref (menu);
				break;
			}
		}
	}

	item_unload (item);
}

static void
on_item_list_row_activated (GtkListView *listview, guint position, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	ItemListEntry *entry = g_list_model_get_item (G_LIST_MODEL (ilv->sort_model), position);
	itemPtr item;

	if (!entry)
		return;

	item = item_load (entry->id);
	g_object_unref (entry);
	if (!item)
		return;

	browser_launch_item (item, BROWSER_LAUNCH_DEFAULT);
	item_unload (item);
}

GtkWidget *
item_list_view_get_widget (ItemListView *ilv)
{
	return ilv->ilscrolledwindow;
}

void
item_list_view_move_cursor (ItemListView *ilv, int step)
{
	guint n_items = g_list_model_get_n_items (G_LIST_MODEL (ilv->sort_model));
	guint selected = gtk_single_selection_get_selected (ilv->selection_model);
	gint index = (selected != GTK_INVALID_LIST_POSITION) ? (gint)selected : 0;
	gint target = index + step;

	if (selected == GTK_INVALID_LIST_POSITION && step < 0)
		target = G_MAXINT;
	if (target < 0)
		target = 0;

	if ((guint)target >= n_items) {
		if (target > 0 && (guint)(target - 1) < n_items)
			target = target - 1;
		else
			return;
	}

	gtk_single_selection_set_selected (ilv->selection_model, (guint)target);
}

void
item_list_view_move_cursor_to_first (ItemListView *ilv)
{
	if (g_list_model_get_n_items (G_LIST_MODEL (ilv->sort_model)) > 0)
		gtk_single_selection_set_selected (ilv->selection_model, 0);
}

static void
item_list_view_factory_setup_cb (GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *text_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	GtkWidget *header_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *state_image = gtk_image_new ();
	GtkWidget *favicon_image = gtk_image_new ();
	GtkWidget *headline_label = gtk_label_new (NULL);
	GtkWidget *preview_label = gtk_label_new (NULL);
	GtkWidget *date_label = gtk_label_new (NULL);

	gtk_label_set_use_markup (GTK_LABEL (headline_label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (headline_label), 0.0);
	gtk_label_set_wrap (GTK_LABEL (headline_label), FALSE);
	gtk_label_set_ellipsize (GTK_LABEL (headline_label), PANGO_ELLIPSIZE_END);

	gtk_label_set_use_markup (GTK_LABEL (preview_label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (preview_label), 0.0);
	gtk_label_set_wrap (GTK_LABEL (preview_label), TRUE);
	gtk_label_set_wrap_mode (GTK_LABEL (preview_label), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_ellipsize (GTK_LABEL (preview_label), PANGO_ELLIPSIZE_NONE);
	gtk_widget_add_css_class (preview_label, "dim-label");

	gtk_label_set_xalign (GTK_LABEL (date_label), 1.0);
	gtk_widget_set_halign (date_label, GTK_ALIGN_END);
	gtk_widget_set_hexpand (date_label, FALSE);
	gtk_widget_add_css_class (date_label, "dim-label");

	gtk_widget_set_halign (state_image, GTK_ALIGN_START);
	gtk_widget_set_halign (favicon_image, GTK_ALIGN_START);
	gtk_widget_set_hexpand (headline_label, TRUE);
	gtk_widget_set_hexpand (preview_label, TRUE);

	gtk_box_append (GTK_BOX (header_box), headline_label);
	gtk_box_append (GTK_BOX (header_box), date_label);
	gtk_box_append (GTK_BOX (text_box), header_box);
	gtk_box_append (GTK_BOX (text_box), preview_label);

	gtk_box_append (GTK_BOX (box), state_image);
	gtk_box_append (GTK_BOX (box), favicon_image);
	gtk_box_append (GTK_BOX (box), text_box);

	gtk_widget_set_margin_start (box, 6);
	gtk_widget_set_margin_end (box, 6);

	g_object_set_data (G_OBJECT (box), "state_image", state_image);
	g_object_set_data (G_OBJECT (box), "favicon_image", favicon_image);
	g_object_set_data (G_OBJECT (box), "headline_label", headline_label);
	g_object_set_data (G_OBJECT (box), "preview_label", preview_label);
	g_object_set_data (G_OBJECT (box), "date_label", date_label);

	gtk_list_item_set_child (list_item, box);
}

static void
item_list_view_factory_bind_cb (GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	ItemListEntry *entry = gtk_list_item_get_item (list_item);
	GtkWidget *box = gtk_list_item_get_child (list_item);
	itemPtr item;

	if (!entry || !box)
		return;

	g_object_set_data (G_OBJECT (box), "item-id", GSIZE_TO_POINTER ((gsize) entry->id));

	item = item_load (entry->id);
	if (item) {
		item_list_view_render_row (ilv, box, item, entry->source);
		item_unload (item);
	}
}

static void
item_list_view_factory_unbind_cb (GtkListItemFactory *factory, GtkListItem *list_item, gpointer user_data)
{
	GtkWidget *box = gtk_list_item_get_child (list_item);

	if (box)
		g_object_set_data (G_OBJECT (box), "item-id", NULL);
}

static void
item_list_view_add_item (ItemListView *ilv, itemPtr item, Node *node)
{
	ItemListEntry *entry = item_list_view_id_to_entry (ilv, item->id);

	if (!entry) {
		entry = item_list_entry_new (item->id);
		item_list_view_entry_update_fields (entry, item, node);
		g_list_store_append (ilv->base_model, entry);
		g_hash_table_insert (ilv->entries_by_id, GUINT_TO_POINTER (item->id), entry);
		g_object_unref (entry);
	} else {
		item_list_view_entry_update_fields (entry, item, node);
		item_list_view_refresh_bound_row (ilv, GTK_WIDGET (ilv->listview), item->id);
	}
}

static void
item_list_view_item_added (GObject *obj, gint itemId, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	itemPtr item = item_load (itemId);
	Node *node;

	if (!item)
		return;

	node = node_from_id (item->nodeId);
	if (node)
		item_list_view_add_item (ilv, item, node);

	item_unload (item);

	if (!ilv->batch_mode)
		gtk_sorter_changed (ilv->sorter, GTK_SORTER_CHANGE_DIFFERENT);
}

static void
item_list_view_select (GObject *obj, gint id, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);

	if (id)
		item_list_view_select_id (ilv, (gulong) id);
	else
		gtk_single_selection_set_selected (ilv->selection_model, GTK_INVALID_LIST_POSITION);
}

static void
item_list_view_init (ItemListView *ilv)
{
}

ItemListView *
item_list_view_create (FeedList *feedlist, ItemList *itemlist)
{
	ItemListView *ilv;

	ilv = g_object_new (ITEM_LIST_VIEW_TYPE, NULL);
	ilv->wideView = FALSE;
	ilv->sort_type = NODE_VIEW_SORT_BY_TIME;
	ilv->sort_reversed = FALSE;

	ilv->entries_by_id = g_hash_table_new (g_direct_hash, g_direct_equal);

	ilv->keypress = gtk_event_controller_key_new ();
	ilv->gesture = gtk_gesture_click_new ();
	ilv->popup_gesture = gtk_gesture_click_new ();
	ilv->middle_gesture = gtk_gesture_click_new ();

	ilv->ilscrolledwindow = gtk_scrolled_window_new ();
	gtk_widget_set_vexpand (ilv->ilscrolledwindow, TRUE);
	g_object_ref_sink (ilv->ilscrolledwindow);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ilv->ilscrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	ilv->base_model = g_list_store_new (item_list_entry_get_type ());

	ilv->sorter = GTK_SORTER (gtk_custom_sorter_new ((GCompareDataFunc) item_list_view_sort_func, ilv, NULL));

	ilv->sort_model = gtk_sort_list_model_new (NULL, NULL);
	gtk_sort_list_model_set_model (ilv->sort_model, G_LIST_MODEL (ilv->base_model));
	gtk_sort_list_model_set_sorter (ilv->sort_model, ilv->sorter);

	ilv->selection_model = gtk_single_selection_new (NULL);
	gtk_single_selection_set_model (ilv->selection_model, G_LIST_MODEL (ilv->sort_model));
	gtk_single_selection_set_autoselect (ilv->selection_model, FALSE);
	gtk_single_selection_set_can_unselect (ilv->selection_model, TRUE);

	ilv->factory = gtk_signal_list_item_factory_new ();
	g_signal_connect (ilv->factory, "setup", G_CALLBACK (item_list_view_factory_setup_cb), ilv);
	g_signal_connect (ilv->factory, "bind", G_CALLBACK (item_list_view_factory_bind_cb), ilv);
	g_signal_connect (ilv->factory, "unbind", G_CALLBACK (item_list_view_factory_unbind_cb), ilv);

	ilv->listview = GTK_LIST_VIEW (gtk_list_view_new (NULL, NULL));
	gtk_list_view_set_model (ilv->listview, GTK_SELECTION_MODEL (ilv->selection_model));
	gtk_list_view_set_factory (ilv->listview, ilv->factory);
	gtk_list_view_set_single_click_activate (ilv->listview, FALSE);
	gtk_widget_set_name (GTK_WIDGET (ilv->listview), "itemlist");
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (ilv->ilscrolledwindow), GTK_WIDGET (ilv->listview));

	g_signal_connect (G_OBJECT (ilv->selection_model), "notify::selected-item", G_CALLBACK (on_itemlist_selection_changed), ilv);
	g_signal_connect (G_OBJECT (ilv->listview), "activate", G_CALLBACK (on_item_list_row_activated), ilv);

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (ilv->middle_gesture), GDK_BUTTON_MIDDLE);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (ilv->middle_gesture), GTK_PHASE_CAPTURE);
	g_signal_connect (ilv->middle_gesture, "pressed", G_CALLBACK (on_item_list_view_pressed_event), ilv);
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (ilv->popup_gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect (ilv->popup_gesture, "pressed", G_CALLBACK (on_item_list_view_pressed_event), ilv);
	g_signal_connect (ilv->gesture, "pressed", G_CALLBACK (on_item_list_view_pressed_event), ilv);
	g_signal_connect (ilv->keypress, "key-pressed", G_CALLBACK (on_item_list_view_key_pressed_event), ilv);

	gtk_widget_add_controller (GTK_WIDGET (ilv->listview), GTK_EVENT_CONTROLLER (ilv->middle_gesture));
	gtk_widget_add_controller (GTK_WIDGET (ilv->listview), GTK_EVENT_CONTROLLER (ilv->popup_gesture));
	gtk_widget_add_controller (GTK_WIDGET (ilv->listview), GTK_EVENT_CONTROLLER (ilv->gesture));
	gtk_widget_add_controller (GTK_WIDGET (ilv->listview), ilv->keypress);

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

gboolean
item_list_view_contains_id (ItemListView *ilv, gulong id)
{
	return (NULL != item_list_view_id_to_entry (ilv, id));
}

itemPtr
item_list_view_find_unread_item (ItemListView *ilv, gulong startId)
{
	guint n_items = g_list_model_get_n_items (G_LIST_MODEL (ilv->sort_model));
	guint index = 0;

	if (startId) {
		index = item_list_view_find_view_position (ilv, startId);
		if (index == GTK_INVALID_LIST_POSITION)
			return NULL;
	}

	for (; index < n_items; index++) {
		ItemListEntry *entry = g_list_model_get_item (G_LIST_MODEL (ilv->sort_model), index);
		gulong id;
		itemPtr item;

		if (!entry)
			continue;

		id = entry->id;
		g_object_unref (entry);

		item = item_load (id);
		if (item) {
			if (!item->readStatus && item->id != startId)
				return item;
			item_unload (item);
		}
	}

	return NULL;
}

void
on_popup_copy_URL_clipboard (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	itemPtr item;

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
	} else {
		liferea_shell_set_important_status_bar (_("No item has been selected"));
	}
}
