/**
 * @file callbacks.c misc UI stuff
 *
 * Most of the GUI code is distributed over the ui_*.c
 * files but what didn't fit somewhere else stayed here.
 * 
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Christophe Barbe <christophe.barbe@ufies.org>	
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include "ui_notification.h"
	
extern GHashTable	*feedHandler;

/* all used icons */
GdkPixbuf *icons[MAX_ICONS];

/* icon names */
static gchar *iconNames[] = {	"read.xpm",		/* ICON_READ */
				"unread.png",		/* ICON_UNREAD */
				"flag.png",		/* ICON_FLAG */
				"available.png",	/* ICON_AVAILABLE */
				NULL,			/* ICON_UNAVAILABLE */
				"ocs.png",		/* ICON_OCS */
				"directory.png",	/* ICON_FOLDER */
				"vfolder.png",		/* ICON_VFOLDER */
				"empty.png",		/* ICON_EMPTY */
				"online.png",		/* ICON_ONLINE */
				"offline.png",		/* ICON_OFFLINE */
				"updating.png",		/* ICON_UPDATING */
				NULL
				};
				
/*------------------------------------------------------------------------------*/
/* generic GUI functions							*/
/*------------------------------------------------------------------------------*/

/* GUI initialization, must be called only once! */

static void callbacks_schedule_update_default_cb(nodePtr ptr) {
	feed_schedule_update((feedPtr)ptr, 0);
}

void ui_init(gboolean startIconified) {
	GtkWidget	*widget;
	int		i;

	mainwindow = ui_mainwindow_new();
	
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
		
	for(i = 0;  i < MAX_ICONS; i++)
		icons[i] = create_pixbuf(iconNames[i]);

	/* set up icons that are build from stock */
	widget = gtk_button_new();
	icons[ICON_UNAVAILABLE] = gtk_widget_render_icon(widget, GTK_STOCK_DIALOG_ERROR, GTK_ICON_SIZE_MENU, "");
	gtk_widget_destroy(widget);
	
	ui_mainwindow_update_toolbar();
	ui_mainwindow_update_menubar();
	ui_mainwindow_update_onlinebtn();
	
	ui_tray_enable(getBooleanConfValue(SHOW_TRAY_ICON));			/* init tray icon */
	ui_dnd_setup_URL_receiver(mainwindow);	/* setup URL dropping support */
	ui_popup_setup_menues();		/* create popup menues */
	conf_load_subscriptions();

	switch(getNumericConfValue(STARTUP_FEED_ACTION)) {
	case 1: /* Update all feeds */
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, callbacks_schedule_update_default_cb);
		break;
	case 2:
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (nodeActionFunc)feed_reset_update_counter);
		break;
	default:
		/* default, which is to use the lastPoll times, does not need any actions here. */;
	}
	
	/* setup 5 second timer for external subscription FIFO checking */
	g_timeout_add(5*1000, ui_feedlist_check_subscription_fifo, NULL);
	/* setup one minute timer for automatic updating, and try updating now */
 	g_timeout_add(60*1000, ui_feedlist_auto_update, NULL);
	ui_feedlist_auto_update(NULL);
	
	gtk_widget_show(mainwindow);
	ui_mainwindow_finish(mainwindow); /* Ugly hack to make mozilla work */
		
	if(startIconified)
		gtk_widget_hide(mainwindow);
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

static void on_refreshbtn_clicked_cb(nodePtr ptr) {
	feed_schedule_update((feedPtr)ptr, 0);
}

void on_refreshbtn_clicked(GtkButton *button, gpointer user_data) { 

	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, on_refreshbtn_clicked_cb);
}

void on_popup_next_unread_item_selected(void) { on_next_unread_item_activate(NULL, NULL); }
void on_nextbtn_clicked(GtkButton *button, gpointer user_data) { on_next_unread_item_activate(NULL, NULL); }

void on_popup_zoomin_selected(void) {
	ui_mainwindow_zoom_in();
}
void on_popup_zoomout_selected(void) {
	ui_mainwindow_zoom_out();
}

void on_popup_allunread_selected(void) {
	nodePtr		np;
	
	if(np = ui_feedlist_get_selected()) {
		if(IS_FOLDER(np->type)) {
			/* if we have selected a folder we mark all item of all feeds as read */
			ui_feedlist_do_for_all(np, ACTION_FILTER_FEED, (nodeActionFunc)feed_mark_all_items_read);
		} else {
			/* if not we mark all items of the item list as read */
			ui_itemlist_mark_all_as_read();
		}
		ui_feedlist_update();
	}
}

/*------------------------------------------------------------------------------*/
/* status bar callback, error box function, GUI update function			*/
/*------------------------------------------------------------------------------*/

void ui_update(void) {
	if (!lifereaStarted)
		return;
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

void on_popup_quit(gpointer callback_data,
			    guint callback_action,
			    GtkWidget *widget) {
	on_quit(NULL, NULL, NULL);
}

void on_about_activate(GtkMenuItem *menuitem, gpointer user_data) {
	GtkWidget	*dialog;

	dialog = create_aboutdialog();
	g_assert(NULL != dialog);
	gtk_widget_show(dialog);

}
