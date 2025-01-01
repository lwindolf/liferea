/**
 * @file popup_menu.c popup menus
 *
 * Copyright (C) 2003-2024 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2009 Adrian Bunk <bunk@users.sourceforge.net>
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

#include "ui/popup_menu.h"

#include <string.h>

#include "common.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "node_providers/folder.h"
#include "net_monitor.h"
#include "node_providers/newsbin.h"
#include "node.h"
#include "social.h"
#include "node_providers/vfolder.h"
#include "ui/feed_list_view.h"
#include "ui/item_list_view.h"
#include "ui/itemview.h"
#include "ui/liferea_shell.h"
#include "ui/preferences_dialog.h"
#include "node_source.h"

void
ui_popup_item_menu (itemPtr item, const GdkEvent *event)
{
	GtkWidget	*menu;
	GMenu		*menu_model, *section;
	GMenuItem	*menu_item;
	GSimpleActionGroup *action_group;
	GSList		*iter;
	gchar		*text, *item_link;
	const gchar *author;

	item_link = item_make_link (item);
	menu_model = g_menu_new ();
	menu_item = g_menu_item_new (NULL, NULL);
	author = item_get_author(item);

	section = g_menu_new ();
	g_menu_item_set_label (menu_item, _("Open In _Tab"));
	g_menu_item_set_action_and_target (menu_item, "item.open-item-in-tab", "t", (guint64) item->id);
	g_menu_append_item (section, menu_item);

	g_menu_item_set_label (menu_item, _("_Open In Browser"));
	g_menu_item_set_action_and_target (menu_item, "item.open-item-in-browser", "t", (guint64) item->id);
	g_menu_append_item (section, menu_item);

	g_menu_item_set_label (menu_item, _("Open In _External Browser"));
	g_menu_item_set_action_and_target (menu_item, "item.open-item-in-external-browser", "t", (guint64) item->id);
	g_menu_append_item (section, menu_item);

	if(author){
		g_menu_item_set_label (menu_item, _("Email The Author"));
		g_menu_item_set_action_and_target (menu_item, "app.email-the-author", "t", (guint64) item->id);
		g_menu_append_item (section, menu_item);
	}

	g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	iter = newsbin_get_list ();
	if (iter) {
		GMenu 		*submenu;
		guint32		i = 0;

		section = g_menu_new ();
		submenu = g_menu_new ();

		while (iter) {
			Node *node = (Node *)iter->data;
			g_menu_item_set_label (menu_item, node_get_title (node));
			g_menu_item_set_action_and_target (menu_item, "item.copy-item-to-newsbin", "(umt)", i, TRUE, (guint64) item->id);
			g_menu_append_item (submenu, menu_item);
			iter = g_slist_next (iter);
			i++;
		}

		g_menu_append_submenu (section, _("Copy to News Bin"), G_MENU_MODEL (submenu));
		g_object_unref (submenu);
		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
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

	g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	section = g_menu_new ();

	g_menu_item_set_label (menu_item, _("Toggle _Read Status"));
	g_menu_item_set_action_and_target (menu_item, "item.toggle-item-read-status", "t", (guint64) item->id);
	g_menu_append_item (section, menu_item);

	g_menu_item_set_label (menu_item, _("Toggle Item _Flag"));
	g_menu_item_set_action_and_target (menu_item, "item.toggle-item-flag", "t", (guint64) item->id);
	g_menu_append_item (section, menu_item);

	g_menu_item_set_label (menu_item, _("R_emove Item"));
	g_menu_item_set_action_and_target (menu_item, "item.remove-item", "t", (guint64) item->id);
	g_menu_append_item (section, menu_item);

	g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	g_object_unref (menu_item);
	g_free (item_link);
	g_menu_freeze (menu_model);
	menu = gtk_popover_menu_new_from_model (G_MENU_MODEL (menu_model));

	action_group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP(action_group), ui_popup_item_gaction_entries, G_N_ELEMENTS (ui_popup_item_gaction_entries), NULL);
	gtk_widget_insert_action_group (menu, "item", G_ACTION_GROUP (action_group));

	gtk_widget_set_parent (menu, liferea_shell_lookup ("itemlist"));
	g_object_unref (menu_model);
	ui_popup_menu (menu, event);
}

/**
 * Shows popup menus for the feed list depending on the
 * node type.
 */
static void
ui_popup_node_menu (Node *node, gboolean validSelection, const GdkEvent *event)
{
	GMenu 			*menu_model, *section;
	GSimpleActionGroup 	*action_group;

	menu_model = g_menu_new ();
	section = g_menu_new ();

	g_menu_append (section, _("_Update"), "node.node-update");
	g_menu_append (section, _("_Update Folder"), "node.node-update");


	GMenu		*submenu;

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

	if (validSelection) {
		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
		section = g_menu_new ();
		g_menu_append (section, _("_Mark All As Read"), "node.node-mark-all-read");
		if (NODE_PROVIDER (node)->capabilities & NODE_CAPABILITY_EXPORT_ITEMS) {
			g_menu_append (section, _("_Export Items To File"), "node.node-export-items-to-file");
		}
	}

	if (IS_VFOLDER (node)) {
		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
		section = g_menu_new ();
		g_menu_append (section, _("_Rebuild"), "node.node-rebuild-vfolder");
	}

	if (validSelection) {
		if (writeableFeedlist) {
			g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
			g_object_unref (section);
			section = g_menu_new ();
			g_menu_append (section, _("_Delete"), "node.node-delete");
			g_menu_append (section, _("_Properties"), "node.node-properties");
		}

		if (IS_NODE_SOURCE (node) && NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL) {
			g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
			g_object_unref (section);
			section = g_menu_new ();
			g_menu_append (section, _("Convert To Local Subscriptions..."), "node.node-convert-to-local");
		}
	}

	g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
	g_object_unref (section);

	g_menu_freeze (menu_model);
	return menu_model;
}

/* mouse button handler */
gboolean
on_mainfeedlist_button_press_event (GtkWidget *widget,
                                    GdkEvent *event,
                                    gpointer user_data)
{
	GdkEventButton 	*eb;
	GtkWidget	*treeview;
	GtkTreeModel	*model;
	GtkTreePath	*path;
	GtkTreeIter	iter;
	gboolean	selected = TRUE;
	Node		*node = NULL;

	treeview = liferea_shell_lookup ("feedlist");

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	eb = (GdkEventButton*)event;

	/* determine node */
	if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), eb->x, eb->y, &path, NULL, NULL, NULL)) {
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_path_free (path);
		gtk_tree_model_get (model, &iter, FS_PTR, &node, -1);
	} else {
		selected = FALSE;
		node = feedlist_get_root ();
	}

	/* apply action */
	switch (eb->button) {
		default:
			return FALSE;
			break;
		case 2:
			if (node) {
				feedlist_mark_all_read (node);
				itemview_update_node_info (node);
				itemview_update ();
			}
			break;
		case 3:
			if (node) {
				feed_list_view_select (node);
			} else {
				/* This happens when an "empty" node or nothing (feed list root) is clicked */
				selected = FALSE;
				node = feedlist_get_root ();
			}

			gtk_widget_grab_focus (widget);
			ui_popup_node_menu (node, selected, event);
			break;
	}

	return TRUE;
}

/* popup key handler */
gboolean
on_mainfeedlist_popup_menu (GtkWidget *widget,
                            gpointer   user_data)
{
	GtkWidget	*treeview;
	GtkTreeSelection *selection;
	GtkTreeModel	*model;
	GtkTreeIter	iter;
	gboolean	selected = TRUE;
	Node		*node = NULL;

	treeview = liferea_shell_lookup ("feedlist");
	selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (treeview));
	if (gtk_tree_selection_get_selected (selection, &model, &iter)) {
		gtk_tree_model_get (model, &iter, FS_PTR, &node, -1);
	} else {
		selected = FALSE;
		node = feedlist_get_root ();
	}

	ui_popup_node_menu (node, selected, NULL);
	return TRUE;
}

/*

static void
set_up_context_popover (GtkWidget *widget,
                        GMenuModel *model)
{
  GtkWidget *popover = gtk_popover_menu_new_from_model (model);
  GtkGesture *gesture;

  gtk_widget_set_parent (popover, widget);
  gtk_popover_set_has_arrow (GTK_POPOVER (popover), FALSE);
  gesture = gtk_gesture_click_new ();
  gtk_event_controller_set_name (GTK_EVENT_CONTROLLER (gesture), "widget-factory-context-click");
  gtk_gesture_single_set_button (GTK_GESTURE_SINGLE (gesture), GDK_BUTTON_SECONDARY);
  g_signal_connect (gesture, "pressed", G_CALLBACK (clicked_cb), popover);
  gtk_widget_add_controller (widget, GTK_EVENT_CONTROLLER (gesture));
}

ui_popup_menu_setup (void) {
	GtkBuilder *builder;

	builder = gtk_builder_new ();
	model = (GMenuModel *)gtk_builder_get_object (builder, "new_style_context_menu_model");
	set_up_context_popover (widget, model);
	g_object_unref (builder);
}*/