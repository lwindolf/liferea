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
#include "ui/feed_list_node.h"
#include "ui/feed_list_view.h"
#include "ui/item_list_view.h"
#include "ui/itemview.h"
#include "ui/liferea_shell.h"
#include "ui/preferences_dialog.h"
#include "fl_sources/node_source.h"

#define UI_POPUP_ITEM_IS_TOGGLE		1

static void
on_popup_quit (void)
{
	liferea_shutdown ();
}

static void
on_toggle_visibility (void)
{
	liferea_shell_toggle_visibility ();
}

static void
on_popup_toggle_online (void)
{
	network_monitor_set_online (!network_monitor_is_online ());
}

static void
on_popup_preferences (void)
{
	preferences_dialog_open ();
}

static void
ui_popup_menu_at_pos (GtkWidget *menu, GtkMenuPositionFunc func, guint button, guint32 activate_time, gpointer user_data)
{
	g_signal_connect_after (G_OBJECT(menu), "unmap-event", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_widget_show_all (menu);

	gtk_menu_popup (GTK_MENU(menu), NULL, NULL, func, user_data, button, activate_time);
}

static void
ui_popup_menu (GtkWidget *menu, guint button, guint32 activate_time)
{
	ui_popup_menu_at_pos(menu, NULL, button, activate_time, NULL);
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

void
ui_popup_item_menu (itemPtr item, guint button, guint32 activate_time)
{
	GtkWidget	*menu;
	GSList		*iter;
	gchar		*text;

	menu = gtk_menu_new ();

	ui_popup_add_menuitem (menu, _("Open In _Tab"), on_popup_launch_item_in_tab_selected, NULL, 0);
	ui_popup_add_menuitem (menu, _("_Open In Browser"), on_popup_launch_item_selected, NULL, 0);
	ui_popup_add_menuitem (menu, _("Open In _External Browser"), on_popup_launch_item_external_selected, NULL, 0);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

	iter = newsbin_get_list ();
	if (iter) {
		GtkWidget	*item;
		GtkWidget	*submenu;
		int		i = 0;

		submenu = gtk_menu_new ();

		item = ui_popup_add_menuitem (menu, _("Copy to News Bin"), NULL, NULL, 0);

		while (iter) {
			nodePtr	node = (nodePtr)iter->data;
			ui_popup_add_menuitem (submenu, node_get_title (node), on_popup_copy_to_newsbin, GINT_TO_POINTER(i), 0);
			iter = g_slist_next (iter);
			i++;
		}

		gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);

		gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
	}

	text = g_strdup_printf (_("_Bookmark at %s"), social_get_bookmark_site ());
	ui_popup_add_menuitem (menu, text, on_popup_social_bm_item_selected, NULL, 0);
	g_free (text);

	ui_popup_add_menuitem (menu, _("Copy Item _Location"), on_popup_copy_URL_clipboard, NULL, 0);
	
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

	ui_popup_add_menuitem (menu, _("Toggle _Read Status"), on_popup_toggle_read, NULL, 0);
	ui_popup_add_menuitem (menu, _("Toggle Item _Flag"), on_popup_toggle_flag, NULL, 0);
	ui_popup_add_menuitem (menu, _("R_emove Item"), on_popup_remove_selected, NULL, 0);

	ui_popup_menu (menu, button, activate_time);
}

void
ui_popup_enclosure_menu (enclosurePtr enclosure, guint button,
			 guint32 activate_time)
{
	GtkWidget	*menu;

	menu = gtk_menu_new ();

	ui_popup_add_menuitem (menu, _("Open Enclosure..."), on_popup_open_enclosure, enclosure, 0);
	ui_popup_add_menuitem (menu, _("Save As..."), on_popup_save_enclosure, enclosure, 0);
	ui_popup_add_menuitem (menu, _("Copy Link Location"), on_popup_copy_enclosure, enclosure, 0);

	ui_popup_menu (menu, button, activate_time);
}

void
ui_popup_systray_menu (GtkMenuPositionFunc func, guint button, guint32 activate_time, gpointer user_data)
{
	GtkWidget	*menu;
	GtkWidget 	*mainwindow = liferea_shell_get_window ();

	menu = gtk_menu_new ();

	ui_popup_add_menuitem (menu, _("_Work Offline"), on_popup_toggle_online, NULL, (!network_monitor_is_online ()) + UI_POPUP_ITEM_IS_TOGGLE);
	ui_popup_add_menuitem (menu, _("_Update All"), on_menu_update_all, NULL, 0);
	ui_popup_add_menuitem (menu, _("_Preferences"), on_popup_preferences, "preferences-system", 0);

	gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

	ui_popup_add_menuitem (menu, _("_Show Liferea"), on_toggle_visibility, NULL, (!(gdk_window_get_state (gtk_widget_get_window (mainwindow)) & GDK_WINDOW_STATE_ICONIFIED) && gtk_widget_get_visible (mainwindow)) + UI_POPUP_ITEM_IS_TOGGLE);
	ui_popup_add_menuitem (menu, _("_Quit"), on_popup_quit, "application-exit", 0);

	ui_popup_menu_at_pos (menu, func, button, activate_time, user_data);
}

/* popup callback wrappers */

static void
ui_popup_mark_as_read (gpointer callback_data) 
{
	feedlist_mark_all_read ((nodePtr)callback_data);
}

static void
ui_popup_add_feed (void)
{
	node_type_request_interactive_add (feed_get_node_type ());
}

static void
ui_popup_add_folder (void)
{
	node_type_request_interactive_add (folder_get_node_type ());
}

static void
ui_popup_add_vfolder (void)
{
	node_type_request_interactive_add (vfolder_get_node_type ());
}

static void
ui_popup_rebuild_vfolder (gpointer callback_data)
{
	vfolder_rebuild ((nodePtr)callback_data);
}

static void
ui_popup_add_source (void)
{
	node_type_request_interactive_add (node_source_get_node_type ());
}

static void
ui_popup_add_newsbin (void)
{
	node_type_request_interactive_add (newsbin_get_node_type ());
}

static void
ui_popup_properties (gpointer callback_data)
{
	nodePtr node = (nodePtr) callback_data;
	
	NODE_TYPE (node)->request_properties (node);
}

static void
ui_popup_delete (gpointer callback_data)
{
	feed_list_node_remove ((nodePtr)callback_data);
}

static void
ui_popup_sort_feeds (gpointer callback_data)
{
	feed_list_view_sort_folder ((nodePtr)callback_data);
}

static void
ui_popup_add_convert_to_local (gpointer callback_data)
{
	node_source_convert_to_local ((nodePtr)callback_data);
}

/** 
 * Shows popup menus for the feed list depending on the
 * node type.
 */
static void
ui_popup_node_menu (nodePtr node, gboolean validSelection, guint button, guint32 activate_time)
{
	GtkWidget	*menu;
	gboolean	writeableFeedlist, isRoot, addChildren;

	menu = gtk_menu_new ();
	
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

	if (validSelection) {
		if (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE)
			ui_popup_add_menuitem (menu, _("_Update"), on_menu_update, node, 0);
		else if (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE_CHILDS)
			ui_popup_add_menuitem (menu, _("_Update Folder"), on_menu_update, node, 0);
	}

	if (writeableFeedlist) {
		if (addChildren) {
			GtkWidget	*item;
			GtkWidget	*submenu;

			submenu = gtk_menu_new ();

			item = ui_popup_add_menuitem (menu, _("_New"), NULL, NULL, 0);

			if (node_can_add_child_feed (node))
				ui_popup_add_menuitem (submenu, _("New _Subscription..."), ui_popup_add_feed, NULL, 0);
			
			if (node_can_add_child_folder (node))
				ui_popup_add_menuitem (submenu, _("New _Folder..."), ui_popup_add_folder, NULL, 0);
				
			if (isRoot) {
				ui_popup_add_menuitem (submenu, _("New S_earch Folder..."), ui_popup_add_vfolder, NULL, 0);
				ui_popup_add_menuitem (submenu, _("New S_ource..."), ui_popup_add_source, NULL, 0);
				ui_popup_add_menuitem (submenu, _("New _News Bin..."), ui_popup_add_newsbin, NULL, 0);
			}

			gtk_menu_item_set_submenu (GTK_MENU_ITEM(item), submenu);
		}
		
		if (isRoot && node->children) {
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
			ui_popup_add_menuitem (menu, _("Sort Feeds"), ui_popup_sort_feeds, node, 0);
		}
	}

	if (validSelection) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
		ui_popup_add_menuitem (menu, _("_Mark All As Read"), ui_popup_mark_as_read, node, 0);
	}

	if (IS_VFOLDER (node)) {
		gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
		ui_popup_add_menuitem (menu, _("_Rebuild"), ui_popup_rebuild_vfolder, node, 0);
	}

	if (validSelection) {
		if (writeableFeedlist) {
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
			ui_popup_add_menuitem (menu, _("_Delete"), ui_popup_delete, node, 0);
			ui_popup_add_menuitem (menu, _("_Preferences"), ui_popup_properties, node, 0);
		}

		if (IS_NODE_SOURCE (node) && NODE_SOURCE_TYPE (node)->capabilities & NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL) {
			gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
			ui_popup_add_menuitem (menu, _("Convert To Local Subscriptions..."), ui_popup_add_convert_to_local, node, 0);
		}
	}

	ui_popup_menu (menu, button, activate_time);
}

/* mouse button handler */
gboolean
on_mainfeedlist_button_press_event (GtkWidget *widget,
                                    GdkEventButton *event,
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
	if (gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), event->x, event->y, &path, NULL, NULL, NULL)) {
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
			ui_popup_node_menu (node, selected, eb->button, eb->time);
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

	ui_popup_node_menu (node, selected, 3, gtk_get_current_event_time ());
	return TRUE;
}
