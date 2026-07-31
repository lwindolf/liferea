/**
 * @file feed_list_view.c  the feed list in a GtkListBox
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

	GtkListBox		*listbox;
	GHashTable		*rows_by_id; /* node id -> GtkListBoxRow* */

	gboolean		feedlist_reduced_unread;
	gboolean		suppress_selection;
};

enum {
	SELECTION_CHANGED,
	LAST_SIGNAL
};

static FeedListView *flv = NULL;
static guint feed_list_view_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (FeedListView, feed_list_view, G_TYPE_OBJECT);

static void feed_list_view_reload_feedlist (void);
static void feed_list_view_select (Node *node);
static gboolean feed_list_view_filter_visible_function (Node *node);
static void feed_list_view_confirm_remove_node_cb (gpointer user_data);
static void feed_list_view_confirm_add_subscription_cb (gpointer user_data);
static void feed_list_view_confirm_free_subscription_cb (gpointer user_data);

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
	f->rows_by_id = g_hash_table_new (g_str_hash, g_str_equal);
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

static gboolean
feed_list_view_is_expandable (Node *node)
{
	return IS_FOLDER (node) || IS_NODE_SOURCE (node);
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

static GtkWidget *
feed_list_view_create_row (Node *node, guint depth)
{
	GtkWidget *row = gtk_list_box_row_new ();
	GtkWidget *box = gtk_box_new (GTK_ORIENTATION_HORIZONTAL, 6);
	GtkWidget *arrow = gtk_image_new ();
	GtkWidget *icon = gtk_image_new ();;
	GtkWidget *label = gtk_label_new (NULL);
	GtkWidget *count = gtk_label_new (NULL);
	gchar *label_markup = feed_list_view_node_label_markup (node);
	gchar *count_markup = feed_list_view_node_count_markup (node);

	gtk_widget_set_margin_start (box, 6 + (gint)depth * 16);
	gtk_widget_set_margin_end (box, 6);
	gtk_widget_set_margin_top (box, 0);
	gtk_widget_set_margin_bottom (box, 0);

	if (feed_list_view_is_expandable (node) && !flv->feedlist_reduced_unread) {
		gtk_image_set_from_icon_name (GTK_IMAGE (arrow), node->expanded ? "pan-down-symbolic" : "pan-end-symbolic");
	} else {
		gtk_image_set_from_icon_name (GTK_IMAGE (arrow), "pan-end-symbolic");
		gtk_widget_set_opacity (arrow, 0.0);
	}

	gtk_image_set_from_gicon (GTK_IMAGE (icon), (GIcon *)(node->available ? node_get_icon (node) : icon_get (ICON_UNAVAILABLE)));
	gtk_label_set_markup (GTK_LABEL (label), label_markup);
	gtk_label_set_xalign (GTK_LABEL (label), 0.0f);
	gtk_label_set_ellipsize (GTK_LABEL (label), PANGO_ELLIPSIZE_END);
	gtk_widget_set_hexpand (label, TRUE);

	if (count_markup)
		gtk_label_set_markup (GTK_LABEL (count), count_markup);
	gtk_label_set_xalign (GTK_LABEL (count), 1.0f);

	gtk_box_append (GTK_BOX (box), arrow);
	gtk_box_append (GTK_BOX (box), icon);
	gtk_box_append (GTK_BOX (box), label);
	gtk_box_append (GTK_BOX (box), count);
	gtk_list_box_row_set_child (GTK_LIST_BOX_ROW (row), box);
	g_object_set_data (G_OBJECT (row), "node", node);
	g_object_set_data (G_OBJECT (row), "expander", arrow);
	ui_dnd_setup_feedlist_row (row);

	g_hash_table_insert (flv->rows_by_id, (gpointer)node->id, row);

	g_free (label_markup);
	g_free (count_markup);
	return row;
}

static gboolean
feed_list_view_widget_is_descendant (GtkWidget *widget, GtkWidget *ancestor)
{
	while (widget) {
		if (widget == ancestor)
			return TRUE;
		widget = gtk_widget_get_parent (widget);
	}

	return FALSE;
}

static void
feed_list_view_append_children (Node *parent, guint depth)
{
	for (GSList *iter = parent->children; iter; iter = g_slist_next (iter)) {
		Node *node = (Node *)iter->data;
		gboolean visible = feed_list_view_filter_visible_function (node);

		if (visible) {
			GtkWidget *row = feed_list_view_create_row (node, flv->feedlist_reduced_unread ? 0 : depth);
			gtk_list_box_append (flv->listbox, row);
		}

		if (flv->feedlist_reduced_unread) {
			if (feed_list_view_is_expandable (node))
				feed_list_view_append_children (node, 0);
		} else if (feed_list_view_is_expandable (node) && node->expanded) {
			feed_list_view_append_children (node, depth + 1);
		}
	}
}

static void
feed_list_view_clear_feedlist (void)
{
	flv->suppress_selection = TRUE;
	for (GtkWidget *child = gtk_widget_get_first_child (GTK_WIDGET (flv->listbox)); child;) {
		GtkWidget *next = gtk_widget_get_next_sibling (child);
		gtk_list_box_remove (flv->listbox, child);
		child = next;
	}
	g_hash_table_remove_all (flv->rows_by_id);
	flv->suppress_selection = FALSE;
}

static void
feed_list_view_load_feedlist (Node *node)
{
	feed_list_view_append_children (node, 0);
}

static void
feed_list_view_reload_feedlist (void)
{
	Node *selected = feedlist_get_selected ();
	feed_list_view_clear_feedlist ();
	feed_list_view_load_feedlist (feedlist_get_root ());
	feed_list_view_select (selected);
}

static gboolean
feed_list_view_expand (Node *node)
{
	gboolean changed = FALSE;

	if (!node)
		return FALSE;

	if (node->parent)
		changed = feed_list_view_expand (node->parent);

	if (feed_list_view_is_expandable (node) && !node->expanded) {
		node->expanded = TRUE;
		changed = TRUE;
	}

	return changed;
}

static void
feed_list_view_set_expansion (Node *folder, gboolean expanded)
{
	if (flv->feedlist_reduced_unread)
		return;

	if (!feed_list_view_is_expandable (folder))
		return;

	if (folder->expanded == expanded)
		return;

	folder->expanded = expanded;
	feed_list_view_reload_feedlist ();
}

static void
feed_list_view_selection_changed_cb (GtkListBox *listbox, gpointer data)
{
	GtkListBoxRow *row;
	Node *node;

	if (flv->suppress_selection)
		return;

	row = gtk_list_box_get_selected_row (listbox);
	if (!row)
		return;

	node = g_object_get_data (G_OBJECT (row), "node");
	if (!node)
		return;

	debug (DEBUG_GUI, "feed list selection changed to \"%s\"", node_get_title (node));

	feedlist_set_selected (node);
	browser_tabs_show_headlines ();

	/* Re-apply reduced mode filtering after selection changes. */
	if (flv->feedlist_reduced_unread)
		feed_list_view_reload_feedlist ();
}

static void
feed_list_view_row_activated_cb (GtkListBox *listbox, GtkListBoxRow *row, gpointer data)
{
	Node *node = g_object_get_data (G_OBJECT (row), "node");

	if (node && feed_list_view_is_expandable (node))
		feed_list_view_set_expansion (node, !node->expanded);
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
	GtkListBoxRow *row;
	Node *node = NULL;

	if (n_press != 1)
		return FALSE;

	row = gtk_list_box_get_row_at_y (flv->listbox, (gint)y);
	if (row)
		node = g_object_get_data (G_OBJECT (row), "node");

	switch (gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture))) {
		case GDK_BUTTON_PRIMARY:
			if (row && node && feed_list_view_is_expandable (node)) {
				GtkWidget *row_widget = GTK_WIDGET (row);
				GtkWidget *expander = g_object_get_data (G_OBJECT (row), "expander");
				graphene_point_t src = GRAPHENE_POINT_INIT ((float)x, (float)y);
				graphene_point_t dst;

				if (expander && gtk_widget_compute_point (GTK_WIDGET (flv->listbox), row_widget, &src, &dst)) {
					GtkWidget *picked = gtk_widget_pick (row_widget, dst.x, dst.y, GTK_PICK_DEFAULT);
					if (picked && feed_list_view_widget_is_descendant (picked, expander)) {
						feed_list_view_set_expansion (node, !node->expanded);
						gtk_gesture_set_state (GTK_GESTURE (gesture), GTK_EVENT_SEQUENCE_CLAIMED);
						return TRUE;
					}
				}
			}
			break;
		case GDK_BUTTON_SECONDARY: {
			GMenu *menu = feed_list_view_popup_menu (node);
			GtkWidget *popover = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu));
			GtkWidget *anchor = gtk_widget_get_parent (GTK_WIDGET (flv->listbox));
			GdkRectangle rect;
			graphene_point_t src = GRAPHENE_POINT_INIT ((float)x, (float)y);
			graphene_point_t dst;

			if (!anchor)
				anchor = GTK_WIDGET (flv->listbox);

			gtk_widget_set_parent (popover, anchor);

			if ((anchor != GTK_WIDGET (flv->listbox)) &&
			    gtk_widget_compute_point (GTK_WIDGET (flv->listbox), anchor, &src, &dst)) {
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
static gboolean
feed_list_view_filter_visible_function (Node *node)
{
	if (!flv->feedlist_reduced_unread)
		return TRUE;

	if (node->subscription && node->subscription->alwaysShowInReduced)
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

	return node->unreadCount > 0;
}

static void
feed_list_view_node_changed (GObject *obj, gchar *nodeId, gpointer user_data)
{
	feed_list_view_reload_feedlist ();
}

static void
feed_list_view_node_removed (GObject *obj, gchar *nodeId, gpointer user_data)
{
	GtkListBoxRow *row;

	row = g_hash_table_lookup (flv->rows_by_id, nodeId);
	if (!row)
		return;

	gtk_list_box_remove (flv->listbox, GTK_WIDGET (row));
	g_hash_table_remove (flv->rows_by_id, nodeId);
}

static void
feed_list_view_node_updated (GObject *obj, const gchar *nodeId, gpointer user_data)
{
	feed_list_view_reload_feedlist ();
}

static void
feed_list_view_node_selected (GObject *obj, gchar *nodeId, gpointer user_data)
{
	feed_list_view_select (node_from_id (nodeId));
}

FeedListView *
feed_list_view_create (GtkListBox *listbox, FeedList *feedlist)
{
	GtkEventController *controller;
	GtkGesture *primary_gesture;
	GtkGesture *popup_gesture;
	GtkGesture *middle_gesture;

	g_assert (NULL == flv);
	flv = FEED_LIST_VIEW (g_object_new (FEED_LIST_VIEW_TYPE, NULL));
	flv->listbox = listbox;

	gtk_list_box_set_selection_mode (flv->listbox, GTK_SELECTION_SINGLE);
	gtk_list_box_set_activate_on_single_click (flv->listbox, FALSE);

	controller = gtk_event_controller_key_new ();
	primary_gesture = gtk_gesture_click_new ();
	popup_gesture = gtk_gesture_click_new ();
	middle_gesture = gtk_gesture_click_new ();

	g_signal_connect (G_OBJECT (flv->listbox), "row-activated", G_CALLBACK (feed_list_view_row_activated_cb), flv);
	g_signal_connect (G_OBJECT (flv->listbox), "selected-rows-changed", G_CALLBACK (feed_list_view_selection_changed_cb), flv);
	g_signal_connect (controller, "key-pressed", G_CALLBACK (feed_list_view_key_pressed_cb), flv);

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (primary_gesture), GDK_BUTTON_PRIMARY);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (primary_gesture), GTK_PHASE_CAPTURE);
	g_signal_connect (primary_gesture, "pressed", G_CALLBACK (feed_list_view_pressed_cb), flv);

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (middle_gesture), GDK_BUTTON_MIDDLE);
	gtk_event_controller_set_propagation_phase (GTK_EVENT_CONTROLLER (middle_gesture), GTK_PHASE_CAPTURE);
	g_signal_connect (middle_gesture, "pressed", G_CALLBACK (feed_list_view_pressed_cb), flv);

	gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (popup_gesture), GDK_BUTTON_SECONDARY);
	g_signal_connect (popup_gesture, "pressed", G_CALLBACK (feed_list_view_pressed_cb), flv);

	gtk_widget_add_controller (GTK_WIDGET (flv->listbox), controller);
	gtk_widget_add_controller (GTK_WIDGET (flv->listbox), GTK_EVENT_CONTROLLER (primary_gesture));
	gtk_widget_add_controller (GTK_WIDGET (flv->listbox), GTK_EVENT_CONTROLLER (middle_gesture));
	gtk_widget_add_controller (GTK_WIDGET (flv->listbox), GTK_EVENT_CONTROLLER (popup_gesture));

	/* Keep URL drops enabled; row reordering DnD is no longer tree-model based. */
	ui_dnd_setup_feedlist (GTK_WIDGET (flv->listbox));

	/* For performance prevent selection signals when filling the feed list
	   will be enabled when LifereaShell setup is finished */
	gtk_widget_set_sensitive (GTK_WIDGET (flv->listbox), FALSE);

	g_signal_connect (feedlist, "node-added", G_CALLBACK (feed_list_view_node_changed), flv);
	g_signal_connect (feedlist, "node-removed", G_CALLBACK (feed_list_view_node_removed), flv);
	g_signal_connect (feedlist, "node-updated", G_CALLBACK (feed_list_view_node_updated), flv);
	g_signal_connect (feedlist, "node-selected", G_CALLBACK (feed_list_view_node_selected), flv);

	feed_list_view_reload_feedlist ();
	return flv;
}

static void
feed_list_view_select (Node *node)
{
	GtkListBoxRow *row;

	flv->suppress_selection = TRUE;

	if (node && node != feedlist_get_root ()) {
		if (!flv->feedlist_reduced_unread && node->parent) {
			if (feed_list_view_expand (LIFEREA_NODE (node->parent)))
				feed_list_view_reload_feedlist ();
		}

		row = g_hash_table_lookup (flv->rows_by_id, node->id);
		if (row) {
			GtkRoot *root = gtk_widget_get_root (GTK_WIDGET (flv->listbox));
			GtkWidget *focus = root ? gtk_root_get_focus (root) : NULL;
			gboolean keep_feedlist_focus = focus &&
			    gtk_widget_is_ancestor (focus, GTK_WIDGET (flv->listbox));

			gtk_list_box_select_row (flv->listbox, row);

			/* Preserve feed list focus for feed-list navigation, but avoid stealing
			   focus from the item list during background node-updated refreshes. */
			if (keep_feedlist_focus)
				gtk_widget_grab_focus (GTK_WIDGET (row));
		}
	} else {
		gtk_list_box_unselect_all (flv->listbox);
	}

	flv->suppress_selection = FALSE;
}

void
feed_list_view_move_cursor (FeedListView *view, gint step)
{
	GtkListBoxRow *selected;
	gint index;

	if (!view)
		return;

	selected = gtk_list_box_get_selected_row (view->listbox);
	if (!selected) {
		selected = gtk_list_box_get_row_at_index (view->listbox, 0);
		if (!selected)
			return;
		gtk_list_box_select_row (view->listbox, selected);
		return;
	}

	index = gtk_list_box_row_get_index (selected);
	selected = gtk_list_box_get_row_at_index (view->listbox, index + step);
	if (selected)
		gtk_list_box_select_row (view->listbox, selected);
}

/* Expansion & Collapsing */

gboolean
feed_list_view_is_expanded (const gchar *nodeId)
{
	Node *node;

	if (flv->feedlist_reduced_unread)
		return FALSE;

	node = node_from_id (nodeId);
	return node ? node->expanded : FALSE;
}

void
feed_list_view_sort_folder (Node *folder)
{
	folder->children = g_slist_sort (folder->children, feed_list_view_sort_folder_compare);
	feed_list_view_reload_feedlist ();
	feedlist_schedule_save ();
}

void
feed_list_view_set_reduce_mode (gboolean newReduceMode)
{
	flv->feedlist_reduced_unread = newReduceMode;
	feed_list_view_reload_feedlist ();
}

static void
on_nodenamedialog_response (GtkButton *button, gpointer user_data)
{
	GtkWidget *dialog = GTK_WIDGET (user_data);
	Node *node = node_from_id (g_object_get_data (G_OBJECT (dialog), "node"));

	if (node) {
		node_set_title (node, liferea_dialog_entryrow_get (GTK_WIDGET (dialog), "nameEntry"));
		feed_list_view_reload_feedlist ();
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
feed_list_view_confirm_remove_node_cb (gpointer user_data)
{
	feedlist_remove_node ((Node *)user_data);
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
	feed_list_view_reload_feedlist ();
}