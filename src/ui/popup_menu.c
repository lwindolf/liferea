/**
 * @file popup_menu.c popup menus
 *
 * Copyright (C) 2003-2013 Lars Windolf <lars.windolf@gmx.de>
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
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "net_monitor.h"
#include "newsbin.h"
#include "node.h"
#include "social.h"
#include "vfolder.h"
#include "ui/enclosure_list_view.h"
#include "ui/feed_list_view.h"
#include "ui/item_list_view.h"
#include "ui/itemview.h"
#include "ui/liferea_shell.h"
#include "ui/preferences_dialog.h"
#include "fl_sources/node_source.h"

#define UI_POPUP_ITEM_IS_TOGGLE		1

static void
ui_popup_menu (GtkWidget *menu, const GdkEvent *event)
{
	g_signal_connect_after (G_OBJECT(menu), "unmap-event", G_CALLBACK(gtk_widget_destroy), NULL);
	gtk_widget_show_all (menu);
	gtk_menu_popup_at_pointer (GTK_MENU(menu), event);
}

static GtkWidget*
ui_popup_add_menuitem (GtkWidget *menu, const gchar *label, gpointer callback, gpointer data, gint toggle)
{
	GtkWidget	*item;

	g_assert (label);
	if (toggle) {
		item = gtk_check_menu_item_new_with_mnemonic (label);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(item), toggle - UI_POPUP_ITEM_IS_TOGGLE);
	} else {
		item = gtk_menu_item_new_with_mnemonic (label);
	}

	if (callback)
		g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(callback), data);

	gtk_menu_shell_append (GTK_MENU_SHELL(menu), item);

	return item;
}

static const GActionEntry ui_popup_item_gaction_entries[] = {
	{"copy-item-to-newsbin", on_action_copy_to_newsbin, "(umt)", NULL, NULL},
	{"toggle-item-read-status", on_toggle_unread_status, "t", NULL, NULL},
	{"toggle-item-flag", on_toggle_item_flag, "t", NULL, NULL},
	{"remove-item", on_action_remove_item, "t", NULL, NULL},
	{"open-item-in-tab", on_action_launch_item_in_tab, "t", NULL, NULL},
	{"open-item-in-browser", on_action_launch_item_in_browser, "t", NULL, NULL},
	{"open-item-in-external-browser", on_action_launch_item_in_external_browser, "t", NULL, NULL}
};

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
			nodePtr	node = (nodePtr)iter->data;
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
	menu = gtk_menu_new_from_model (G_MENU_MODEL (menu_model));

	action_group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP(action_group), ui_popup_item_gaction_entries, G_N_ELEMENTS (ui_popup_item_gaction_entries), NULL);
	gtk_widget_insert_action_group (menu, "item", G_ACTION_GROUP (action_group));

	/* The menu has to be attached to an application window or one of its children for access to app actions.*/
	gtk_menu_attach_to_widget (GTK_MENU (menu), liferea_shell_lookup ("mainwindow"), NULL);
	g_object_unref (menu_model);
	ui_popup_menu (menu, event);
}

void
ui_popup_enclosure_menu (enclosurePtr enclosure, const GdkEvent *event)
{
	GtkWidget	*menu;

	menu = gtk_menu_new ();

	ui_popup_add_menuitem (menu, _("Open Enclosure..."), on_popup_open_enclosure, enclosure, 0);
	ui_popup_add_menuitem (menu, _("Save As..."), on_popup_save_enclosure, enclosure, 0);
	ui_popup_add_menuitem (menu, _("Copy Link Location"), on_popup_copy_enclosure, enclosure, 0);

	ui_popup_menu (menu, event);
}

/* popup callback wrappers */

static void
ui_popup_rebuild_vfolder (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	vfolder_rebuild ((nodePtr)user_data);
}

static void
ui_popup_properties (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	nodePtr node = (nodePtr) user_data;

	NODE_TYPE (node)->request_properties (node);
}

static void
ui_popup_delete (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	feed_list_view_remove ((nodePtr)user_data);
}

static void
ui_popup_sort_feeds (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	feed_list_view_sort_folder ((nodePtr)user_data);
}

static void
ui_popup_add_convert_to_local (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	node_source_convert_to_local ((nodePtr)user_data);
}

static void
on_menu_export_items_to_file (GSimpleAction *action, GVariant *parameter, gpointer user_data)
{
	nodePtr node;
	GtkWindow *parent;
	GtkWidget *dialog;
	GtkFileChooser *chooser;
	GtkFileFilter *feed_files_filter, *all_files_filter;
	gint res;
	gchar *curname;
	const gchar *title;
	GError *err = NULL;

	node = (nodePtr) user_data;
	parent = GTK_WINDOW (liferea_shell_lookup ("mainwindow"));

	dialog = gtk_file_chooser_dialog_new (_("Save items to file"),
	                                      parent,
	                                      GTK_FILE_CHOOSER_ACTION_SAVE,
	                                      _("_Cancel"),
	                                      GTK_RESPONSE_CANCEL,
	                                      _("_Save"),
	                                      GTK_RESPONSE_ACCEPT,
	                                      NULL);
	chooser = GTK_FILE_CHOOSER (dialog);

	/* Filters are only for improving usability for now, as the code
	 * itself can only save feeds as RSS 2.0.
	 */
	feed_files_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (feed_files_filter, _("RSS 2.0 files"));
	gtk_file_filter_add_pattern (feed_files_filter, "*.rss");
	gtk_file_filter_add_pattern (feed_files_filter, "*.xml");
	gtk_file_chooser_add_filter(chooser, feed_files_filter);

	all_files_filter = gtk_file_filter_new ();
	gtk_file_filter_set_name (all_files_filter, _("All files"));
	gtk_file_filter_add_pattern (all_files_filter, "*");
	gtk_file_chooser_add_filter(chooser, all_files_filter);

	title = node_get_title (node);
	curname = g_strdup_printf("%s.rss", title != NULL ? title : _("Untitled"));
	gtk_file_chooser_set_filename (chooser, curname);
	gtk_file_chooser_set_current_name (chooser, curname);
	gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);
	g_free(curname);

	res = gtk_dialog_run (GTK_DIALOG (dialog));
	if (res == GTK_RESPONSE_ACCEPT) {
		gchar *filename = gtk_file_chooser_get_filename (chooser);
		node_save_items_to_file (node, filename, &err);
		g_free (filename);
	}

	if (err) {
		GtkWidget *errdlg;
		GtkDialogFlags flags = GTK_DIALOG_DESTROY_WITH_PARENT;
		errdlg = gtk_message_dialog_new (GTK_WINDOW (dialog),
		                                flags,
		                                GTK_MESSAGE_ERROR,
		                                GTK_BUTTONS_CLOSE,
		                                "%s",
		                                err->message);
		gtk_dialog_run (GTK_DIALOG (errdlg));
		gtk_widget_destroy (errdlg);
		g_error_free(err);
		err = NULL;
	}

	gtk_widget_destroy (dialog);
}

/* Those actions work on the node passed as user_data parameter. */
static const GActionEntry ui_popup_node_gaction_entries[] = {
  {"node-mark-all-read", on_action_mark_all_read, NULL, NULL, NULL},
  {"node-rebuild-vfolder", ui_popup_rebuild_vfolder, NULL, NULL, NULL},
  {"node-properties", ui_popup_properties, NULL, NULL, NULL},
  {"node-delete", ui_popup_delete, NULL, NULL, NULL},
  {"node-sort-feeds", ui_popup_sort_feeds, NULL, NULL, NULL},
  {"node-convert-to-local", ui_popup_add_convert_to_local, NULL, NULL, NULL},
  {"node-update", on_menu_update, NULL, NULL, NULL},
  {"node-export-items-to-file", on_menu_export_items_to_file, NULL, NULL, NULL},
};

/**
 * Shows popup menus for the feed list depending on the
 * node type.
 */
static void
ui_popup_node_menu (nodePtr node, gboolean validSelection, const GdkEvent *event)
{
	GtkWidget		*menu;
	GMenu 			*menu_model, *section;
	GSimpleActionGroup 	*action_group;
	gboolean		writeableFeedlist, isRoot, addChildren;

	if (node->parent) {
		writeableFeedlist = NODE_SOURCE_TYPE (node->parent->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST;
		isRoot = NODE_SOURCE_TYPE (node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_IS_ROOT;
		addChildren = NODE_TYPE (node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS;
	} else {
		/* if we have no parent then we have the root node... */
		writeableFeedlist = TRUE;
		isRoot = TRUE;
		addChildren = TRUE;
	}

	menu_model = g_menu_new ();
	section = g_menu_new ();

	if (validSelection) {
		if (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE)
			g_menu_append (section, _("_Update"), "node.node-update");
		else if (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE_CHILDS)
			g_menu_append (section, _("_Update Folder"), "node.node-update");
	}

	if (writeableFeedlist) {
		if (addChildren) {
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
		}

		if (isRoot && node->children) {
			/* Ending section and starting a new one to get a separator : */
			g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
			g_object_unref (section);
			section = g_menu_new ();
			g_menu_append (section, _("Sort Feeds"), "node.node-sort-feeds");
		}
	}

	if (validSelection) {
		g_menu_append_section (menu_model, NULL, G_MENU_MODEL (section));
		g_object_unref (section);
		section = g_menu_new ();
		g_menu_append (section, _("_Mark All As Read"), "node.node-mark-all-read");
		if (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_EXPORT_ITEMS) {
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
	action_group = g_simple_action_group_new ();
	g_action_map_add_action_entries (G_ACTION_MAP(action_group), ui_popup_node_gaction_entries, G_N_ELEMENTS (ui_popup_node_gaction_entries), node);
	menu = gtk_menu_new_from_model (G_MENU_MODEL (menu_model));
	gtk_widget_insert_action_group (menu, "node", G_ACTION_GROUP (action_group));
	gtk_menu_attach_to_widget (GTK_MENU (menu), liferea_shell_lookup ("mainwindow"), NULL);
	g_object_unref (menu_model);

	ui_popup_menu (menu, event);
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
	nodePtr		node = NULL;

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
	nodePtr		node = NULL;

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
