/**
 * @file ui_mainwindow.c some functions concerning the main window 
 *
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkkeysyms.h>
#include "interface.h"
#include "support.h"
#include "conf.h"
#include "debug.h"
#include "callbacks.h"
#include "ui_feedlist.h"
#include "ui_mainwindow.h"
#include "ui_folder.h"
#include "ui_tray.h"
#include "ui_htmlview.h"
#include "ui_itemlist.h"
#include "ui_session.h"
#include "ui_queue.h"
#include "update.h"

#if GTK_CHECK_VERSION(2,4,0)
#define TOOLBAR_ADD(toolbar, label, icon, tooltips, tooltip, function) \
 do { \
	GtkToolItem *item = gtk_tool_button_new(gtk_image_new_from_stock (icon, GTK_ICON_SIZE_LARGE_TOOLBAR), label); \
	gtk_tool_item_set_tooltip(item, tooltips, tooltip, NULL); \
	gtk_tool_item_set_homogeneous (item, FALSE); \
	gtk_tool_item_set_is_important (item, TRUE); \
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

static GtkWidget *htmlview = NULL;		/* HTML rendering widget */
static gfloat 	zoom;				/* HTML rendering widget zoom level */
static int trayCount = 0;
extern gboolean	startIconified; /* From main.c */

/* some prototypes */
static void ui_mainwindow_restore_position(GtkWidget *window);

GtkWidget *ui_mainwindow_get_active_htmlview(void) {

	return htmlview;
}

extern htmlviewPluginInfo *htmlviewInfo;

gboolean on_mainwindow_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	gboolean modifier_matches = FALSE;

	if((event->type == GDK_KEY_PRESS) &&
	   (event->keyval == GDK_space)) {

		switch(getNumericConfValue(BROWSE_KEY_SETTING)) {
			case 0:
				modifier_matches = ((event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK);
				break;
			default:
			case 1:
				modifier_matches = (0 == event->state);
				if(!strcmp(htmlviewInfo->name, "Mozilla")) /* Hack to make space handled in the module */
					return FALSE;
				break;
			case 2:
				modifier_matches = ((event->state & GDK_MOD1_MASK) == GDK_MOD1_MASK);
				break;
		}
		
		if(modifier_matches) {
			/* Note that this code is duplicated in mozilla/mozilla.cpp! */
			if(ui_htmlview_scroll() == FALSE)
				on_next_unread_item_activate(NULL, NULL);
			return TRUE;
		}
	}
	return FALSE;
}

void ui_mainwindow_set_three_pane_mode(gboolean threePane) {
		
	if(NULL == htmlview) {
		htmlview = ui_htmlview_new(FALSE);
		gtk_container_add(GTK_CONTAINER(lookup_widget(mainwindow, "viewportThreePaneHtml")), GTK_WIDGET(htmlview));
		gtk_widget_show(htmlview);
	}
	
	ui_htmlview_clear(htmlview);

	debug1(DEBUG_GUI, "Setting threePane mode: %s", threePane?"on":"off");
	if(threePane == TRUE) {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(lookup_widget(mainwindow, "itemtabs")), 0);
		gtk_widget_reparent(GTK_WIDGET(htmlview), lookup_widget(mainwindow, "viewportThreePaneHtml"));
	} else {
		gtk_notebook_set_current_page(GTK_NOTEBOOK(lookup_widget(mainwindow, "itemtabs")), 1);
		gtk_widget_reparent(GTK_WIDGET(htmlview), lookup_widget(mainwindow, "viewportTwoPaneHtml"));
	}
}

void ui_mainwindow_set_toolbar_style(GtkWindow *window, const gchar *toolbar_style) {
	GtkWidget *toolbar = lookup_widget(GTK_WIDGET(window), "toolbar");
	
	if (toolbar_style == NULL) /* default to icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	else if (!strcmp(toolbar_style, "text"))
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_TEXT);
	else if (!strcmp(toolbar_style, "both"))
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH);
	else if (!strcmp(toolbar_style, "both_horiz") || !strcmp(toolbar_style, "both-horiz") )
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_BOTH_HORIZ);
	else /* default to icons */
		gtk_toolbar_set_style(GTK_TOOLBAR(toolbar), GTK_TOOLBAR_ICONS);
	
}

GtkWidget* ui_mainwindow_new(void) {

	GtkWidget *window = create_mainwindow();
	GtkWidget *toolbar = lookup_widget(window, "toolbar");
	GtkTooltips *tooltips = gtk_tooltips_new();
	gchar *toolbar_style = getStringConfValue("/desktop/gnome/interface/toolbar_style");

	ui_mainwindow_set_toolbar_style(GTK_WINDOW(window), toolbar_style);
	g_free(toolbar_style);

	TOOLBAR_ADD(toolbar,  _("New Feed"), GTK_STOCK_ADD, tooltips,  _("Add a new subscription."), on_newbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Next Unread"), GTK_STOCK_GO_FORWARD, tooltips,  _("Jumps to the next unread item. If necessary selects the next feed with unread items."), on_nextbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Mark As Read"), GTK_STOCK_APPLY, tooltips,  _("Mark all items of the selected subscription or of all subscriptions of the selected folder as read."), on_popup_allunread_selected);
	TOOLBAR_ADD(toolbar,  _("Update All"), GTK_STOCK_REFRESH, tooltips,  _("Updates all subscriptions. This does not update OCS directories."), on_refreshbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Search"), GTK_STOCK_FIND, tooltips,  _("Search all feeds."), on_searchbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Preferences"), GTK_STOCK_PREFERENCES, tooltips,  _("Edit preferences."), on_prefbtn_clicked);
	gtk_widget_show_all(GTK_WIDGET(toolbar));

	ui_mainwindow_restore_position(window);
	
	return window;
}

void ui_mainwindow_finish(GtkWidget *window) {
	gchar	*buffer = NULL;

	/* force two pane mode */
	ui_feedlist_select(NULL);
	ui_itemlist_set_two_pane_mode(TRUE);
	
	/* set zooming properties */	
	zoom = getNumericConfValue(LAST_ZOOMLEVEL)/100.;
	ui_htmlview_set_zoom(htmlview, zoom);
	
	/* create welcome text */
	ui_htmlview_start_output(&buffer, NULL, FALSE);
	/*addToHTMLBuffer(&buffer, _("<div style=\"background-color:#f7f0a3;padding:5px;border:solid 1px black\">"
	                           "When Liferea starts for the first time it loads a localized default "
				   "feed list. There are already default lists for English, French, German "
				   "and Spanish. If you think you know a good set of default feeds for your "
				   "language please post them (maybe as OPML) on the <a href=\"mailto:liferea-devel@lists.sourceforge.net\">mailing list</a> "
				   "and it will be included in the next release!</div>"));*/
				   
	addToHTMLBuffer(&buffer, _("<h2>Welcome to Liferea</h2>"
	                           "<p>The left pane contains the feed list where you can add new subscriptions "
	                           "and select subscriptions to read their headlines. The right side either "
	                           "displays a list of the headlines of the selected subscription and a "
	                           "pane to view the selected headline or all headlines at once if condensed "
	                           "mode is selected.</p>"
	                           "<h3>Basic Actions</h3>"
	                           "<ul>"
	                           "<li><p><b>Add Subscription</b> - Creates a new subscription in the selected "
	                           "folder. To create a new subscription enter the feed URL or if you don't "
	                           "know it the URL of the website which provides the feed.</p></li>"
	                           "<li><p><b>Update Subscription</b> - This will update the subscription you selected. "
	                           "You may want to do this if you want to immediatly check a subscription for "
	                           "updates. Usually it is adequate to rely on the auto updating according to the "
	                           "update interval of the subscription.</p></li>"
	                           "<li><p><b>Update All</b> - This will update all your subscriptions at once. "
	                           "Again usually it is adequate to rely on the auto updating.</li>"
	                           "<li><p><b>Edit Subscription Properties</b> - Sometimes you might want to "
	                           "change the update interval, title, authentication or caching properties "
	                           "of a subscription.</p></li>"
	                           "</ul>"
	                           "<p>To learn more about Liferea you should read the documentation "
	                           "provided in the help feed or in the <a href=\"http://liferea.sourceforge.net/faq.php\">FAQ</a> available at the "
	                           "<a href=\"http://liferea.sf.net\">project homepage</a>.</p>"));
	ui_htmlview_finish_output(&buffer);
	ui_htmlview_write(ui_mainwindow_get_active_htmlview(), buffer, NULL);
	g_free(buffer);
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
	gboolean enabled = (FST_FEED == type) || (FST_FOLDER == type) || (FST_VFOLDER == type);
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
	
	if(download_is_online()) {
		ui_mainwindow_set_status_bar(_("Liferea is now online"));
		gtk_image_set_from_pixbuf(GTK_IMAGE(widget), icons[ICON_ONLINE]);
	} else {
		ui_mainwindow_set_status_bar(_("Liferea is now offline"));
		gtk_image_set_from_pixbuf(GTK_IMAGE(widget), icons[ICON_OFFLINE]);
	}
}

void on_onlinebtn_clicked(GtkButton *button, gpointer user_data) {
	
	download_set_online(!download_is_online());
	ui_mainwindow_update_onlinebtn();

	GTK_CHECK_MENU_ITEM(lookup_widget(mainwindow, "work_offline"))->active = !download_is_online();
}

void on_work_offline_activate(GtkMenuItem *menuitem, gpointer user_data) {

	download_set_online(!GTK_CHECK_MENU_ITEM(menuitem)->active);
	ui_mainwindow_update_onlinebtn();
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

void ui_mainwindow_save_position(void) {
	gint x, y, w, h;

	if(!GTK_WIDGET_VISIBLE(mainwindow))
		return;
	
	if(getBooleanConfValue(LAST_WINDOW_MAXIMIZED))
		return;

	gtk_window_get_position(GTK_WINDOW(mainwindow), &x, &y);
	gtk_window_get_size(GTK_WINDOW(mainwindow), &w, &h);

	if(x+w<0 || y+h<0 ||
	    x > gdk_screen_width() ||
	    y > gdk_screen_height())
		return;
	
	/* save window position */
	setNumericConfValue(LAST_WINDOW_X, x);
	setNumericConfValue(LAST_WINDOW_Y, y);	

	/* save window size */
	setNumericConfValue(LAST_WINDOW_WIDTH, w);
	setNumericConfValue(LAST_WINDOW_HEIGHT, h);
}

void ui_mainwindow_tray_add() {
	trayCount++;
}

void ui_mainwindow_tray_remove() {
	trayCount--;
	
	if (trayCount == 0)
		if (!GTK_WIDGET_VISIBLE(mainwindow)) {
			ui_mainwindow_restore_position(mainwindow);
			gtk_window_present(GTK_WINDOW(mainwindow));
			if (!startIconified)
				session_set_cmd(NULL, FALSE);
		}
}

/**
 * Restore the window position from the values saved into gconf. Note
 * that this does not display/present/show the mainwindow.
 */
static void ui_mainwindow_restore_position(GtkWidget *window) {
	/* load window position */
	int x, y, w, h;
	
	x = getNumericConfValue(LAST_WINDOW_X);
	y = getNumericConfValue(LAST_WINDOW_Y);
	
	w = getNumericConfValue(LAST_WINDOW_WIDTH);
	h = getNumericConfValue(LAST_WINDOW_HEIGHT);
	
	/* Restore position only if the width and height were saved */
	if(w != 0 && h != 0) {
	
		if(x >= gdk_screen_width())
			x = gdk_screen_width() - 100;
		else if(x + w < 0)
			x  = 100;

		if(y >= gdk_screen_height())
			y = gdk_screen_height() - 100;
		else if(y + w < 0)
			y  = 100;
	
		gtk_window_move(GTK_WINDOW(window), x, y);

		/* load window size */
		gtk_window_resize(GTK_WINDOW(window), w, h);
	}

	if(getBooleanConfValue(LAST_WINDOW_MAXIMIZED))
		gtk_window_maximize(GTK_WINDOW(window));
	else
		gtk_window_unmaximize(GTK_WINDOW(window));

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

void on_menu_folder_new(GtkMenuItem *menuitem, gpointer user_data) {

	on_popup_newfolder_selected();
}

void on_menu_delete(GtkMenuItem *menuitem, gpointer user_data) {
	nodePtr ptr = (nodePtr)ui_feedlist_get_selected();

	ui_feedlist_delete(ptr);
}

void on_menu_properties(GtkMenuItem *menuitem, gpointer user_data) {
	nodePtr ptr = ui_feedlist_get_selected();
	
	if((ptr != NULL) && (FST_FOLDER == ptr->type)) {
		on_popup_foldername_selected((gpointer)ptr, 0, NULL);
	} else if((ptr != NULL) && (FST_FEED == ptr->type)) {
		on_popup_prop_selected((gpointer)ptr, 0, NULL);
	} else {
		g_warning("You have found a bug in Liferea. You must select a node in the feedlist to do what you just did.");
	}
}


void on_menu_update(GtkMenuItem *menuitem, gpointer user_data) {
	nodePtr ptr = ui_feedlist_get_selected();
	
	if (ptr != NULL) {
		on_popup_refresh_selected((gpointer)ptr, 0, NULL);
	} else {
		g_warning("You have found a bug in Liferea. You must select a node in the feedlist to do what you just did.");
	}
}

gboolean on_close(GtkWidget *widget, GdkEvent *event, gpointer user_data) {

	if(trayCount == 0)
		return on_quit(widget, event, user_data);
	ui_mainwindow_save_position();
	gtk_widget_hide(mainwindow);
	return TRUE;
}

void ui_mainwindow_toggle_visibility(GtkMenuItem *menuitem, gpointer data) {

	if((gdk_window_get_state(GTK_WIDGET(mainwindow)->window) & GDK_WINDOW_STATE_ICONIFIED) || !GTK_WIDGET_VISIBLE(mainwindow)) {
		ui_mainwindow_restore_position(mainwindow);
		gtk_window_present(GTK_WINDOW(mainwindow));
		if (!startIconified)
			session_set_cmd(NULL, FALSE);
	} else {
		ui_mainwindow_save_position();
		gtk_widget_hide(mainwindow);
		if (!startIconified)
			session_set_cmd(NULL, TRUE);
	}
}

gboolean on_mainwindow_window_state_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	
	if(!GTK_WIDGET_VISIBLE(mainwindow))
		return FALSE;
	
	if((event->type) == (GDK_WINDOW_STATE)) {
		if((((GdkEventWindowState*)event)->new_window_state) & GDK_WINDOW_STATE_MAXIMIZED)
			setBooleanConfValue(LAST_WINDOW_MAXIMIZED, TRUE);
		else
			setBooleanConfValue(LAST_WINDOW_MAXIMIZED, FALSE);
	}
	return FALSE;
}

struct file_chooser_tuple {
	GtkWidget *dialog;
	fileChoosenCallback func;
	gpointer user_data;
};

#if GTK_CHECK_VERSION(2,4,0)
static void ui_choose_file_save_cb(GtkDialog *dialog, gint response_id, gpointer user_data) {
	struct file_chooser_tuple *tuple = (struct file_chooser_tuple*)user_data;
	gchar *filename;
	
	if(response_id == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
		tuple->func(filename, tuple->user_data);
		g_free(filename);
	} else {
		tuple->func(NULL, user_data);
	}
	
	gtk_widget_destroy(GTK_WIDGET(dialog));
	g_free(tuple);
}

#else

static void ui_choose_file_cb(GtkButton *button, gpointer user_data) {
	struct file_chooser_tuple *tuple = (struct file_chooser_tuple*)user_data;
	const gchar *filename;
	
	filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(tuple->dialog));
	tuple->func(filename, tuple->user_data);
	gtk_widget_destroy(GTK_WIDGET(tuple->dialog));
	g_free(tuple);
}

static void ui_choose_file_cb_canceled(GtkButton *button, gpointer user_data) {
	struct file_chooser_tuple *tuple = (struct file_chooser_tuple*)user_data;
	
	tuple->func(NULL, tuple->user_data);
	
	gtk_widget_destroy(GTK_WIDGET(tuple->dialog));
	g_free(tuple);
}
#endif

static void ui_choose_file_or_dir(gchar *title, GtkWindow *parent, gchar *buttonName, gboolean saving, gboolean directory, fileChoosenCallback callback, const gchar *filename, gpointer user_data) {
	GtkWidget			*dialog;
	struct file_chooser_tuple	*tuple;
	GtkWidget			*button;

	g_print("saving=%s directory=%s\n", saving?"TRUE":"FALSE", directory?"TRUE":"FALSE");	
	g_assert(TRUE != (saving & directory));
	
#if GTK_CHECK_VERSION(2,4,0)
	dialog = gtk_file_chooser_dialog_new(title,
	                                     parent,
	                                     (directory?GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
					      (saving ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN)),
	                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                     NULL);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	if(saving)
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), "feedlist.opml");
	tuple = g_new0(struct file_chooser_tuple, 1);
	tuple->dialog = dialog;
	tuple->func = callback;
	tuple->user_data = user_data;
	
	button = gtk_dialog_add_button(GTK_DIALOG(dialog), buttonName, GTK_RESPONSE_ACCEPT);
	GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
	gtk_widget_grab_default(button);
	
	g_signal_connect(G_OBJECT(dialog), "response",
	                 G_CALLBACK(ui_choose_file_save_cb), tuple);
	if(filename != NULL && g_file_test(filename, G_FILE_TEST_EXISTS))
		gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), filename);
	gtk_widget_show_all(dialog);
#else
	dialog = gtk_file_selection_new(title);
	gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

	tuple = g_new0(struct file_chooser_tuple, 1);
	tuple->dialog = dialog;
	tuple->func = callback;
	tuple->user_data = user_data;

	if(filename != NULL)
		gtk_file_selection_set_filename(GTK_FILE_SELECTION(dialog), filename);
		
	if(TRUE == directory)
		gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(dialog));
		
	g_signal_connect(GTK_FILE_SELECTION(dialog)->ok_button,
	                 "clicked",
	                 G_CALLBACK(ui_choose_file_cb),
	                 tuple);

	g_signal_connect(GTK_FILE_SELECTION(dialog)->cancel_button,
	                 "clicked",
	                 G_CALLBACK(ui_choose_file_cb_canceled),
	                 tuple);
	
	gtk_widget_show_all(dialog);
#endif
}

void ui_choose_file(gchar *title, GtkWindow *parent, gchar *buttonName, gboolean saving, fileChoosenCallback callback, const gchar *filename, gpointer user_data) {

	ui_choose_file_or_dir(title, parent, buttonName, saving, FALSE, callback, filename, user_data);
}

void ui_choose_directory(gchar *title, GtkWindow *parent, gchar *buttonName, fileChoosenCallback callback, const gchar *filename, gpointer user_data) {

	ui_choose_file_or_dir(title, parent, buttonName, FALSE, TRUE, callback, filename, user_data);
}

