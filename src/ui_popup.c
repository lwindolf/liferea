/*
   popup menus

   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#include "feed.h"
#include "folder.h"
#include "support.h"
#include "callbacks.h"
#include "ui_popup.h"

/* from callbacks.c */
extern gint selected_type;
extern gboolean itemlist_mode;

/*------------------------------------------------------------------------------*/
/* popup menu callbacks 							*/
/*------------------------------------------------------------------------------*/

/* feed list menues */
static gint feed_menu_len = 0;
static GtkItemFactoryEntry *feed_menu_items;
static gint dir_menu_len = 0;
static GtkItemFactoryEntry *dir_menu_items;
static gint node_menu_len = 0;
static GtkItemFactoryEntry *node_menu_items;
static gint vfolder_menu_len = 0;
static GtkItemFactoryEntry *vfolder_menu_items;
static gint default_menu_len = 0;
static GtkItemFactoryEntry *default_menu_items;

/* item list menues */
static gint item_menu_len = 0;
static GtkItemFactoryEntry *item_menu_items;
static gint html_menu_len = 0;
static GtkItemFactoryEntry *html_menu_items;
static gint url_menu_len = 0;
static GtkItemFactoryEntry *url_menu_items;

static void addPopupOption(GtkItemFactoryEntry **menu, gint *menu_len, gchar *path, gchar *acc, 
			   GtkItemFactoryCallback cb, guint cb_action, gchar *item_type, gconstpointer extra_data) {
	
	(*menu_len)++;
	*menu = (GtkItemFactoryEntry *)g_realloc(*menu, sizeof(GtkItemFactoryEntry)*(*menu_len));
	if(NULL == (*menu)) {
		g_error(_("could not allocate memory!"));
		exit(1);
	}
	
	(*menu + *menu_len - 1)->path 			= path;
	(*menu + *menu_len - 1)->accelerator		= acc;
	(*menu + *menu_len - 1)->callback		= cb;
	(*menu + *menu_len - 1)->callback_action	= cb_action;
	(*menu + *menu_len - 1)->item_type		= item_type;
	(*menu + *menu_len - 1)->extra_data		= extra_data;
}

#define TOGGLE_CONDENSED_VIEW	"/Condensed View"

/* prepares the popup menues */
void setupPopupMenues(void) {

	/* feed list menues */
	feed_menu_items = NULL;
	feed_menu_len = 0;
	addPopupOption(&feed_menu_items, &feed_menu_len, _("/_Update Feed"), 		NULL, on_popup_refresh_selected, 	0, "<StockItem>", GTK_STOCK_REFRESH);
	addPopupOption(&feed_menu_items, &feed_menu_len, _("/_New"),			NULL, 0, 				0, "<Branch>", 0);
	addPopupOption(&feed_menu_items, &feed_menu_len, _("/_New/New _Feed"), 		NULL, on_newbtn_clicked, 		0, NULL, 0);
	addPopupOption(&feed_menu_items, &feed_menu_len, _("/_New/New F_older"), 	NULL, on_popup_newfolder_selected, 	0, NULL, 0);
	addPopupOption(&feed_menu_items, &feed_menu_len, _("/_Delete Feed"),		NULL, on_popup_delete_selected, 	0, "<StockItem>", GTK_STOCK_DELETE);
	addPopupOption(&feed_menu_items, &feed_menu_len, _("/_Properties"),		NULL, on_popup_prop_selected, 		0, "<StockItem>", GTK_STOCK_PROPERTIES );

	dir_menu_items = NULL;
	dir_menu_len = 0;
	addPopupOption(&dir_menu_items, &dir_menu_len, _("/_Update Directory"),		NULL, on_popup_refresh_selected, 	0, "<StockItem>", GTK_STOCK_REFRESH);
	addPopupOption(&dir_menu_items, &dir_menu_len, _("/_New"),			NULL, 0, 				0, "<Branch>", GTK_STOCK_NEW);
	addPopupOption(&dir_menu_items, &dir_menu_len, _("/_New/New _Feed"), 		NULL, on_newbtn_clicked, 		0, NULL, 0);
	addPopupOption(&dir_menu_items, &dir_menu_len, _("/_New/New F_older"), 		NULL, on_popup_newfolder_selected, 	0, NULL, 0);
	addPopupOption(&dir_menu_items, &dir_menu_len, _("/_Delete Directory"),		NULL, on_popup_delete_selected, 	0, "<StockItem>", GTK_STOCK_DELETE);
	addPopupOption(&dir_menu_items, &dir_menu_len, _("/_Properties"),		NULL, on_popup_prop_selected, 		0, "<StockItem>", GTK_STOCK_PROPERTIES);

	node_menu_items = NULL;
	node_menu_len = 0;
	addPopupOption(&node_menu_items, &node_menu_len, _("/_New"),			NULL, 0, 				0, "<Branch>", GTK_STOCK_NEW);
	addPopupOption(&node_menu_items, &node_menu_len, _("/_New/New _Feed"), 		NULL, on_newbtn_clicked, 		0, NULL, 0);
	addPopupOption(&node_menu_items, &node_menu_len, _("/_New/New F_older"), 	NULL, on_popup_newfolder_selected, 	0, NULL, 0);
	addPopupOption(&node_menu_items, &node_menu_len, _("/_Rename Folder"),		NULL, on_popup_foldername_selected, 	0, "<StockItem>", GTK_STOCK_PROPERTIES);
	addPopupOption(&node_menu_items, &node_menu_len, _("/_Delete Folder"), 		NULL, on_popup_removefolder_selected, 	0, "<StockItem>", GTK_STOCK_DELETE);

	vfolder_menu_items = NULL;
	vfolder_menu_len = 0;
	addPopupOption(&vfolder_menu_items, &vfolder_menu_len, _("/_New"),		NULL, 0, 				0, "<Branch>", GTK_STOCK_NEW);
	addPopupOption(&vfolder_menu_items, &vfolder_menu_len, _("/_New/New _Feed"), 	NULL, on_newbtn_clicked, 		0, NULL, 0);
	addPopupOption(&vfolder_menu_items, &vfolder_menu_len, _("/_New/New F_older"), 	NULL, on_popup_newfolder_selected, 	0, NULL, 0);
	addPopupOption(&vfolder_menu_items, &vfolder_menu_len, _("/_Delete VFolder"),	NULL, on_popup_delete_selected, 	0, "<StockItem>", GTK_STOCK_DELETE);

	default_menu_items = NULL;
	default_menu_len = 0;
	addPopupOption(&default_menu_items, &default_menu_len, _("/_New"),		NULL, 0, 				0, "<Branch>", GTK_STOCK_NEW);
	addPopupOption(&default_menu_items, &default_menu_len, _("/_New/New _Feed"), 	NULL, on_newbtn_clicked, 		0, NULL, 0);
	addPopupOption(&default_menu_items, &default_menu_len, _("/_New/New F_older"), 	NULL, on_popup_newfolder_selected, 	0, NULL, 0);
	
	/* item list menues */
	item_menu_items = NULL;
	item_menu_len = 0;
	addPopupOption(&item_menu_items, &item_menu_len, _("/_Mark All As Read"),	NULL, on_popup_allunread_selected, 		0, NULL, 0);
	addPopupOption(&item_menu_items, &item_menu_len, _("/_Next Unread Item"),	NULL, on_popup_next_unread_item_selected,	0, "<StockItem>", GTK_STOCK_GO_FORWARD);
	addPopupOption(&item_menu_items, &item_menu_len, "/",				NULL, NULL, 					0, "<Separator>", 0);
	addPopupOption(&item_menu_items, &item_menu_len, _("/_Launch Item In Browser"), NULL, on_popup_launchitem_selected, 		0, NULL, 0);
	addPopupOption(&item_menu_items, &item_menu_len, "/"	,			NULL, NULL, 					0, "<Separator>", 0);
	addPopupOption(&item_menu_items, &item_menu_len, _(TOGGLE_CONDENSED_VIEW),	NULL, on_popup_toggle_condensed_view,		0, "<ToggleItem>", 0);
/*      {"/sep2",			NULL, NULL, 					0, "<Separator>" },
    	{"/_Edit Filters",		NULL, on_popup_filter_selected, 		0, NULL },*/

	/* HTML view popup menues */
	html_menu_items = NULL;
	html_menu_len = 0;
	addPopupOption(&html_menu_items, &html_menu_len, _("/Zoom In"),			NULL, on_popup_zoomin_selected,		0, "<StockItem>", GTK_STOCK_ZOOM_IN);
	addPopupOption(&html_menu_items, &html_menu_len, _("/Zoom Out"),		NULL, on_popup_zoomout_selected,	0, "<StockItem>", GTK_STOCK_ZOOM_OUT);
	addPopupOption(&html_menu_items, &html_menu_len, "/",				NULL, NULL, 				0, "<Separator>", 0);
	addPopupOption(&html_menu_items, &html_menu_len, _(TOGGLE_CONDENSED_VIEW),	NULL, on_popup_toggle_condensed_view, 	0, "<ToggleItem>", 0);
	
	url_menu_items = NULL;
	url_menu_len = 0;
	addPopupOption(&url_menu_items, &url_menu_len, _("/_Copy Link Location"),	NULL, on_popup_copy_url_selected,		0, NULL, 0);
	addPopupOption(&url_menu_items, &url_menu_len, _("/_Subscribe"),		NULL, on_popup_subscribe_url_selected, 		0, NULL, 0);
}

/* function to generate a generic menu specified by its number */
GtkMenu *make_menu(GtkItemFactoryEntry *menu_items, gint nmenu_items, gpointer cb_data) {
	GtkWidget 		*menu, *toggle;
	GtkItemFactory 		*item_factory;
	
	item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<popup>", NULL);
	gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, cb_data);
	menu = gtk_item_factory_get_widget(item_factory, "<popup>");
	
	/* check if the itemlist toogle option is in the generated menu
	   and set it appropiately */
	if(NULL != (toggle = gtk_item_factory_get_item(item_factory, TOGGLE_CONDENSED_VIEW)))
		gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(toggle), !itemlist_mode);

	return GTK_MENU(menu);
}

/* function to generate popup menus for the item list depending
   on the list mode given in itemlist_mode */
GtkMenu *make_item_menu(void) {
	GtkMenu 	*menu;
	
	if(TRUE == itemlist_mode)
		menu = make_menu(item_menu_items, item_menu_len, NULL);
	else
		menu = make_menu(html_menu_items, html_menu_len, NULL);

	return menu;
}

/* popup menu generating functions for the HTML view */
GtkMenu *make_html_menu(void) { return make_menu(html_menu_items, html_menu_len, NULL); }
GtkMenu *make_url_menu(char* url) {
	return make_menu(url_menu_items, url_menu_len, g_strdup(url));
}

/* function to generate popup menus for the feed list depending
   on the type parameter. */
static GtkMenu *make_entry_menu(gint type) {
	GtkMenu		*menu;
	
	switch(type) {
		case FST_NODE:
			menu = make_menu(node_menu_items, node_menu_len, NULL);
			break;
		case FST_VFOLDER:
			menu = make_menu(vfolder_menu_items, vfolder_menu_len, NULL);
			break;
		case FST_PIE:
		case FST_RSS:
		case FST_CDF:
		case FST_HELPFEED:
			menu = make_menu(feed_menu_items, feed_menu_len, NULL);
			break;
		case FST_OPML:
		case FST_OCS:
			menu = make_menu(dir_menu_items, dir_menu_len, NULL);
			break;
		case FST_EMPTY:
			menu = NULL;
			break;
		default:
			menu = make_menu(default_menu_items, default_menu_len, NULL);
			break;
	}
	
	/* should never come here */
	return menu;
}

/*------------------------------------------------------------------------------*/
/* mouse button handler 							*/
/*------------------------------------------------------------------------------*/

gboolean on_mainfeedlist_button_press_event(GtkWidget *widget,
					    GdkEventButton *event,
                                            gpointer user_data)
{
	GdkEventButton 	*eb;
	GtkMenu		*menu;
  
	if (event->type != GDK_BUTTON_PRESS) return FALSE;
	eb = (GdkEventButton*) event;

	if (eb->button != 3)
		return FALSE;

	// FIXME: don't use existing selection, but determine
	// which selection would result from the right mouse click
	if(NULL != (menu = make_entry_menu(selected_type)))
		gtk_menu_popup(menu, NULL, NULL, NULL, NULL, eb->button, eb->time);
		
	return TRUE;
}

gboolean on_itemlist_button_press_event(GtkWidget *widget,
					    GdkEventButton *event,
                                            gpointer user_data)
{
	GdkEventButton 	*eb;
  
	if (event->type != GDK_BUTTON_PRESS) return FALSE;
	eb = (GdkEventButton*) event;

	if (eb->button != 3) 
		return FALSE;

	/* right click -> popup */
	gtk_menu_popup(make_item_menu(), NULL, NULL, NULL, NULL, eb->button, eb->time);
		
	return TRUE;
}
