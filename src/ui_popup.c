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

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include "feed.h"
#include "folder.h"
#include "callbacks.h"

/* from callbacks.c */
extern gint selected_type;
extern gboolean itemlist_mode;

/*------------------------------------------------------------------------------*/
/* popup menu callbacks 							*/
/*------------------------------------------------------------------------------*/

static GtkItemFactoryEntry feedentry_menu_items[] = {
      {"/_Update Feed", 	NULL, on_popup_refresh_selected, 	0, "<StockItem>", GTK_STOCK_REFRESH },
      {"/_New",			NULL, 0, 				0, "<Branch>" },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0, "<StockItem>", GTK_STOCK_NEW },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0, "<StockItem>", GTK_STOCK_NEW },
      {"/_Delete Feed",		NULL, on_popup_delete_selected, 	0, "<StockItem>", GTK_STOCK_DELETE },
      {"/_Properties",		NULL, on_popup_prop_selected, 		0, "<StockItem>", GTK_STOCK_PROPERTIES }
};

static GtkItemFactoryEntry ocsentry_menu_items[] = {
      {"/_Update Directory",	NULL, on_popup_refresh_selected, 	0, "<StockItem>", GTK_STOCK_REFRESH },
      {"/_New",			NULL, 0, 				0, "<Branch>",	  GTK_STOCK_NEW },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0, "<StockItem>", GTK_STOCK_NEW },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0, "<StockItem>", GTK_STOCK_NEW },      
      {"/_Delete Directory",	NULL, on_popup_delete_selected, 	0, "<StockItem>", GTK_STOCK_DELETE },
      {"/_Properties",		NULL, on_popup_prop_selected, 		0, "<StockItem>", GTK_STOCK_PROPERTIES }
};

static GtkItemFactoryEntry node_menu_items[] = {
      {"/_New",			NULL, 0, 				0, "<Branch>",	  GTK_STOCK_NEW },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0, "<StockItem>", GTK_STOCK_NEW },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0, "<StockItem>", GTK_STOCK_NEW },
      {"/_Rename Folder",	NULL, on_popup_foldername_selected, 	0, "<StockItem>", GTK_STOCK_PROPERTIES },
      {"/_Delete Folder", 	NULL, on_popup_removefolder_selected, 	0, "<StockItem>", GTK_STOCK_DELETE }
};

static GtkItemFactoryEntry vfolder_menu_items[] = {
      {"/_New",			NULL, 0, 				0, "<Branch>",	  GTK_STOCK_NEW },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0, "<StockItem>", GTK_STOCK_NEW },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0, "<StockItem>", GTK_STOCK_NEW },      
      {"/_Delete VFolder",	NULL, on_popup_delete_selected, 	0, "<StockItem>", GTK_STOCK_DELETE }
};

static GtkItemFactoryEntry default_menu_items[] = {
      {"/_New",			NULL, 0, 				0, "<Branch>",	  GTK_STOCK_NEW },
      {"/_New/New _Feed", 	NULL, on_newbtn_clicked, 		0, "<StockItem>", GTK_STOCK_NEW },
      {"/_New/New F_older", 	NULL, on_popup_newfolder_selected, 	0, "<StockItem>", GTK_STOCK_NEW }
};

static GtkMenu *make_entry_menu(gint type) {
	GtkWidget 		*menubar;
	GtkItemFactory 		*item_factory;
	gint 			nmenu_items;
	GtkItemFactoryEntry	*menu_items;
	
	switch(type) {
		case FST_NODE:
			menu_items = node_menu_items;
			nmenu_items = sizeof(node_menu_items)/(sizeof(node_menu_items[0]));
			break;
		case FST_VFOLDER:
			menu_items = vfolder_menu_items;
			nmenu_items = sizeof(vfolder_menu_items)/(sizeof(vfolder_menu_items[0]));
			break;
		case FST_PIE:
		case FST_RSS:
		case FST_CDF:
		case FST_HELPFEED:
			menu_items = feedentry_menu_items;
			nmenu_items = sizeof(feedentry_menu_items)/(sizeof(feedentry_menu_items[0]));
			break;
		case FST_OPML:
		case FST_OCS:
			menu_items = ocsentry_menu_items;
			nmenu_items = sizeof(ocsentry_menu_items)/(sizeof(ocsentry_menu_items[0]));
			break;
		case FST_EMPTY:
			return NULL;
			break;
		default:
			menu_items = default_menu_items;
			nmenu_items = sizeof(default_menu_items)/(sizeof(default_menu_items[0]));
			break;
	}

	item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<feedentrypopup>", NULL);
	gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);
	menubar = gtk_item_factory_get_widget(item_factory, "<feedentrypopup>");

	return GTK_MENU(menubar);
}


static GtkItemFactoryEntry item_menu_items[] = {
      {"/_Mark All As Read", 		NULL, on_popup_allunread_selected, 		0, NULL },
      {"/_Launch Item In Browser", 	NULL, on_popup_launchitem_selected, 		0, NULL },
      {"/Toggle Item _Flag",	 	NULL, on_toggle_item_flag, 			0, NULL },
      {"/sep1",				NULL, NULL, 					0, "<Separator>" },
      {"/_Toggle Condensed View",	NULL, on_toggle_condensed_view_selected,	0, NULL }, 
      {"/_Next Unread Item",		NULL, on_popup_next_unread_item_selected,	0, "<StockItem>", GTK_STOCK_GO_FORWARD }
/*      {"/sep2",			NULL, NULL, 					0, "<Separator>" },
      {"/_Edit Filters",		NULL, on_popup_filter_selected, 		0, NULL }*/
};

static GtkItemFactoryEntry htmlview_menu_items[] = {
      {"/_Toggle Condensed View",	NULL, on_toggle_condensed_view_selected, 	0, NULL },
      {"/sep1",				NULL, NULL, 					0, "<Separator>"}, 
      {"/Zoom In",			NULL, NULL,					0, NULL },
      {"/Zoom Out",			NULL, NULL,					0, NULL }
};

GtkMenu *make_item_menu(void) {
	GtkWidget 		*menubar;
	GtkItemFactory 		*item_factory;
	gint 			nmenu_items;
	GtkItemFactoryEntry	*menu_items;
	
	if(TRUE == itemlist_mode) {
		menu_items = item_menu_items;
		nmenu_items = sizeof(item_menu_items)/(sizeof(item_menu_items[0]));
	} else {
		menu_items = htmlview_menu_items;
		nmenu_items = sizeof(htmlview_menu_items)/(sizeof(htmlview_menu_items[0]));
	}

	item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<itempopup>", NULL);
	gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);
	menubar = gtk_item_factory_get_widget(item_factory, "<itempopup>");

	return GTK_MENU(menubar);
}

GtkMenu *make_html_menu(void) {
	GtkWidget 		*menubar;
	GtkItemFactory 		*item_factory;
	gint 			nmenu_items;
	GtkItemFactoryEntry	*menu_items;
	
	menu_items = htmlview_menu_items;
	nmenu_items = sizeof(htmlview_menu_items)/(sizeof(htmlview_menu_items[0]));

	item_factory = gtk_item_factory_new(GTK_TYPE_MENU, "<htmlpopup>", NULL);
	gtk_item_factory_create_items(item_factory, nmenu_items, menu_items, NULL);
	menubar = gtk_item_factory_get_widget(item_factory, "<htmlpopup>");

	return GTK_MENU(menubar);
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
