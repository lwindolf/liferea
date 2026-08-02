/*
 * @file item_list_view.c  presenting items in a GtkListBox
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

typedef struct {
	gulong		id;
	guint64		time;
	gchar		*sort_label;
	guint		state;
	GtkWidget	*row;
	GtkWidget	*state_image;
	GtkWidget	*favicon_image;
	GtkWidget	*headline_label;
	GtkWidget	*preview_label;
	GtkWidget	*date_label;
	Node		*source;
} ItemListRow;

struct _ItemListView {
	GObject		parentInstance;

	GtkEventController *keypress;
	GtkGesture	*gesture;
	GtkGesture	*popup_gesture;
	GtkGesture	*middle_gesture;

	GtkListBox	*listbox;
	GtkWidget 	*ilscrolledwindow;	/*<< The complete ItemListView widget */
	GSList		*item_ids;		/*<< list of all currently known item ids */
	GHashTable	*rows_by_id;		/*<< gulong id -> ItemListRow* */

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

static void
item_list_row_free (ItemListRow *row)
{
	g_free (row->sort_label);
	g_free (row);
}

static ItemListRow *
item_list_view_id_to_row (ItemListView *ilv, gulong id)
{
	return g_hash_table_lookup (ilv->rows_by_id, GUINT_TO_POINTER (id));
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

static void
item_list_view_apply_row_visibility (ItemListView *ilv, ItemListRow *row)
{
	Node *selected = feedlist_get_selected ();

	gtk_widget_set_visible (row->favicon_image, !(selected && !selected->children));
	gtk_widget_set_visible (row->date_label, !ilv->wideView);
	gtk_widget_set_visible (row->preview_label, ilv->wideView);
	gtk_widget_set_visible (row->state_image, !ilv->wideView);
}

static void
item_list_view_apply_row_layout (ItemListView *ilv, ItemListRow *row)
{
	GtkWidget *box = gtk_list_box_row_get_child (GTK_LIST_BOX_ROW (row->row));

	if (ilv->wideView) {
		gtk_image_set_icon_size (GTK_IMAGE (row->favicon_image), GTK_ICON_SIZE_LARGE);
		gtk_widget_set_margin_start (row->favicon_image, 6);
		gtk_widget_set_margin_end (row->favicon_image, 6);
		gtk_widget_set_margin_top (box, 6);
		gtk_widget_set_margin_bottom (box, 6);
		gtk_label_set_wrap (GTK_LABEL (row->headline_label), TRUE);
		gtk_label_set_wrap_mode (GTK_LABEL (row->headline_label), PANGO_WRAP_WORD_CHAR);
		gtk_label_set_ellipsize (GTK_LABEL (row->headline_label), PANGO_ELLIPSIZE_NONE);

		gtk_label_set_wrap (GTK_LABEL (row->preview_label), TRUE);
		gtk_label_set_wrap_mode (GTK_LABEL (row->preview_label), PANGO_WRAP_WORD_CHAR);
		gtk_label_set_ellipsize (GTK_LABEL (row->preview_label), PANGO_ELLIPSIZE_NONE);
	} else {
		gtk_image_set_icon_size (GTK_IMAGE (row->favicon_image), GTK_ICON_SIZE_NORMAL);
		gtk_widget_set_margin_start (row->favicon_image, 6);
		gtk_widget_set_margin_end (row->favicon_image, 6);
		gtk_widget_set_margin_top (box, 2);
		gtk_widget_set_margin_bottom (box, 2);
		gtk_label_set_wrap (GTK_LABEL (row->headline_label), FALSE);
		gtk_label_set_wrap_mode (GTK_LABEL (row->headline_label), PANGO_WRAP_NONE);
		gtk_label_set_ellipsize (GTK_LABEL (row->headline_label), PANGO_ELLIPSIZE_END);

		gtk_label_set_wrap (GTK_LABEL (row->preview_label), FALSE);
		gtk_label_set_wrap_mode (GTK_LABEL (row->preview_label), PANGO_WRAP_NONE);
		gtk_label_set_ellipsize (GTK_LABEL (row->preview_label), PANGO_ELLIPSIZE_NONE);
	}

	item_list_view_apply_row_visibility (ilv, row);
}

static void
item_list_view_for_each_row (ItemListView *ilv, GFunc func, gpointer user_data)
{
	for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (ilv->listbox));
	     child;
	     child = gtk_widget_get_next_sibling (child)) {
		func (g_object_get_data (G_OBJECT (child), "item-list-row"), user_data);
	}
}

static void
item_list_view_update_wide_mode_cb (gpointer data, gpointer user_data)
{
	ItemListRow *row = data;
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	item_list_view_apply_row_layout (ilv, row);
}

static nodeViewSortType
item_list_view_effective_sort_type (ItemListView *ilv)
{
	if (ilv->sort_type == NODE_VIEW_SORT_BY_TITLE && ilv->wideView)
		return NODE_VIEW_SORT_BY_TIME;

	return ilv->sort_type;
}

static gint
item_list_view_cmp_rows (ItemListView *ilv, ItemListRow *a, ItemListRow *b)
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
				cmp = -1;
			else if (a->time < b->time)
				cmp = 1;
			else
				cmp = 0;
			break;
	}

	if (cmp == 0) {
		if (a->time > b->time)
			cmp = -1;
		else if (a->time < b->time)
			cmp = 1;
		else
			cmp = (a->id < b->id) ? -1 : (a->id > b->id ? 1 : 0);
	}

	if (ilv->sort_reversed)
		cmp = -cmp;

	return cmp;
}

static gint
item_list_view_sort_func (GtkListBoxRow *row_a, GtkListBoxRow *row_b, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	ItemListRow *a = g_object_get_data (G_OBJECT (row_a), "item-list-row");
	ItemListRow *b = g_object_get_data (G_OBJECT (row_b), "item-list-row");

	if (!a || !b)
		return 0;

	return item_list_view_cmp_rows (ilv, a, b);
}

static void
item_list_view_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
	ItemListView *ilv = ITEM_LIST_VIEW (object);

	switch (prop_id) {
		case PROP_WIDE_VIEW:
			ilv->wideView = g_value_get_boolean (value);
			gtk_list_box_set_show_separators (ilv->listbox, ilv->wideView);
			item_list_view_for_each_row (ilv, item_list_view_update_wide_mode_cb, ilv);
			gtk_list_box_invalidate_sort (ilv->listbox);
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

	g_signal_handlers_disconnect_by_data (G_OBJECT (ilv->listbox), object);

	g_hash_table_destroy (ilv->rows_by_id);
	g_slist_free (ilv->item_ids);

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
	GtkListBoxRow *selected = gtk_list_box_get_selected_row (ilv->listbox);
	if (selected)
		gtk_list_box_unselect_row (ilv->listbox, selected);

	GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (ilv->listbox));
	while (child) {
		GtkWidget *next = gtk_widget_get_next_sibling (child);
		gtk_list_box_remove (ilv->listbox, child);
		child = next;
	}

	g_hash_table_remove_all (ilv->rows_by_id);
}

static void
on_itemlist_selection_changed (GtkListBox *listbox, gpointer user_data)
{
	GtkListBoxRow *row = gtk_list_box_get_selected_row (listbox);
	ItemListRow *item_row = row ? g_object_get_data (G_OBJECT (row), "item-list-row") : NULL;
	gulong id = item_row ? item_row->id : 0;

	g_signal_emit_by_name (user_data, "selection-changed", id);
}

void
item_list_view_set_sort_column (ItemListView *ilv, nodeViewSortType sortType, gboolean sortReversed)
{
	ilv->sort_type = sortType;
	ilv->sort_reversed = sortReversed;
	gtk_list_box_invalidate_sort (ilv->listbox);
}

static void
item_list_view_all_items_removed (GObject *obj, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);

	debug (DEBUG_CACHE, "item_list_view_all_items_removed()");

	item_list_view_clear_rows (ilv);

	g_slist_free (ilv->item_ids);
	ilv->item_ids = NULL;
}

static void
item_list_view_item_removed (GObject *obj, gulong id, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	ItemListRow *row = item_list_view_id_to_row (ilv, id);

	if (!row) {
		debug (DEBUG_GUI, "item id %lu to be removed not found in item id list!", id);
		ilv->item_ids = g_slist_remove (ilv->item_ids, GUINT_TO_POINTER (id));
		return;
	}

	GtkListBoxRow *selected = gtk_list_box_get_selected_row (ilv->listbox);
	if (selected == GTK_LIST_BOX_ROW (row->row)) {
		gint index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (row->row));
		GtkListBoxRow *next = gtk_list_box_get_row_at_index (ilv->listbox, index + 1);
		if (!next && index > 0)
			next = gtk_list_box_get_row_at_index (ilv->listbox, index - 1);
		if (next)
			gtk_list_box_select_row (ilv->listbox, next);
		else
			gtk_list_box_unselect_row (ilv->listbox, selected);
	}

	gtk_list_box_remove (ilv->listbox, row->row);
	g_hash_table_remove (ilv->rows_by_id, GUINT_TO_POINTER (id));
	ilv->item_ids = g_slist_remove (ilv->item_ids, GUINT_TO_POINTER (id));
}

static void
item_list_view_item_batch_started (GObject *obj, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	//GtkAdjustment *adj;

	// FIXME: still needed with GtkListView?
	//adj = gtk_scrollable_get_vadjustment (GTK_SCROLLABLE (ilv->ilscrolledwindow));
	//gtk_adjustment_set_value (adj, 0.0);

	item_list_view_clear_rows (ilv);
	g_slist_free (ilv->item_ids);
	ilv->item_ids = NULL;

	ilv->batch_mode = TRUE;
}

static void
item_list_view_item_batch_ended (GObject *obj, gpointer *n, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	Node *node = (Node *)n;

	g_assert (ilv->batch_mode);

	item_list_view_set_sort_column (ilv, node->sortColumn, node->sortReversed);
	ilv->batch_mode = FALSE;
}

static void
item_list_view_update_item_internal (ItemListView *ilv, itemPtr item, ItemListRow *row, Node *node)
{
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
	gint state = 0;
	gboolean no_title = FALSE;

	if (item->flagStatus)
		state += 2;
	if (!item->readStatus)
		state += 1;

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

	row->time = item->time;
	row->state = state;
	row->source = node ? node : row->source;

	g_free (row->sort_label);
	row->sort_label = g_utf8_casefold (plain_title, -1);

	gtk_label_set_markup (GTK_LABEL (row->headline_label), headline_markup);
	gtk_label_set_markup (GTK_LABEL (row->preview_label), preview_markup);
	gtk_label_set_xalign (GTK_LABEL (row->headline_label), item_list_title_alignment (plain_title));
	gtk_label_set_xalign (GTK_LABEL (row->preview_label), item_list_title_alignment (plain_title));
	gtk_label_set_text (GTK_LABEL (row->date_label), time_str);

	if (state_icon)
		gtk_image_set_from_gicon (GTK_IMAGE (row->state_image), (GIcon *)state_icon);
	else
		gtk_image_clear (GTK_IMAGE (row->state_image));

	if (row->source)
		gtk_image_set_from_gicon (GTK_IMAGE (row->favicon_image), node_get_icon (row->source));
	else
		gtk_image_clear (GTK_IMAGE (row->favicon_image));

	item_list_view_apply_row_layout (ilv, row);

	gtk_widget_set_tooltip_text (row->headline_label, plain_title);

	g_free (headline_markup);
	g_free (preview_markup);
	g_free (title_limited_escaped);
	g_free (title_limited);
	g_free (time_str_escaped);
	g_free (escaped_title);
	g_free (plain_title);
	g_free (time_str);

	if (!ilv->batch_mode)
		gtk_list_box_invalidate_sort (ilv->listbox);
}

void
item_list_view_update_item (ItemListView *ilv, itemPtr item)
{
	ItemListRow *row = item_list_view_id_to_row (ilv, item->id);

	if (!row)
		return;

	item_list_view_update_item_internal (ilv, item, row, NULL);
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

	for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (ilv->listbox));
	     child;
	     child = gtk_widget_get_next_sibling (child)) {
		ItemListRow *row = g_object_get_data (G_OBJECT (child), "item-list-row");
		if (!row)
			continue;

		itemPtr item = item_load (row->id);
		item_list_view_update_item (ilv, item);
		item_unload (item);
	}
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

static void
on_item_list_view_pressed_event (GtkGestureClick *gesture, guint n_press, gdouble x, gdouble y, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	GtkListBoxRow *row = gtk_list_box_get_row_at_y (ilv->listbox, (int)y);
	ItemListRow *item_row;
	itemPtr item;

	if (!row)
		return;

	item_row = g_object_get_data (G_OBJECT (row), "item-list-row");
	if (!item_row)
		return;

	item = item_load (item_row->id);
	if (!item)
		return;

	if (n_press == 1) {
		switch (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture))) {
			case GDK_BUTTON_PRIMARY: {
				GtkWidget *picked = gtk_widget_pick (GTK_WIDGET (ilv->listbox), x, y, GTK_PICK_DEFAULT);
				if (picked &&
				    (item_list_view_widget_is_descendant (picked, item_row->favicon_image) ||
				     item_list_view_widget_is_descendant (picked, item_row->state_image))) {
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
				GtkWidget *anchor = gtk_widget_get_parent (GTK_WIDGET (ilv->listbox));
				GdkRectangle rect;
				graphene_point_t src = GRAPHENE_POINT_INIT ((float)x, (float)y);
				graphene_point_t dst;

				if (!anchor)
					anchor = GTK_WIDGET (ilv->listbox);

				gtk_widget_set_parent (popover, anchor);

				if (anchor != GTK_WIDGET (ilv->listbox) &&
				    gtk_widget_compute_point (GTK_WIDGET (ilv->listbox), anchor, &src, &dst)) {
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
on_item_list_row_activated (GtkListBox *listbox, GtkListBoxRow *row, gpointer user_data)
{
	ItemListRow *item_row = g_object_get_data (G_OBJECT (row), "item-list-row");
	if (!item_row)
		return;

	itemPtr item = item_load (item_row->id);
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
	GtkListBoxRow *selected = gtk_list_box_get_selected_row (ilv->listbox);
	gint index = selected ? gtk_list_box_row_get_index (selected) : 0;
	gint target = index + step;

	if (!selected && step < 0)
		target = G_MAXINT;
	if (target < 0)
		target = 0;

	GtkListBoxRow *row = gtk_list_box_get_row_at_index (ilv->listbox, target);
	if (!row && target > 0)
		row = gtk_list_box_get_row_at_index (ilv->listbox, target - 1);
	if (row)
		gtk_list_box_select_row (ilv->listbox, row);
}

void
item_list_view_move_cursor_to_first (ItemListView *ilv)
{
	GtkListBoxRow *row = gtk_list_box_get_row_at_index (ilv->listbox, 0);
	if (row)
		gtk_list_box_select_row (ilv->listbox, row);
}

static ItemListRow *
item_list_view_create_row (ItemListView *ilv, itemPtr item, Node *node)
{
	ItemListRow *row = g_new0 (ItemListRow, 1);
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *text_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 2);
	GtkWidget *header_box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);

	row->id = item->id;
	row->source = node;
	row->row = gtk_list_box_row_new ();
	row->state_image = gtk_image_new ();
	row->favicon_image = gtk_image_new ();
	row->headline_label = gtk_label_new (NULL);
	row->preview_label = gtk_label_new (NULL);
	row->date_label = gtk_label_new (NULL);

	gtk_label_set_use_markup (GTK_LABEL (row->headline_label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (row->headline_label), 0.0);
	gtk_label_set_wrap (GTK_LABEL (row->headline_label), FALSE);
	gtk_label_set_ellipsize (GTK_LABEL (row->headline_label), PANGO_ELLIPSIZE_END);

	gtk_label_set_use_markup (GTK_LABEL (row->preview_label), TRUE);
	gtk_label_set_xalign (GTK_LABEL (row->preview_label), 0.0);
	gtk_label_set_wrap (GTK_LABEL (row->preview_label), TRUE);
	gtk_label_set_wrap_mode (GTK_LABEL (row->preview_label), PANGO_WRAP_WORD_CHAR);
	gtk_label_set_ellipsize (GTK_LABEL (row->preview_label), PANGO_ELLIPSIZE_NONE);
	gtk_widget_add_css_class (row->preview_label, "dim-label");

	gtk_label_set_xalign (GTK_LABEL (row->date_label), 1.0);
	gtk_widget_set_halign (row->date_label, GTK_ALIGN_END);
	gtk_widget_set_hexpand (row->date_label, FALSE);
	gtk_widget_add_css_class (row->date_label, "dim-label");

	gtk_widget_set_halign (row->state_image, GTK_ALIGN_START);
	gtk_widget_set_halign (row->favicon_image, GTK_ALIGN_START);
	gtk_widget_set_hexpand (row->headline_label, TRUE);
	gtk_widget_set_hexpand (row->preview_label, TRUE);

	gtk_box_append (GTK_BOX (header_box), row->headline_label);
	gtk_box_append (GTK_BOX (header_box), row->date_label);
	gtk_box_append (GTK_BOX (text_box), header_box);
	gtk_box_append (GTK_BOX (text_box), row->preview_label);

	gtk_box_append (GTK_BOX (box), row->state_image);
	gtk_box_append (GTK_BOX (box), row->favicon_image);
	gtk_box_append (GTK_BOX (box), text_box);

	gtk_widget_set_margin_start (box, 6);
	gtk_widget_set_margin_end (box, 6);

	gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row->row), box);
	g_object_set_data (G_OBJECT (row->row), "item-list-row", row);

	item_list_view_update_item_internal (ilv, item, row, node);
	return row;
}

static void
item_list_view_add_item_to_listbox (ItemListView *ilv, itemPtr item)
{
	Node *node = node_from_id (item->nodeId);
	ItemListRow *row;

	if (!node)
		return;

	row = item_list_view_id_to_row (ilv, item->id);
	if (!row) {
		row = item_list_view_create_row (ilv, item, node);
		gtk_list_box_append (ilv->listbox, row->row);
		g_hash_table_insert (ilv->rows_by_id, GUINT_TO_POINTER (item->id), row);
		ilv->item_ids = g_slist_prepend (ilv->item_ids, GUINT_TO_POINTER (item->id));
	} else {
		item_list_view_update_item_internal (ilv, item, row, node);
	}
}

static void
item_list_view_item_added (GObject *obj, gint itemId, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);
	itemPtr item = item_load (itemId);

	item_list_view_add_item_to_listbox (ilv, item);
	item_unload (item);

	if (!ilv->batch_mode)
		gtk_list_box_invalidate_sort (ilv->listbox);
}

static void
item_list_view_select (GObject *obj, gint id, gpointer user_data)
{
	ItemListView *ilv = ITEM_LIST_VIEW (user_data);

	if (id) {
		ItemListRow *row = item_list_view_id_to_row (ilv, id);
		if (row) {
			gtk_list_box_select_row (ilv->listbox, GTK_LIST_BOX_ROW (row->row));
			gtk_widget_grab_focus (row->row);
		} else {
			debug (DEBUG_GUI, "item_list_view_select: ignore missing item id %d in current view", id);
		}
	} else {
		GtkListBoxRow *selected = gtk_list_box_get_selected_row (ilv->listbox);
		if (selected)
			gtk_list_box_unselect_row (ilv->listbox, selected);
	}
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

	ilv->rows_by_id = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)item_list_row_free);

	ilv->keypress = gtk_event_controller_key_new ();
	ilv->gesture = gtk_gesture_click_new ();
	ilv->popup_gesture = gtk_gesture_click_new ();
	ilv->middle_gesture = gtk_gesture_click_new ();

	ilv->ilscrolledwindow = gtk_scrolled_window_new ();
	gtk_widget_set_vexpand (ilv->ilscrolledwindow, TRUE);
	g_object_ref_sink (ilv->ilscrolledwindow);

	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (ilv->ilscrolledwindow), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

	ilv->listbox = GTK_LIST_BOX (gtk_list_box_new ());
	gtk_list_box_set_selection_mode (ilv->listbox, GTK_SELECTION_SINGLE);
	gtk_list_box_set_activate_on_single_click (ilv->listbox, FALSE);
	gtk_list_box_set_sort_func (ilv->listbox, item_list_view_sort_func, ilv, NULL);
	gtk_list_box_set_show_separators (ilv->listbox, FALSE);
	gtk_scrolled_window_set_child (GTK_SCROLLED_WINDOW (ilv->ilscrolledwindow), GTK_WIDGET (ilv->listbox));
	gtk_widget_set_name (GTK_WIDGET (ilv->listbox), "itemlist");

	g_signal_connect (G_OBJECT (ilv->listbox), "selected-rows-changed", G_CALLBACK (on_itemlist_selection_changed), ilv);
	g_signal_connect (G_OBJECT (ilv->listbox), "row-activated", G_CALLBACK (on_item_list_row_activated), ilv);

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (ilv->middle_gesture), GDK_BUTTON_MIDDLE);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (ilv->middle_gesture), GTK_PHASE_CAPTURE);
	g_signal_connect (ilv->middle_gesture, "pressed", G_CALLBACK (on_item_list_view_pressed_event), ilv);
	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (ilv->popup_gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect (ilv->popup_gesture, "pressed", G_CALLBACK (on_item_list_view_pressed_event), ilv);
	g_signal_connect (ilv->gesture, "pressed", G_CALLBACK (on_item_list_view_pressed_event), ilv);
	g_signal_connect (ilv->keypress, "key-pressed", G_CALLBACK (on_item_list_view_key_pressed_event), ilv);

	gtk_widget_add_controller (GTK_WIDGET (ilv->listbox), GTK_EVENT_CONTROLLER (ilv->middle_gesture));
	gtk_widget_add_controller (GTK_WIDGET (ilv->listbox), GTK_EVENT_CONTROLLER (ilv->popup_gesture));
	gtk_widget_add_controller (GTK_WIDGET (ilv->listbox), GTK_EVENT_CONTROLLER (ilv->gesture));
	gtk_widget_add_controller (GTK_WIDGET (ilv->listbox), ilv->keypress);

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
	return (NULL != item_list_view_id_to_row (ilv, id));
}

itemPtr
item_list_view_find_unread_item (ItemListView *ilv, gulong startId)
{
	gint index = 0;

	if (startId) {
		ItemListRow *start = item_list_view_id_to_row (ilv, startId);
		if (!start)
			return NULL;
		index = gtk_list_box_row_get_index (GTK_LIST_BOX_ROW (start->row));
	}

	for (GtkListBoxRow *row = gtk_list_box_get_row_at_index (ilv->listbox, index);
	     row;
	     row = gtk_list_box_get_row_at_index (ilv->listbox, ++index)) {
		ItemListRow *item_row = g_object_get_data (G_OBJECT (row), "item-list-row");
		if (!item_row)
			continue;

		itemPtr item = item_load (item_row->id);
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
