/**
 * @file ui_popup.c popup menus
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libxml/uri.h>
#include <string.h>

#include "feed.h"
#include "node.h"
#include "support.h"
#include "callbacks.h"
#include "update.h"
#include "ui/ui_popup.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_enclosure.h"
#include "ui/ui_feedlist.h"

/*------------------------------------------------------------------------------*/
/* popup menu callbacks 							*/
/*------------------------------------------------------------------------------*/

/* item list menues */
static gint item_menu_len = 0;
static GtkItemFactoryEntry *item_menu_items;
static gint html_menu_len = 0;
static GtkItemFactoryEntry *html_menu_items;
static gint url_menu_len = 0;
static GtkItemFactoryEntry *url_menu_items;

/* tray menu */
static gint tray_menu_len = 0;
static GtkItemFactoryEntry *tray_menu_items;

/* enclosures popup menu */
static gint enclosure_menu_len = 0;
static GtkItemFactoryEntry *enclosure_menu_items;

static void addPopupOption(GtkItemFactoryEntry **menu, gint *menu_len, gchar *path, gchar *acc, 
			   GtkItemFactoryCallback cb, guint cb_action, gchar *item_type, gconstpointer extra_data) {
	
	(*menu_len)++;
	*menu = (GtkItemFactoryEntry *)g_realloc(*menu, sizeof(GtkItemFactoryEntry)*(*menu_len));
	if(NULL == (*menu))
		g_error("could not allocate memory!");
	
	(*menu + *menu_len - 1)->path 			= path;
	(*menu + *menu_len - 1)->accelerator		= acc;
	(*menu + *menu_len - 1)->callback		= cb;
	(*menu + *menu_len - 1)->callback_action	= cb_action;
	(*menu + *menu_len - 1)->item_type		= item_type;
	(*menu + *menu_len - 1)->extra_data		= extra_data;
}

#define TOGGLE_CONDENSED_VIEW	"/Condensed View"

#define TOGGLE_WORK_OFFLINE     "/Work Offline"
#define TOGGLE_SHOW_WINDOW      "/Show Window"

/* prepares the popup menues */
void ui_popup_setup_menues(void) {

	/* item list menues */
	item_menu_items = NULL;
	item_menu_len = 0;
	addPopupOption(&item_menu_items, &item_menu_len, _("/Toggle _Read Status"),		NULL, on_popup_toggle_read, 			0, NULL, 0);
	addPopupOption(&item_menu_items, &item_menu_len, _("/Toggle Item _Flag"),		NULL, on_popup_toggle_flag, 			0, NULL, 0);
	addPopupOption(&item_menu_items, &item_menu_len, _("/R_emove Item"),			NULL, on_popup_remove_selected,			0, "<StockItem>", GTK_STOCK_DELETE);
	addPopupOption(&item_menu_items, &item_menu_len, "/",					NULL, NULL, 					0, "<Separator>", 0);
	addPopupOption(&item_menu_items, &item_menu_len, _("/_Next Unread Item"),		NULL, on_popup_next_unread_item_selected,	0, "<StockItem>", GTK_STOCK_GO_FORWARD);
	addPopupOption(&item_menu_items, &item_menu_len, "/",					NULL, NULL, 					0, "<Separator>", 0);
	addPopupOption(&item_menu_items, &item_menu_len, _("/Copy Item _URL to Clipboard"),     NULL, on_popup_copy_URL_clipboard,     		0, NULL, 0);
	addPopupOption(&item_menu_items, &item_menu_len, _("/Launch Item In _Tab"),		NULL, on_popup_launchitem_in_tab_selected,	0, NULL, 0);
	addPopupOption(&item_menu_items, &item_menu_len, _("/_Launch Item In Browser"), 	NULL, on_popup_launchitem_selected, 		0, NULL, 0);

	/* HTML view popup menues */
	html_menu_items = NULL;
	html_menu_len = 0;
	addPopupOption(&html_menu_items, &html_menu_len, _("/_Increase Text Size"),	NULL, on_popup_zoomin_selected,		0, "<StockItem>", GTK_STOCK_ZOOM_IN);
	addPopupOption(&html_menu_items, &html_menu_len, _("/_Decrease Text Size"),	NULL, on_popup_zoomout_selected,	0, "<StockItem>", GTK_STOCK_ZOOM_OUT);

	url_menu_items = NULL;
	url_menu_len = 0;
	addPopupOption(&url_menu_items, &url_menu_len, _("/Launch Link In _Tab"),	NULL, on_popup_open_link_in_tab_selected,	0, NULL, 0);
	addPopupOption(&url_menu_items, &url_menu_len, _("/_Launch Link In Browser"),	NULL, on_popup_launch_link_selected, 		0, NULL, 0);
	addPopupOption(&url_menu_items, &url_menu_len, "/",				NULL, NULL,		                	0, "<Separator>", 0);
	addPopupOption(&url_menu_items, &url_menu_len, _("/_Copy Link Location"),	NULL, on_popup_copy_url_selected,		0, NULL, 0);
	addPopupOption(&url_menu_items, &url_menu_len, "/",				NULL, NULL,		                	0, "<Separator>", 0);
	addPopupOption(&url_menu_items, &url_menu_len, _("/_Subscribe..."),		NULL, on_popup_subscribe_url_selected, 		0, "<StockItem>", GTK_STOCK_ADD);

	/* System tray popup menu */
	tray_menu_items = NULL;
	tray_menu_len = 0;
	addPopupOption(&tray_menu_items, &tray_menu_len, _("/Toggle _Online|Offline"),	NULL, on_onlinebtn_clicked,		0, "<CheckItem>", 0);
	addPopupOption(&tray_menu_items, &tray_menu_len, _("/_Update All"),	NULL, on_menu_update_all,		0, "<StockItem>", GTK_STOCK_REFRESH);
	addPopupOption(&tray_menu_items, &tray_menu_len, _("/_Preferences..."),	NULL, on_prefbtn_clicked,		0, "<StockItem>", GTK_STOCK_PREFERENCES);
	addPopupOption(&tray_menu_items, &tray_menu_len, "/",	                NULL, NULL,		                0, "<Separator>", 0);
	addPopupOption(&tray_menu_items, &tray_menu_len, _("/_Show Window"),	NULL, ui_mainwindow_toggle_visibility,		0, "<CheckItem>", 0);
	addPopupOption(&tray_menu_items, &tray_menu_len, _("/_Quit"),	        NULL, on_popup_quit,		                0, "<StockItem>", GTK_STOCK_QUIT);
	
	/* System tray popup menu */
	enclosure_menu_items = NULL;
	enclosure_menu_len = 0;
	addPopupOption(&enclosure_menu_items, &enclosure_menu_len, _("/Open Enclosure..."),	NULL, on_popup_open_enclosure,		0, NULL, 0);
	addPopupOption(&enclosure_menu_items, &enclosure_menu_len, _("/Save As..."),		NULL, on_popup_save_enclosure,		0, NULL, 0);
}

/* function to generate a generic menu specified by its number */
static GtkMenu *make_menu(GtkItemFactoryEntry *menu_items, gint nmenu_items, gpointer cb_data) {
	GtkWidget 		*menu, *toggle;
	GtkItemFactory 		*item_factory;
	
	item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<popup>", NULL);
	gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, cb_data);
	menu = gtk_item_factory_get_widget(item_factory, "<popup>");
	
	/* check if the itemlist toogle option is in the generated menu
	   and set it appropiately */
	if(NULL != (toggle = gtk_item_factory_get_item(item_factory, TOGGLE_CONDENSED_VIEW)))
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toggle), itemlist_get_two_pane_mode());

	/* set toggled state for work offline and show window buttons in 
	   the tray popup menu */
	if(NULL != (toggle = gtk_item_factory_get_widget(item_factory, TOGGLE_WORK_OFFLINE)))
		GTK_CHECK_MENU_ITEM(toggle)->active = !update_is_online();
	if(NULL != (toggle = gtk_item_factory_get_widget(item_factory, TOGGLE_SHOW_WINDOW)))
		GTK_CHECK_MENU_ITEM(toggle)->active = 
			!(gdk_window_get_state(GTK_WIDGET(mainwindow)->window) & GDK_WINDOW_STATE_ICONIFIED) && GTK_WIDGET_VISIBLE(mainwindow);
	return GTK_MENU(menu);
}

/** function to generate popup menus for the item list depending
   on the list mode given in itemlist_mode */

GtkMenu *make_item_menu(itemPtr ip) {
	GtkMenu 	*menu;

	if(itemlist_get_two_pane_mode())
		menu = make_menu(html_menu_items, html_menu_len, ip);
	else
		menu = make_menu(item_menu_items, item_menu_len, ip);

	return menu;
}

/** popup menu generating functions for the HTML view */
GtkMenu *make_html_menu(void) { return make_menu(html_menu_items, html_menu_len, NULL); }
GtkMenu *make_url_menu(char *url) {

	if(url == strstr(url, ENCLOSURE_PROTOCOL))
		return ui_popup_make_enclosure_menu(url);		
	else
		return make_menu(url_menu_items, url_menu_len, g_strdup(url));
}

/** popup menu generation for the tray icon */
GtkMenu *ui_popup_make_systray_menu(void) { return make_menu(tray_menu_items, tray_menu_len, NULL); }

/** popup menu generation for the enclosure popup menu*/
/* FIXME: This memleaks the enclosure URL */
GtkMenu *ui_popup_make_enclosure_menu(const gchar *url) {
	GtkMenu		*menu;
	xmlChar		*enclosure_url;

	if((enclosure_url = xmlURIUnescapeString(url + strlen(ENCLOSURE_PROTOCOL "load?"), 0, NULL))) {
		menu = make_menu(enclosure_menu_items, enclosure_menu_len, g_strdup(enclosure_url)); 
		xmlFree(enclosure_url);
		return menu;
	}

	return NULL;
}

/* popup callback wrappers */

static void ui_popup_update(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	node_request_update((nodePtr)callback_data, FEED_REQ_PRIORITY_HIGH);
}

static void ui_popup_mark_as_read(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	node_mark_all_read((nodePtr)callback_data);
}

static void ui_popup_add_feed(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	node_request_interactive_add(NODE_TYPE_FEED);
}

static void ui_popup_add_folder(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	node_request_interactive_add(NODE_TYPE_FOLDER);
}

static void ui_popup_add_vfolder(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	node_request_interactive_add(NODE_TYPE_VFOLDER);
}

static void ui_popup_add_source(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	node_request_interactive_add(NODE_TYPE_SOURCE);
}

static void ui_popup_properties(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	node_request_properties((nodePtr)callback_data);
}

static void ui_popup_delete(gpointer callback_data, guint callback_action, GtkWidget *widget) {
	ui_feedlist_delete_prompt((nodePtr)callback_data);
}

/** 
 * Generates popup menus for the feed list depending on the
 * type parameter. The node will be passed as a callback_data.
 */
static GtkMenu *ui_popup_node_menu(nodePtr node, gboolean validSelection) {
	GtkItemFactoryEntry	*menu_items = NULL;
	gint 			menu_len = 0;

	if(validSelection) {
		if(node->type == NODE_TYPE_FOLDER)
			addPopupOption(&menu_items, &menu_len, _("/_Update Folder"), 	NULL, ui_popup_update,		0, "<StockItem>", GTK_STOCK_REFRESH);

		if((node->type != NODE_TYPE_FOLDER) && (node->type != NODE_TYPE_VFOLDER))
			addPopupOption(&menu_items, &menu_len, _("/_Update"), 		NULL, ui_popup_update,		0, "<StockItem>", GTK_STOCK_REFRESH);

		addPopupOption(&menu_items, &menu_len, _("/_Mark All As Read"),		NULL, ui_popup_mark_as_read, 	0, "<StockItem>", GTK_STOCK_APPLY);
	}

	if(NODE_TYPE(node->source->root)->capabilities & NODE_CAPABILITY_ADD_CHILDS) {
		addPopupOption(&menu_items, &menu_len, _("/_New"),			NULL, 0, 			0, "<Branch>", 0);
		addPopupOption(&menu_items, &menu_len, _("/_New/New _Subscription..."),	NULL, ui_popup_add_feed, 	0, NULL, 0);
		addPopupOption(&menu_items, &menu_len, _("/_New/New _Folder..."),	NULL, ui_popup_add_folder, 	0, NULL, 0);
		addPopupOption(&menu_items, &menu_len, _("/_New/New S_earch Folder..."),NULL, ui_popup_add_vfolder,	0, NULL, 0);
		addPopupOption(&menu_items, &menu_len, _("/_New/New S_ource..."), 	NULL, ui_popup_add_source, 	0, NULL, 0);
	}

	if(validSelection) {
		if(NODE_SOURCE_TYPE(node->source->root)->capabilities & NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST) {
			if(NODE_TYPE(node->source->root)->capabilities & NODE_CAPABILITY_REMOVE_CHILDS)
				addPopupOption(&menu_items, &menu_len, _("/_Delete"),		NULL, ui_popup_delete,		0, "<StockItem>", GTK_STOCK_DELETE);

			addPopupOption(&menu_items, &menu_len, _("/_Properties..."),	NULL, ui_popup_properties, 	0, "<StockItem>", GTK_STOCK_PROPERTIES );
		}
	}

	return make_menu(menu_items, menu_len, node);
}

/*------------------------------------------------------------------------------*/
/* mouse button handler 							*/
/*------------------------------------------------------------------------------*/

gboolean on_mainfeedlist_button_press_event(GtkWidget *widget,
                                            GdkEventButton *event,
                                            gpointer user_data) {
	GdkEventButton 	*eb;
	GtkWidget	*treeview;
	GtkTreeModel	*model;
	GtkTreePath	*path;
	GtkTreeIter	iter;
	gboolean	selected = TRUE;
	nodePtr		node = NULL;

	treeview = lookup_widget(mainwindow, "feedlist");
	g_assert(treeview);

	if(event->type != GDK_BUTTON_PRESS)
		return FALSE;

	eb = (GdkEventButton*)event;

	if(eb->button != 3)
		return FALSE;

	if(!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(lookup_widget(mainwindow, "feedlist")), event->x, event->y, &path, NULL, NULL, NULL)) {
		selected=FALSE;
		node = feedlist_get_root();
	} else {
		model = gtk_tree_view_get_model(GTK_TREE_VIEW(lookup_widget(mainwindow, "feedlist")));
		gtk_tree_model_get_iter(model, &iter, path);
		gtk_tree_path_free(path);
		gtk_tree_model_get(model, &iter, FS_PTR, &node, -1);
		
		if(node) {
			ui_feedlist_select(node);
		} else {
			/* This happens when an "empty" node or nothing (feed list root) is clicked */
			selected = FALSE;
			node = feedlist_get_root();
		}
	}

	gtk_menu_popup(ui_popup_node_menu(node, selected), NULL, NULL, NULL, NULL, eb->button, eb->time);
		
	return TRUE;
}
