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
#include "callbacks.h"
#include "ui_popup.h"

/* from callbacks.c */
extern gint selected_type;
extern gboolean itemlist_mode;

/*------------------------------------------------------------------------------*/
/* popup menu callbacks 							*/
/*------------------------------------------------------------------------------*/

/* feed list menues */

static GtkItemFactoryEntry feedentry_menu_items[] = {
      {"/_Update Feed", 	NULL, on_popup_refresh_selected, 	0, "<StockItem>", GTK_STOCK_REFRESH },
      {"/_New",			NULL, 0, 				0, "<Branch>" },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0 },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0 },
      {"/_Delete Feed",		NULL, on_popup_delete_selected, 	0, "<StockItem>", GTK_STOCK_DELETE },
      {"/_Properties",		NULL, on_popup_prop_selected, 		0, "<StockItem>", GTK_STOCK_PROPERTIES },
      { NULL }
};

static GtkItemFactoryEntry ocsentry_menu_items[] = {
      {"/_Update Directory",	NULL, on_popup_refresh_selected, 	0, "<StockItem>", GTK_STOCK_REFRESH },
      {"/_New",			NULL, 0, 				0, "<Branch>",	  GTK_STOCK_NEW },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0 },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0 },      
      {"/_Delete Directory",	NULL, on_popup_delete_selected, 	0, "<StockItem>", GTK_STOCK_DELETE },
      {"/_Properties",		NULL, on_popup_prop_selected, 		0, "<StockItem>", GTK_STOCK_PROPERTIES },
      { NULL }
};

static GtkItemFactoryEntry node_menu_items[] = {
      {"/_New",			NULL, 0, 				0, "<Branch>",	  GTK_STOCK_NEW },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0 },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0 },
      {"/_Rename Folder",	NULL, on_popup_foldername_selected, 	0, "<StockItem>", GTK_STOCK_PROPERTIES },
      {"/_Delete Folder", 	NULL, on_popup_removefolder_selected, 	0, "<StockItem>", GTK_STOCK_DELETE },
      { NULL }
};

static GtkItemFactoryEntry vfolder_menu_items[] = {
      {"/_New",			NULL, 0, 				0, "<Branch>",	  GTK_STOCK_NEW },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0 },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0 },      
      {"/_Delete VFolder",	NULL, on_popup_delete_selected, 	0, "<StockItem>", GTK_STOCK_DELETE },
      { NULL }
};

static GtkItemFactoryEntry default_menu_items[] = {
      {"/_New",			NULL, 0, 				0, "<Branch>",	  GTK_STOCK_NEW },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0 },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0 },
      { NULL }
};

/* item list menu */

static GtkItemFactoryEntry item_menu_items[] = {
      {"/_Mark All As Read", 		NULL, on_popup_allunread_selected, 		0, NULL },
      {"/_Launch Item In Browser", 	NULL, on_popup_launchitem_selected, 		0, NULL },
      {"/Toggle Item _Flag",	 	NULL, on_toggle_item_flag, 			0, NULL },
      {"/sep1",				NULL, NULL, 					0, "<Separator>" },
      {"/_Toggle Condensed View",	NULL, on_toggle_condensed_view_selected,	0, NULL }, 
      {"/_Next Unread Item",		NULL, on_popup_next_unread_item_selected,	0, "<StockItem>", GTK_STOCK_GO_FORWARD },
/*      {"/sep2",			NULL, NULL, 					0, "<Separator>" },
      {"/_Edit Filters",		NULL, on_popup_filter_selected, 		0, NULL }*/
      { NULL }
};

/* HTML view popup menues */

static GtkItemFactoryEntry htmlview_menu_items[] = {
      {"/_Toggle Condensed View",	NULL, on_toggle_condensed_view_selected, 	0, NULL },
      {"/sep1",				NULL, NULL, 					0, "<Separator>"}, 
      {"/Zoom In",			NULL, on_popup_zoomin_selected,			0, NULL },
      {"/Zoom Out",			NULL, on_popup_zoomout_selected,		0, NULL },
      { NULL }
};

static GtkItemFactoryEntry url_menu_items[] = {
      {"/_Copy Link Location",		NULL, on_popup_copy_url_selected,		0, NULL },
      {"/_Subscribe",			NULL, on_popup_subscribe_url_selected, 		0, NULL },
      { NULL }
};

static GtkItemFactoryEntry * menues[] = {	feedentry_menu_items,
						ocsentry_menu_items,
						node_menu_items,
						vfolder_menu_items,
						default_menu_items,
						item_menu_items,
						htmlview_menu_items,
						url_menu_items
	  	    		       };

/* function to generate a generic menu specified by its number */
GtkMenu *make_menu(gint nr) {
	GtkWidget 		*menu;
	GtkItemFactory 		*item_factory;
	gint 			nmenu_items = 0;
	GtkItemFactoryEntry	*menu_items, *tmp;
	
	tmp = menu_items = menues[nr];
	/* find out how many menu items there are */
	while(NULL != (tmp->path)) {
		nmenu_items++;
		tmp ++;
	}

	item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<popup>", NULL);
	gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);
	menu = gtk_item_factory_get_widget(item_factory, "<popup>");

	return GTK_MENU(menu);
}

/* function to generate popup menus for the item list depending
   on the list mode given in itemlist_mode */
GtkMenu *make_item_menu(void) {
	GtkMenu 	*menu;
	
	if(TRUE == itemlist_mode)
		menu = make_menu(ITEM_MENU);
	else
		menu = make_menu(HTML_MENU);

	return menu;
}

/* function to generate popup menus for the feed list depending
   on the type parameter. */
static GtkMenu *make_entry_menu(gint type) {
	GtkMenu		*menu;
	
	switch(type) {
		case FST_NODE:
			menu = make_menu(NODE_MENU);
			break;
		case FST_VFOLDER:
			menu = make_menu(VFOLDER_MENU);
			break;
		case FST_PIE:
		case FST_RSS:
		case FST_CDF:
		case FST_HELPFEED:
			menu = make_menu(STDFEED_MENU);
			break;
		case FST_OPML:
		case FST_OCS:
			menu = make_menu(OCS_MENU);
			break;
		case FST_EMPTY:
			menu = NULL;
			break;
		default:
			menu = make_menu(DEFAULT_MENU);
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
	gboolean 	retval;
	gint		type;
  
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
	gboolean 	retval;
	gint		type;
  
	if (event->type != GDK_BUTTON_PRESS) return FALSE;
	eb = (GdkEventButton*) event;

	if (eb->button != 3) 
		return FALSE;

	/* right click -> popup */
	gtk_menu_popup(make_item_menu(), NULL, NULL, NULL, NULL, eb->button, eb->time);
		
	return TRUE;
}
