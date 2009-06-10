/**
 * @file ui_popup.c popup menus
 *
 * Copyright (C) 2003-2008 Lars Lindner <lars.lindner@gmail.com>
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

#include "ui/ui_popup.h"

#include <string.h>

#include "common.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "itemlist.h"
#include "net.h"
#include "newsbin.h"
#include "node.h"
#include "social.h"
#include "vfolder.h"
#include "ui/enclosure_list_view.h"
#include "ui/liferea_shell.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_itemlist.h"
#include "ui/itemview.h"
#include "ui/ui_prefs.h"
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
ui_popup_menu (GtkWidget *menu, guint button, guint32 activate_time)
{
	g_signal_connect_after (G_OBJECT(menu), "unmap-event", G_CALLBACK(gtk_widget_destroy), NULL);

	gtk_widget_show_all (menu);

	gtk_menu_popup (GTK_MENU(menu), NULL, NULL, NULL, NULL, button, activate_time);
}

static GtkWidget*
ui_popup_add_menuitem (GtkWidget *menu, const gchar *label, gpointer callback, gpointer data, const gchar *icon, gint toggle)
{
	GtkWidget	*item;
	GtkWidget	*image;

	if (toggle) {
		item = gtk_check_menu_item_new_with_label (label);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM(item), toggle - UI_POPUP_ITEM_IS_TOGGLE);
	} else {
		if (icon) {
			item = gtk_image_menu_item_new_with_label (label);
			image = gtk_image_new_from_stock (icon, GTK_ICON_SIZE_MENU);
			gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
		} else {
			item = gtk_menu_item_new_with_label (label);
		}
	}

	if (callback)
		g_signal_connect_swapped (G_OBJECT(item), "activate", G_CALLBACK(callback), data);

	gtk_menu_item_set_use_underline (GTK_MENU_ITEM(item), TRUE);
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

	ui_popup_add_menuitem (menu, _("Launch Item In _Tab"), on_popup_launchitem_in_tab_selected, NULL, NULL, 0);
	ui_popup_add_menuitem (menu, _("_Launch Item In Browser"), on_popup_launchitem_selected, NULL, NULL, 0);

	gtk_menu_shell_append (GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

	iter = newsbin_get_list ();
	if (iter) {
		GtkWidget	*item;
		GtkWidget	*submenu;
		int		i = 0;

		submenu = gtk_menu_new ();

		item = ui_popup_add_menuitem (menu, _("Copy to News Bin"), NULL, NULL, NULL, 0);

		while (iter) {
			nodePtr	node = (nodePtr)iter->data;
			ui_popup_add_menuitem (submenu, node_get_title (node), on_popup_copy_to_newsbin, GINT_TO_POINTER(i), NULL, 0);
			iter = g_slist_next (iter);
			i++;
		}

		gtk_menu_item_set_submenu (GTK_MENU_ITEM(item), submenu);

		gtk_menu_shell_append (GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
	}

	text = g_strdup_printf (_("_Bookmark Link at %s"), social_get_bookmark_site ());
	ui_popup_add_menuitem (menu, text, on_popup_social_bm_item_selected, NULL, NULL, 0);
	g_free (text);

	ui_popup_add_menuitem (menu, _("Copy Item _URL to Clipboard"), on_popup_copy_URL_clipboard, NULL, NULL, 0);
	
	gtk_menu_shell_append (GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

	ui_popup_add_menuitem (menu, _("Toggle _Read Status"), on_popup_toggle_read, NULL, GTK_STOCK_APPLY, 0);
	ui_popup_add_menuitem (menu, _("Toggle Item _Flag"), on_popup_toggle_flag, NULL, NULL, 0);
	ui_popup_add_menuitem (menu, _("R_emove Item"), on_popup_remove_selected, NULL, GTK_STOCK_DELETE, 0);

	ui_popup_menu (menu, button, activate_time);
}

void
ui_popup_enclosure_menu (enclosurePtr enclosure, guint button,
			 guint32 activate_time)
{
	GtkWidget	*menu;

	menu = gtk_menu_new ();

	ui_popup_add_menuitem (menu, _("Open Enclosure..."), on_popup_open_enclosure, enclosure, NULL, 0);
	ui_popup_add_menuitem (menu, _("Save As..."), on_popup_save_enclosure, enclosure, NULL, 0);
	ui_popup_add_menuitem (menu, _("Copy Link Location"), on_popup_copy_enclosure, enclosure, NULL, 0);

	ui_popup_menu (menu, button, activate_time);
}

void
ui_popup_systray_menu (guint button, guint32 activate_time)
{
	GtkWidget	*menu;
	GtkWidget 	*mainwindow = liferea_shell_get_window ();

	menu = gtk_menu_new ();

	ui_popup_add_menuitem (menu, _("_Work Offline"), on_onlinebtn_clicked, NULL, NULL, (!network_is_online ()) + UI_POPUP_ITEM_IS_TOGGLE);
	ui_popup_add_menuitem (menu, _("_Update All"), on_menu_update_all, NULL, GTK_STOCK_REFRESH, 0);
	ui_popup_add_menuitem (menu, _("_Preferences"), on_prefbtn_clicked, NULL, GTK_STOCK_PREFERENCES, 0);

	gtk_menu_shell_append (GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());

	ui_popup_add_menuitem (menu, _("_Show Liferea"), on_toggle_visibility, NULL, NULL, (!(gdk_window_get_state (mainwindow->window) & GDK_WINDOW_STATE_ICONIFIED) && GTK_WIDGET_VISIBLE (mainwindow)) + UI_POPUP_ITEM_IS_TOGGLE);
	ui_popup_add_menuitem (menu, _("_Quit"), on_popup_quit, NULL, GTK_STOCK_QUIT, 0);

	ui_popup_menu (menu, button, activate_time);
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
	ui_feedlist_delete_prompt ((nodePtr)callback_data);
}

/** 
 * Shows popup menus for the feed list depending on the
 * node type.
 */
static void
ui_popup_node_menu (nodePtr node, gboolean validSelection, guint button, guint32 activate_time)
{
	GtkWidget	*menu;
	gboolean	writeableFeedlist, isRoot, isHierarchic;

	menu = gtk_menu_new ();
	
	if (node->parent) {
		writeableFeedlist = NODE_SOURCE_TYPE (node->parent->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST;
		isRoot = NODE_SOURCE_TYPE (node->parent->source->root)->capabilities & NODE_SOURCE_CAPABILITY_IS_ROOT;
		isHierarchic = NODE_SOURCE_TYPE (node->parent->source->root)->capabilities & NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST;
	} else {
		/* if we have no parent then we have the root node... */
		writeableFeedlist = TRUE;
		isRoot = TRUE;
		isHierarchic = TRUE;
	}

	if (validSelection) {
		if (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE)
			ui_popup_add_menuitem (menu, _("_Update"), on_menu_update, node, GTK_STOCK_REFRESH, 0);
		else if (NODE_TYPE (node)->capabilities & NODE_CAPABILITY_UPDATE_CHILDS)
			ui_popup_add_menuitem (menu, _("_Update Folder"), on_menu_update, node, GTK_STOCK_REFRESH, 0);

		ui_popup_add_menuitem (menu, _("_Mark All As Read"), ui_popup_mark_as_read, node, GTK_STOCK_APPLY, 0);
	}

	if (writeableFeedlist) {
		if (NODE_TYPE (node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS) {
			GtkWidget	*item;
			GtkWidget	*submenu;

			submenu = gtk_menu_new ();

			item = ui_popup_add_menuitem (menu, _("_New"), NULL, NULL, NULL, 0);

			ui_popup_add_menuitem (submenu, _("New _Subscription..."), ui_popup_add_feed, NULL, NULL, 0);
			
			if (isHierarchic)
				ui_popup_add_menuitem (submenu, _("New _Folder..."), ui_popup_add_folder, NULL, NULL, 0);
				
			if (isRoot) {
				ui_popup_add_menuitem (submenu, _("New S_earch Folder..."), ui_popup_add_vfolder, NULL, NULL, 0);
				ui_popup_add_menuitem (submenu, _("New S_ource..."), ui_popup_add_source, NULL, NULL, 0);
				ui_popup_add_menuitem (submenu, _("New _News Bin..."), ui_popup_add_newsbin, NULL, NULL, 0);
			}

			gtk_menu_item_set_submenu (GTK_MENU_ITEM(item), submenu);
		}
	}

	if (validSelection) {
		if (writeableFeedlist) {
			ui_popup_add_menuitem (menu, _("_Delete"), ui_popup_delete, node, GTK_STOCK_DELETE, 0);
			ui_popup_add_menuitem (menu, _("_Properties..."), ui_popup_properties, node, GTK_STOCK_PROPERTIES, 0);
		}
	}

	ui_popup_menu (menu, button, activate_time);
}

/*------------------------------------------------------------------------------*/
/* mouse button handler 							*/
/*------------------------------------------------------------------------------*/

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
	g_assert (treeview);

	if (event->type != GDK_BUTTON_PRESS)
		return FALSE;

	eb = (GdkEventButton*)event;

	/* determine node */	
	if (!gtk_tree_view_get_path_at_pos (GTK_TREE_VIEW (treeview), event->x, event->y, &path, NULL, NULL, NULL)) {
		selected = FALSE;
		node = feedlist_get_root ();
	} else {
		model = gtk_tree_view_get_model (GTK_TREE_VIEW (treeview));
		gtk_tree_model_get_iter (model, &iter, path);
		gtk_tree_path_free (path);
		gtk_tree_model_get (model, &iter, FS_PTR, &node, -1);
	}
	
	/* apply action */
	switch (eb->button) {
		default:
			/* Shouldn't happen... */
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
				ui_feedlist_select (node);
			} else {
				/* This happens when an "empty" node or nothing (feed list root) is clicked */
				selected = FALSE;
				node = feedlist_get_root ();
			}

			ui_popup_node_menu (node, selected, eb->button, eb->time);
			break;
	}
			
	return TRUE;
}
