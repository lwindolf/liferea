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

#include "debug.h"
#include "interface.h"
#include "support.h"
#include "folder.h"
#include "feed.h"
#include "item.h"
#include "conf.h"
#include "export.h"
#include "htmlview.h"
#include "common.h"
#include "callbacks.h"
#include "ui_mainwindow.h"
#include "ui_folder.h"
#include "ui_feedlist.h"
#include "ui_itemlist.h"
#include "ui_tray.h"
#include "ui_queue.h"
	
extern GHashTable	*feedHandler;

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
				"online.png",		/* ICON_ONLINE */
				"offline.png",		/* ICON_OFFLINE */
				NULL
				};
				
/*------------------------------------------------------------------------------*/
/* generic GUI functions							*/
/*------------------------------------------------------------------------------*/

/* GUI initialization, must be called only once! */
void ui_init(void) {
	int i;

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

	/* order important !!! */
	ui_feedlist_init(lookup_widget(mainwindow, "feedlist"));
	ui_itemlist_init(lookup_widget(mainwindow, "Itemlist"));
	
	if(getBooleanConfValue(LAST_ITEMLIST_MODE))
		gtk_widget_activate(lookup_widget(mainwindow, "toggle_condensed_view"));

	ui_mainwindow_set_mode(itemlist_mode);
		
	for(i = 0; i < MAX_ICONS; i++)
		icons[i] = create_pixbuf(iconNames[i]);
		
	ui_mainwindow_update_toolbar();
	ui_mainwindow_update_menubar();
	ui_mainwindow_update_onlinebtn();
	
	ui_tray_enable(getBooleanConfValue(SHOW_TRAY_ICON));			/* init tray icon */
	ui_dnd_setup_URL_receiver(mainwindow);	/* setup URL dropping support */
	setupPopupMenues();			/* create popup menues */
	
	loadSubscriptions();
		
	/* setup one minute timer for automatic updating, and try updating now */
 	g_timeout_add(60*1000, ui_feedlist_auto_update, NULL);
	ui_feedlist_auto_update(NULL);
}

void ui_redraw_widget(gchar *name) {
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

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (gpointer)feed_schedule_update);
}

void on_popup_next_unread_item_selected(void) { on_next_unread_item_activate(NULL, NULL); }
void on_nextbtn_clicked(GtkButton *button, gpointer user_data) { on_next_unread_item_activate(NULL, NULL); }

void on_popup_zoomin_selected(void) {
	ui_mainwindow_zoom_in();
}
void on_popup_zoomout_selected(void) {
	ui_mainwindow_zoom_out();
}

void on_popup_copy_url_selected(gpointer url, guint callback_action, GtkWidget *widget) {
	GtkClipboard *clipboard;

	clipboard = gtk_clipboard_get(GDK_SELECTION_PRIMARY);
	gtk_clipboard_set_text(clipboard, url, -1);
 
	clipboard = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
	gtk_clipboard_set_text(clipboard, url, -1);
	
	g_free(url);
}

void on_popup_subscribe_url_selected(gpointer url, guint callback_action, GtkWidget *widget) {

	ui_feedlist_new_subscription(url, NULL, TRUE);
	g_free(url);
}

void on_popup_allunread_selected(void) {
	
	nodePtr np = ui_feedlist_get_selected();
	if (np) {
		if(IS_FOLDER(np->type)) {
			/* if we have selected a folder we mark all item of all feeds as read */
			ui_feedlist_do_for_all(np, ACTION_FILTER_FEED, (nodeActionFunc)feed_mark_all_items_read);
			ui_feedlist_update();
		} else {
			/* if not we mark all items of the item list as read */
			ui_itemlist_mark_all_as_read();
			ui_feedlist_update();
		}
	}
}

/*------------------------------------------------------------------------------*/
/* status bar callback, error box function, GUI update function			*/
/*------------------------------------------------------------------------------*/

void ui_update(void) {
	
	while(gtk_events_pending())
		gtk_main_iteration();
}

void ui_show_error_box(const char *format, ...) {
	GtkWidget	*dialog;
	va_list		args;
	gchar		*msg;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	msg = g_strdup_vprintf(format, args);
	va_end(args);
	
	dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_ERROR,
                  GTK_BUTTONS_CLOSE,
                  "%s", msg);
	gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
	g_free(msg);
}

void ui_show_info_box(const char *format, ...) { 
	GtkWidget	*dialog;
	va_list		args;
	gchar		*msg;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	msg = g_strdup_vprintf(format, args);
	va_end(args);
		
	dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_INFO,
                  GTK_BUTTONS_CLOSE,
                  "%s", msg);
	gtk_dialog_run(GTK_DIALOG (dialog));
	gtk_widget_destroy(dialog);
	g_free(msg);
}

/*------------------------------------------------------------------------------*/
/* exit handler									*/
/*------------------------------------------------------------------------------*/

gboolean on_quit(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	GtkWidget	*pane;
	gint		x,y;
	gtk_widget_hide(mainwindow);

	debug_enter("on_quit");
	conf_feedlist_save();
	
	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, (gpointer)feed_save);
	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FOLDER, (gpointer)folder_state_save);
	
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
	setNumericConfValue(LAST_ZOOMLEVEL, (gint)(100.*ui_htmlview_get_zoom(ui_mainwindow_get_active_htmlview())));
		
	gtk_main_quit();
	debug_exit("on_quit");
	return FALSE;
}

void on_about_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget	*dialog;

	dialog = create_aboutdialog();
	g_assert(NULL != dialog);
	gtk_widget_show(dialog);

}
