/*
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
#include "update.h"

#include "vfolder.h"	// FIXME
				
extern GAsyncQueue	*results;
extern GThread		*updateThread;

extern GHashTable	*feedHandler;

/* selection information from ui_feedlist.c and ui_itemlist.c */
extern gchar		*selected_keyprefix;
extern itemPtr		selected_ip;
extern feedPtr		selected_fp;

/* 2 or 3 pane mode flag from ui_mainwindow.c */
extern gboolean 	itemlist_mode;

/* all used icons */
GdkPixbuf *icons[MAX_ICONS];

/* icon names */
static gchar *iconNames[] = {	"read.xpm",		/* ICON_READ */
				"unread.xpm",		/* ICON_UNREAD */
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

	switchPaneMode(!getBooleanConfValue(LAST_ITEMLIST_MODE));		
	setupHTMLViews(mainwindow, lookup_widget(mainwindow, "itemview"),
			 	   lookup_widget(mainwindow, "itemlistview"),
				   getNumericConfValue(LAST_ZOOMLEVEL));
	     			     			     	
	setupFeedList(lookup_widget(mainwindow, "feedlist"));
	setupItemList(lookup_widget(mainwindow, "Itemlist"));

	ui_mainwindow_update_toolbar();
	ui_mainwindow_update_menubar();

	for(i = 0; i < MAX_ICONS; i++)
		icons[i] = create_pixbuf(iconNames[i]);
	
	updateTrayIcon();		/* init tray icon */
	setupURLReceiver(mainwindow);	/* setup URL dropping support */
	setupPopupMenues();		/* create popup menues */
}

void resetScrolling(GtkScrolledWindow *itemview) {
	GtkAdjustment	*adj;

	if(NULL != itemview) {
		adj = gtk_scrolled_window_get_vadjustment(itemview);
		gtk_adjustment_set_value(adj, 0.0);
		gtk_scrolled_window_set_vadjustment(itemview, adj);
		gtk_adjustment_value_changed(adj);

		adj = gtk_scrolled_window_get_hadjustment(itemview);
		gtk_adjustment_set_value(adj, 0.0);
		gtk_scrolled_window_set_hadjustment(itemview, adj);
		gtk_adjustment_value_changed(adj);
	} else {
		g_warning(_("internal error! could not reset HTML widget scrolling!"));
	}
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

void on_next_unread_item_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkTreeStore	*feedstore;
	feedPtr		fp;
	
	/* before scanning the feed list, we test if there is a unread 
	   item in the currently selected feed! */
	if(TRUE == findUnreadItem())
		return;
	
	/* find first feed with unread items */
	feedstore = getFeedStore();
	fp = findUnreadFeed(NULL);
	
// FIXME: workaround to prevent segfaults...
// something with the selection is buggy!
selected_ip = NULL;

	if(NULL == fp) {
		print_status(_("There are no unread items!"));
		return;	/* if we don't find a feed with unread items do nothing */
	}

	/* load found feed */
	loadItemList(fp, NULL);
	
	/* find first unread item */
	findUnreadItem();
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

	subscribeTo(FST_AUTODETECT, url, g_strdup(selected_keyprefix), TRUE);
	g_free(url);
}

/*------------------------------------------------------------------------------*/
/* status bar callback, error box function, GUI update function			*/
/*------------------------------------------------------------------------------*/

void updateUI(void) {
	
	while(gtk_events_pending())
		gtk_main_iteration();
}

void print_status(gchar *statustext) {
	GtkWidget *statusbar;
	
	g_assert(mainwindow != NULL);
	statusbar = lookup_widget(mainwindow, "statusbar");

	g_print("%s\n", statustext);

	/* lock handling, because this method may be called from main
	   and update thread */
	if(updateThread == g_thread_self())
		gdk_threads_enter();

	gtk_label_set_text(GTK_LABEL(GTK_STATUSBAR(statusbar)->label), statustext);	

	if(updateThread == g_thread_self())
		gdk_threads_leave();
}

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
	feedHandlerPtr		fhp;
	gint			type;
	gchar			*msg;

	if(NULL == (request = g_async_queue_try_pop(results)))
		return TRUE;

	gdk_threads_enter();
	if(NULL != request->new_fp) {
		type = getFeedType(request->fp);
		g_assert(NULL != feedHandler);
		if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&type))) {
			/* can happen during a long update e.g. of an OCS directory, then the type is not set, FIXME ! */
			//msg = g_strdup_printf(_("internal error! unknown feed type %d while updating feeds!"), type)
			//g_warning(msg);
			//g_free(msg);
			gdk_threads_leave();
			return TRUE;
		}

		if(TRUE == fhp->merge)
			/* If the feed type supports merging... */
			mergeFeed(request->fp, request->new_fp);
		else {
			/* Otherwise we simply use the new feed info... */
			copyFeed(request->fp, request->new_fp);
			msg = g_strdup_printf(_("\"%s\" updated..."), getFeedTitle(request->fp));
			print_status(msg);
			g_free(msg);
		}
		
		/* note this is to update the feed URL on permanent redirects */
		if(0 != strcmp(request->feedurl, getFeedSource(request->fp))) {
			setFeedSource(request->fp, g_strdup(request->feedurl));	
			msg = g_strdup_printf(_("The URL of \"%s\" has changed permanently and was updated."), getFeedTitle(request->fp));
			print_status(msg);
			g_free(msg);
		}

		if(NULL != request->fp) {
			/* now fp contains the actual feed infos */
			saveFeed(request->fp);

			if(selected_fp == request->fp) {
				clearItemList();
				loadItemList(request->fp, NULL);
				preFocusItemlist();
			}
			
			redrawFeedList();
			updateUI();
		}
	} else if(304 == request->lasthttpstatus) {
		msg = g_strdup_printf(_("\"%s\" has not changed since last update."), getFeedTitle(request->fp));
		print_status(msg);
		g_free(msg);
	} else {
		msg = g_strdup_printf(_("\"%s\" is not available!"), getFeedTitle(request->fp));
		print_status(msg);
		g_free(msg);
		request->fp->available = FALSE;
	}
	gdk_threads_leave();
		
	return TRUE;
}

/*------------------------------------------------------------------------------*/
/* exit handler									*/
/*------------------------------------------------------------------------------*/

gboolean on_quit(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	GtkWidget	*pane;
	gint		x,y;

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
