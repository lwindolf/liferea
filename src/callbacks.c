/*
   callbacks (most of the GUI code is distributed over the ui_*.c
   files but what didn't fit somewhere else stayed here)
   
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
   Copyright (C) 2004 Christophe Barbe <christophe.barbe@ufies.org>	
   Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
   
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <string.h>

#include "interface.h"
#include "support.h"
#include "folder.h"
#include "feed.h"
#include "item.h"
#include "conf.h"
#include "htmlview.h"
#include "common.h"
#include "callbacks.h"
#include "ui_mainwindow.h"
#include "ui_folder.h"
#include "ui_itemlist.h"
#include "ui_tray.h"
#include "ui_queue.h"
#include "update.h"

#include "vfolder.h"	// FIXME
				
extern GAsyncQueue	*results;

extern GHashTable	*feedHandler;

/* selection information from ui_feedlist.c and ui_itemlist.c */
extern gchar		*selected_keyprefix;
extern feedPtr		selected_fp;

/* 2 or 3 pane mode flag from ui_mainwindow.c */
extern gboolean 	itemlist_mode;

/* all used icons */
GdkPixbuf *icons[MAX_ICONS];

/* icon names */
static gchar *iconNames[] = {	"read.xpm",		/* ICON_READ */
				"unread.png",		/* ICON_UNREAD */
				"flag.png",		/* ICON_FLAG */
				"available.png",	/* ICON_AVAILABLE */
				"unavailable.png",	/* ICON_UNAVAILABLE */
				"ocs.png",		/* ICON_OCS */
				"directory.png",	/* ICON_FOLDER */
				"help.png",		/* ICON_HELP */
				"vfolder.png",		/* ICON_VFOLDER */
				"empty.png",		/* ICON_EMPTY */
				NULL
				};
				
/*------------------------------------------------------------------------------*/
/* generic GUI functions							*/
/*------------------------------------------------------------------------------*/

/* GUI initialization, must be called only once! */
void initGUI(void) {
	int i;

	selected_keyprefix = g_strdup(ROOT_FOLDER_PREFIX);

	/* load window position */
	if((0 != getNumericConfValue(LAST_WINDOW_X)) && 
	   (0 != getNumericConfValue(LAST_WINDOW_Y)))
	   	gtk_window_move(GTK_WINDOW(mainwindow), getNumericConfValue(LAST_WINDOW_X),
					 		getNumericConfValue(LAST_WINDOW_Y));

	/* load window size */
	if((0 != getNumericConfValue(LAST_WINDOW_WIDTH)) && 
	   (0 != getNumericConfValue(LAST_WINDOW_HEIGHT)))
	   	gtk_window_resize(GTK_WINDOW(mainwindow), getNumericConfValue(LAST_WINDOW_WIDTH),
					 		  getNumericConfValue(LAST_WINDOW_HEIGHT));
	
	/* load pane proportions */
	if(0 != getNumericConfValue(LAST_VPANE_POS))
		gtk_paned_set_position(GTK_PANED(lookup_widget(mainwindow, "leftpane")), getNumericConfValue(LAST_VPANE_POS));
	if(0 != getNumericConfValue(LAST_HPANE_POS))
		gtk_paned_set_position(GTK_PANED(lookup_widget(mainwindow, "rightpane")), getNumericConfValue(LAST_HPANE_POS));

	switchPaneMode(!getBooleanConfValue(LAST_ITEMLIST_MODE));
	initHTMLViewModule();
	setupHTMLViews(mainwindow, lookup_widget(mainwindow, "itemview"),
			 	   lookup_widget(mainwindow, "itemlistview"),
				   getNumericConfValue(LAST_ZOOMLEVEL));
	setHTMLViewMode(itemlist_mode);
	
	setupFeedList(lookup_widget(mainwindow, "feedlist"));
	initItemList(lookup_widget(mainwindow, "Itemlist"));

	ui_mainwindow_update_toolbar();
	ui_mainwindow_update_menubar();

	for(i = 0; i < MAX_ICONS; i++)
		icons[i] = create_pixbuf(iconNames[i]);
	
	updateTrayIcon();		/* init tray icon */
	setupURLReceiver(mainwindow);	/* setup URL dropping support */
	setupPopupMenues();		/* create popup menues */
	ui_queue_init();		/* set up callback queue for other threads */
}

void redrawWidget(gchar *name) {
	GtkWidget	*list;
	gchar		*msg;
	
	if(NULL == mainwindow)
		return;
	
	if(NULL != (list = lookup_widget(mainwindow, name)))
		gtk_widget_queue_draw(list);
	else {
		msg = g_strdup_printf("Fatal! Could not lookup widget \"%s\"!", name);
		g_warning(msg);
		g_free(msg);
	}
}

/*------------------------------------------------------------------------------*/
/* simple callbacks which don't belong to item or feed list 			*/
/*------------------------------------------------------------------------------*/

void on_refreshbtn_clicked(GtkButton *button, gpointer user_data) { 

	updateAllFeeds(); 
}

void on_scrolldown_activate(GtkMenuItem *menuitem, gpointer user_data) {

	if (scrollItemView(lookup_widget(mainwindow, "itemview")) == FALSE)
 		on_next_unread_item_activate(menuitem, user_data);
}

void on_popup_next_unread_item_selected(void) { on_next_unread_item_activate(NULL, NULL); }
void on_nextbtn_clicked(GtkButton *button, gpointer user_data) { on_next_unread_item_activate(NULL, NULL); }

void on_popup_zoomin_selected(void) { changeZoomLevel(0.2); }
void on_popup_zoomout_selected(void) { changeZoomLevel(-0.2); }

void on_popup_copy_url_selected(gpointer url, guint callback_action, GtkWidget *widget) {
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, url, -1);
 
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, url, -1);
	
	g_free(url);
}

void on_popup_subscribe_url_selected(gpointer url, guint callback_action, GtkWidget *widget) {

	subscribeTo(FST_AUTODETECT, g_strdup(url), g_strdup(selected_keyprefix), TRUE);
	g_free(url);
}

void on_popup_allunread_selected(void) {
	GtkTreeIter	iter;
	gint		tmp_type;

	if(getFeedListIter(&iter)) {
		gtk_tree_model_get(GTK_TREE_MODEL(getFeedStore()), &iter, FS_TYPE, &tmp_type, -1);
		if(IS_NODE(tmp_type)) {
			/* if we have selected a folder we mark all item of all feeds as read */
			ui_folder_mark_all_as_read();
		} else {
			/* if not we mark all items of the item list as read */
			ui_itemlist_mark_all_as_read();
		}
	}
}

/*------------------------------------------------------------------------------*/
/* status bar callback, error box function, GUI update function			*/
/*------------------------------------------------------------------------------*/

void updateUI(void) {
	
	while(gtk_events_pending())
		gtk_main_iteration();
}

static int print_status_idle(gpointer data) {
	gchar *statustext = (gchar *)data;
	GtkWidget *statusbar;
	
	g_assert(NULL != mainwindow);
	statusbar = lookup_widget(mainwindow, "statusbar");
	g_assert(NULL != statusbar);

	g_print("%s\n", statustext);
	gtk_label_set_text(GTK_LABEL(GTK_STATUSBAR(statusbar)->label), statustext);	
	return 0;
}

void print_status(gchar *statustext) { ui_queue_add(print_status_idle, (gpointer)statustext); }

void showErrorBox(gchar *msg) {
	GtkWidget	*dialog;
	
	dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_ERROR,
                  GTK_BUTTONS_CLOSE,
                  msg);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

void showInfoBox(gchar *msg) { 
	GtkWidget	*dialog;
			
	dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_INFO,
                  GTK_BUTTONS_CLOSE,
                  msg);
	gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
}

/*------------------------------------------------------------------------------*/
/* timeout callback to check for update results					*/
/*------------------------------------------------------------------------------*/

gint checkForUpdateResults(gpointer data) {
	struct feed_request	*request = NULL;
	feedPtr			new_fp;
	feedHandlerPtr		fhp;
	gint			type;
	gchar			*msg;

	if(NULL == (request = g_async_queue_try_pop(results)))
		return TRUE;

	ui_lock();
	
	request->fp->available = TRUE;
	
	if(304 == request->lasthttpstatus) {	
		print_status(g_strdup_printf(_("\"%s\" has not changed since last update."), getFeedTitle(request->fp)));
	} else if(NULL != request->data) {
		/* determine feed type handler */
		type = getFeedType(request->fp);
		g_assert(NULL != feedHandler);
		if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&type))) {
			msg = g_strdup_printf(_("internal error! unknown feed type %d while updating feeds!"), type);
			g_warning(msg);
			g_free(msg);
			gdk_threads_leave();
			return TRUE;
		}
		
		/* parse the new downloaded feed into new_fp */
		new_fp = getNewFeedStruct();
		new_fp->source = g_strdup(request->fp->source);
		(*(fhp->readFeed))(new_fp, request->data);

		if(selected_fp == request->fp) {
			clearItemList();
		}
		
		if(TRUE == fhp->merge)
			/* If the feed type supports merging... */
			mergeFeed(request->fp, new_fp);
		else {
			/* Otherwise we simply use the new feed info... */
			copyFeed(request->fp, new_fp);
			print_status(g_strdup_printf(_("\"%s\" updated..."), getFeedTitle(request->fp)));
		}

		/* note this is to update the feed URL on permanent redirects */
		if(0 != strcmp(request->feedurl, getFeedSource(request->fp))) {
			setFeedSource(request->fp, g_strdup(request->feedurl));				
			print_status(g_strdup_printf(_("The URL of \"%s\" has changed permanently and was updated."), getFeedTitle(request->fp)));
		}

		if(NULL != request->fp) {
			/* now fp contains the actual feed infos */
			saveFeed(request->fp);

			if(selected_fp == request->fp) {
				loadItemList(request->fp, NULL);
				preFocusItemlist();
			}
			
			redrawFeedList();
			updateUI();
		}
	} else {
		
		print_status(g_strdup_printf(_("\"%s\" is not available!"), getFeedTitle(request->fp)));
		request->fp->available = FALSE;
	}

	ui_unlock();
	
	/* request structure cleanup... */
	g_free(request->feedurl);
	g_free(request->data);
		
	return TRUE;
}

/*------------------------------------------------------------------------------*/
/* exit handler									*/
/*------------------------------------------------------------------------------*/

gboolean on_quit(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	GtkWidget	*pane;
	gint		x,y;

	gtk_widget_hide(mainwindow);

	saveAllFeeds();
	saveAllFolderCollapseStates();
	
	/* save pane proportions */
	if(NULL != (pane = lookup_widget(mainwindow, "leftpane"))) {
		x = gtk_paned_get_position(GTK_PANED(pane));
		setNumericConfValue(LAST_VPANE_POS, x);
	}
	
	if(NULL != (pane = lookup_widget(mainwindow, "rightpane"))) {
		y = gtk_paned_get_position(GTK_PANED(pane));
		setNumericConfValue(LAST_HPANE_POS, y);
	}
	
	/* save window position */
	gtk_window_get_position(GTK_WINDOW(mainwindow), &x, &y);
	setNumericConfValue(LAST_WINDOW_X, x);
	setNumericConfValue(LAST_WINDOW_Y, y);	

	/* save window size */
	gtk_window_get_size(GTK_WINDOW(mainwindow), &x, &y);
	setNumericConfValue(LAST_WINDOW_WIDTH, x);
	setNumericConfValue(LAST_WINDOW_HEIGHT, y);
	
	/* save itemlist properties */
	setBooleanConfValue(LAST_ITEMLIST_MODE, !itemlist_mode);
	setNumericConfValue(LAST_ZOOMLEVEL, (gint)(100*getZoomLevel()));
		
	gtk_main_quit();
	return FALSE;
}

void on_about_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget	*dialog;

	dialog = create_aboutdialog();
	g_assert(NULL != dialog);
	gtk_widget_show(dialog);

}
