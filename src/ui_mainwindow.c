/*
   some functions concerning the main window 
   
   Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#include "interface.h"
#include "support.h"
#include "conf.h"
#include "debug.h"
#include "callbacks.h"
#include "ui_feedlist.h"
#include "ui_mainwindow.h"
#include "ui_folder.h"
#include "ui_tray.h"
#include "ui_itemlist.h"
#include "ui_queue.h"
#include "update.h"
#include "htmlview.h"

#if GTK_CHECK_VERSION(2,4,0)
#define TOOLBAR_ADD(toolbar, label, icon, tooltips, tooltip, function) \
 do { \
	GtkToolItem *item = gtk_tool_button_new(gtk_image_new_from_stock (icon, GTK_ICON_SIZE_LARGE_TOOLBAR), label); \
     gtk_tool_item_set_tooltip(item, tooltips, tooltip, NULL); \
     g_signal_connect((gpointer) item, "clicked", G_CALLBACK(function), NULL); \
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), \
				    item, \
				    -1); \
 } while (0);
#else
#define TOOLBAR_ADD(toolbar, label, icon, tooltips, tooltip, function)      \
 gtk_toolbar_append_item(GTK_TOOLBAR(toolbar), \
				    label, \
					tooltip, \
					NULL, \
					gtk_image_new_from_stock (icon, GTK_ICON_SIZE_LARGE_TOOLBAR), \
					G_CALLBACK(function), NULL)

#endif
GtkWidget 	*mainwindow;

static GtkWidget *htmlview_two;
static GtkWidget *htmlview_three;

gboolean	itemlist_mode = TRUE;		/* TRUE means three pane, FALSE means two panes */

void ui_mainwindow_add_htmlviews() {
	htmlview_three = ui_htmlview_new(getNumericConfValue(LAST_ZOOMLEVEL));
	gtk_paned_pack2(GTK_PANED (lookup_widget(mainwindow, "rightpane")), GTK_WIDGET(htmlview_three), FALSE, TRUE);
	gtk_widget_show_all(htmlview_three);

	htmlview_two = ui_htmlview_new(getNumericConfValue(LAST_ZOOMLEVEL));
	gtk_container_add(GTK_CONTAINER(lookup_widget(mainwindow, "itemtabs")), htmlview_two);
	gtk_widget_show_all(htmlview_two);
}

GtkWidget *ui_mainwindow_get_active_htmlview() {
     if (itemlist_mode == TRUE)
          return htmlview_three;
     else
          return htmlview_two;
}

void ui_mainwindow_set_mode(gboolean threePane) {
     debug1(DEBUG_GUI, "Setting threePane mode: %s", threePane?"on":"off");

     if (threePane == TRUE)
          gtk_notebook_set_current_page(GTK_NOTEBOOK(lookup_widget(mainwindow, "itemtabs")), 0);
     else {
          gtk_notebook_set_current_page(GTK_NOTEBOOK(lookup_widget(mainwindow, "itemtabs")), 1);
          ui_htmlview_write(htmlview_two, "<html><body>TEST</body></html>");
     }
	itemlist_mode = threePane;
}


GtkWidget* ui_mainwindow_new() {
	GtkWidget *window = create_mainwindow();
	GtkWidget *toolbar = lookup_widget(window, "toolbar");
	GtkTooltips *tooltips = gtk_tooltips_new();
	gchar *toolbar_style = getStringConfValue("/desktop/gnome/interface/toolbar_style");

	
	if (!strcmp(toolbar_style, "text"))
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_TEXT);
	else if (!strcmp(toolbar_style, "both"))
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	else if (!strcmp(toolbar_style, "both_horiz"))
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);
	else /* default to icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);

	g_free(toolbar_style);

	TOOLBAR_ADD(toolbar,  _("New Feed"), GTK_STOCK_ADD, tooltips,  _("Add a new subscription."), on_newbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Next Unread"), GTK_STOCK_GO_FORWARD, tooltips,  _("Jumps to the next unread item. If necessary selects the next feed with unread items."), on_nextbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Mark As Read"), GTK_STOCK_APPLY, tooltips,  _("Mark all items of the selected subscription or of all subscriptions of the selected folder as read."), on_popup_allunread_selected);
	TOOLBAR_ADD(toolbar,  _("Update"), GTK_STOCK_REFRESH, tooltips,  _("Update all feeds."), on_refreshbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Search"), GTK_STOCK_FIND, tooltips,  _("Search all feeds."), on_searchbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Preferences"), GTK_STOCK_PREFERENCES, tooltips,  _("Edit preferences."), on_prefbtn_clicked);
	gtk_widget_show_all(GTK_WIDGET(toolbar));

	return window;
}

void ui_mainwindow_update_toolbar(void) {
	GtkWidget *widget;
	
	if(NULL != (widget = lookup_widget(mainwindow, "toolbar"))) {	
		/* to avoid "locking out" the user */
		if(getBooleanConfValue(DISABLE_MENUBAR) && getBooleanConfValue(DISABLE_TOOLBAR))
			setBooleanConfValue(DISABLE_TOOLBAR, FALSE);
			
		if(getBooleanConfValue(DISABLE_TOOLBAR))
			gtk_widget_hide(widget);
		else
			gtk_widget_show(widget);
	}
}

void ui_mainwindow_update_feed_menu(gint type) {
	gboolean enabled = IS_FEED(type) || IS_FOLDER(type);
	GtkWidget *item;
	
	item = lookup_widget(mainwindow, "properties");
	gtk_widget_set_sensitive(item, enabled);

	item = lookup_widget(mainwindow, "feed_update");
	gtk_widget_set_sensitive(item, enabled);

	item = lookup_widget(mainwindow, "delete_selected");
	gtk_widget_set_sensitive(item, enabled);

	item = lookup_widget(mainwindow, "mark_all_as_read1");
	gtk_widget_set_sensitive(item, enabled);
}

void ui_mainwindow_update_menubar(void) {
	GtkWidget *widget;
	
	if(NULL != (widget = lookup_widget(mainwindow, "menubar"))) {
		if(getBooleanConfValue(DISABLE_MENUBAR))
			gtk_widget_hide(widget);
		else
			gtk_widget_show(widget);
	}
}

void ui_mainwindow_update_onlinebtn(void) {
	GtkWidget	*widget;

	g_return_if_fail(NULL != (widget = lookup_widget(mainwindow, "onlineimage")));
	
	if(update_thread_is_online()) {
		ui_mainwindow_set_status_bar(_("Liferea is now online."));
		gtk_image_set_from_pixbuf(GTK_IMAGE(widget), icons[ICON_ONLINE]);
	} else {
		ui_mainwindow_set_status_bar(_("Liferea is now offline!"));
		gtk_image_set_from_pixbuf(GTK_IMAGE(widget), icons[ICON_OFFLINE]);
	}
}

void on_onlinebtn_clicked(GtkButton *button, gpointer user_data) {
	
	update_thread_set_online(!update_thread_is_online());
	ui_mainwindow_update_onlinebtn();

	GTK_CHECK_MENU_ITEM(lookup_widget(mainwindow, "work_offline"))->active = !update_thread_is_online();
}

void on_work_offline_activate(GtkMenuItem *menuitem, gpointer user_data) {

	update_thread_set_online(!GTK_CHECK_MENU_ITEM(menuitem)->active);
	ui_mainwindow_update_onlinebtn();
}

static void ui_mainwindow_toggle_condensed_view(void) {
	
	itemlist_mode = !itemlist_mode;
	ui_mainwindow_set_mode(itemlist_mode);
	ui_itemlist_display();
}

void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data) { 

	if(!itemlist_mode != GTK_CHECK_MENU_ITEM(menuitem)->active)
		ui_mainwindow_toggle_condensed_view();
}

void on_popup_toggle_condensed_view(gpointer cb_data, guint cb_action, GtkWidget *item) {

	if(!itemlist_mode != GTK_CHECK_MENU_ITEM(item)->active)
		ui_mainwindow_toggle_condensed_view();
}

static int ui_mainwindow_set_status_idle(gpointer data) {
	gchar		*statustext = (gchar *)data;
	GtkWidget	*statusbar;
	
	g_assert(NULL != mainwindow);
	statusbar = lookup_widget(mainwindow, "statusbar");
	g_assert(NULL != statusbar);

	gtk_label_set_text(GTK_LABEL(GTK_STATUSBAR(statusbar)->label), statustext);	
	g_free(statustext);
	return 0;
}

/* Set the main window status bar to the text given as 
   statustext. statustext is freed afterwards. */
void ui_mainwindow_set_status_bar(const char *format, ...) {
	va_list		args;
	char 		*str = NULL;
	
	g_return_if_fail(format != NULL);

	va_start(args, format);
	str = g_strdup_vprintf(format, args);
	va_end(args);

	ui_queue_add(ui_mainwindow_set_status_idle, (gpointer)str); 
}

/*
 * Feed menu callbacks
 */

void on_menu_feed_new(GtkMenuItem *menuitem, gpointer user_data) {
	on_newbtn_clicked(NULL, NULL);
}

void on_menu_feed_update(GtkMenuItem *menuitem, gpointer user_data) {
	feedPtr fp = (feedPtr)ui_feedlist_get_selected();

	on_popup_refresh_selected((gpointer)fp, 0, NULL);
}


void on_menu_folder_new (GtkMenuItem *menuitem, gpointer user_data) {
	on_popup_newfolder_selected();
}


void on_menu_delete (GtkMenuItem     *menuitem, gpointer         user_data) {
	nodePtr ptr = (nodePtr)ui_feedlist_get_selected();

	ui_feedlist_delete(ptr);
}


void on_menu_properties (GtkMenuItem *menuitem, gpointer user_data) {
	nodePtr ptr = ui_feedlist_get_selected();
	if (ptr != NULL && IS_FOLDER(ptr->type)) {
		on_popup_foldername_selected((gpointer)ptr, 0, NULL);
	} else if (ptr != NULL && IS_FEED(ptr->type)) {
		on_popup_prop_selected((gpointer)ptr, 0, NULL);
	} else {
		g_warning("You have found a bug in Liferea. You must select a node in the feedlist to do what you just did.");
	}
}


void on_menu_update (GtkMenuItem     *menuitem, gpointer         user_data) {
	nodePtr ptr = ui_feedlist_get_selected();
	
	if (ptr != NULL) {
		on_popup_refresh_selected((gpointer)ptr, 0, NULL);
	} else {
		g_warning("You have found a bug in Liferea. You must select a node in the feedlist to do what you just did.");
	}
}

