/**
 * @file ui_mainwindow.c some functions concerning the main window 
 *
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
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
#include <string.h>
#include "callbacks.h"
#include "common.h"
#include "conf.h"
#include "debug.h"
#include "interface.h"
#include "support.h"
#include "update.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"
#include "ui/ui_tray.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_itemlist.h"
#include "ui/ui_session.h"

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
				"edit.png",		/* ICON_UPDATED */
				NULL
				};

GtkWidget 	*mainwindow;

static GtkWidget *htmlview = NULL;		/* HTML rendering widget */
static gfloat 	zoom;				/* HTML rendering widget zoom level */

/* some prototypes */
static void ui_mainwindow_restore_position(GtkWidget *window);

GtkWidget *ui_mainwindow_get_active_htmlview(void) {

	return htmlview;
}

extern htmlviewPluginInfo *htmlviewInfo;

/*------------------------------------------------------------------------------*/
/* keyboard navigation	 							*/
/*------------------------------------------------------------------------------*/

/* Set cursor to the first item on a treeview. */
static void on_treeview_set_first(char* treename) {
	GtkTreeView	*treeview;
	GtkTreePath	*path;

	treeview = GTK_TREE_VIEW(lookup_widget(mainwindow, treename));
	path = gtk_tree_path_new_first();
	gtk_tree_view_set_cursor(treeview, path, NULL, FALSE);
	gtk_tree_path_free(path);
}

/* Move treeview cursor up and down. */
void on_treeview_move(char* treename, gint step) {
	GtkTreeView	*treeview;
	gboolean	ret;

	treeview = GTK_TREE_VIEW(lookup_widget(mainwindow, treename));
	gtk_widget_grab_focus(GTK_WIDGET(treeview));
	g_signal_emit_by_name(treeview, "move-cursor", GTK_MOVEMENT_DISPLAY_LINES, step, &ret);
}

static void on_treeview_prev(char* treename) {
	on_treeview_move(treename, -1);
}

static void on_treeview_next(char* treename) {
	on_treeview_move(treename, 1);
}

gboolean on_mainwindow_key_press_event(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	gboolean	modifier_matches = FALSE;
	guint		default_modifiers;
	const gchar	*type;
	GtkWidget	*focusw;

	if(event->type == GDK_KEY_PRESS) {
		default_modifiers = gtk_accelerator_get_default_mod_mask();

		/* handle headline skimming hotkey */
		switch(event->keyval) {
			case GDK_space:
				switch(getNumericConfValue(BROWSE_KEY_SETTING)) {
					case 0:
						modifier_matches = ((event->state & GDK_CONTROL_MASK) == GDK_CONTROL_MASK);
						break;
					default:
					case 1:
						modifier_matches = ((event->state & default_modifiers) == 0);
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
				break;
		}

		/* menu hotkeys (duplicated so they work with hidden menu */
		if(GDK_CONTROL_MASK == (event->state & default_modifiers)) {
			switch(event->keyval) {
				case GDK_KP_Add:
					on_popup_zoomin_selected(NULL, 0, NULL);
					return TRUE;
					break;
				case GDK_KP_Subtract:
					on_popup_zoomout_selected(NULL, 0, NULL);
					return TRUE;
					break;
				case GDK_n:
					on_next_unread_item_activate(NULL, NULL);
					return TRUE;
					break;
				case GDK_r:
					on_popup_allunread_selected();
					return TRUE;
					break;
				case GDK_t:
					on_toggle_item_flag(NULL, NULL);
					return TRUE;
					break;
				case GDK_u:
					on_toggle_unread_status(NULL, NULL);
					return TRUE;
					break;
				case GDK_a:
					on_refreshbtn_clicked(NULL, NULL);
					return TRUE;
					break;
				case GDK_f:
					on_searchbtn_clicked(NULL, NULL);
					return TRUE;
					break;
			}
		}

		/* prevent usage of navigation keys in entries */
		focusw = gtk_window_get_focus(GTK_WINDOW(widget));
		if(GTK_IS_ENTRY(focusw))
			return FALSE;

		/* prevent usage of navigation keys in HTML view */
		type = g_type_name(GTK_WIDGET_TYPE(focusw));
		if((NULL != type) && (0 == strcmp(type, "MozContainer")))
			return FALSE;

		/* somehow we don't need to check for GtkHTML2... */

		/* check for treeview navigation */
		if(0 == (event->state & default_modifiers)) {
			switch(event->keyval) {
				case GDK_KP_Delete:
				case GDK_Delete:
					on_remove_item_activate(NULL, NULL);
					return TRUE;
					break;
				case GDK_n: 
					on_next_unread_item_activate(NULL, NULL);
					return TRUE;
					break;
				case GDK_f:
					on_treeview_next("Itemlist");
					return TRUE;
					break;
				case GDK_b:
					on_treeview_prev("Itemlist");
					return TRUE;
					break;
				case GDK_u:
					on_treeview_prev("feedlist");
					on_treeview_set_first("Itemlist");
					return TRUE;
					break;
				case GDK_d:
					on_treeview_next("feedlist");
					on_treeview_set_first("Itemlist");
					return TRUE;
					break;
			}
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

static gboolean on_key_press_event_null_cb(GtkWidget *widget, GdkEventKey *event, gpointer data) {
	return TRUE;
}

static gboolean
on_notebook_scroll_event_null_cb (GtkWidget *widget, GdkEventScroll *event)
{
	GtkNotebook *notebook = GTK_NOTEBOOK (widget);

	GtkWidget* child;
	GtkWidget* originator;

	if (!notebook->cur_page)
		return FALSE;

	child = gtk_notebook_get_nth_page(notebook, gtk_notebook_get_current_page(notebook));
	originator = gtk_get_event_widget ((GdkEvent *)event);

	/* ignore scroll events from the content of the page */
	if (!originator || gtk_widget_is_ancestor (originator, child))
		return FALSE;

	return TRUE;
}


GtkWidget* ui_mainwindow_new(void) {

	GtkWidget *window = create_mainwindow();
	GtkWidget *toolbar = lookup_widget(window, "toolbar");
	GtkTooltips *tooltips = gtk_tooltips_new();
	gchar *toolbar_style = getStringConfValue("/desktop/gnome/interface/toolbar_style");
	
	gtk_widget_set_name(window, "lifereaMainwindow");
	gtk_widget_set_name(lookup_widget(window, "feedlist"), "feedlist");
	gtk_widget_set_name(lookup_widget(window, "Itemlist"), "itemlist");
	
	ui_mainwindow_set_toolbar_style(GTK_WINDOW(window), toolbar_style);
	g_free(toolbar_style);

	TOOLBAR_ADD(toolbar,  _("New Feed"), GTK_STOCK_ADD, tooltips,  _("Add a new subscription."), on_newbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Next Unread"), GTK_STOCK_GO_FORWARD, tooltips,  _("Jumps to the next unread item. If necessary selects the next feed with unread items."), on_nextbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Mark As Read"), GTK_STOCK_APPLY, tooltips,  _("Marks all items of the selected subscription or of all subscriptions of the selected folder as read."), on_popup_allunread_selected);
	TOOLBAR_ADD(toolbar,  _("Update All"), GTK_STOCK_REFRESH, tooltips,  _("Updates all subscriptions. This does not update OCS directories."), on_refreshbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Search"), GTK_STOCK_FIND, tooltips,  _("Search all feeds."), on_searchbtn_clicked);
	TOOLBAR_ADD(toolbar,  _("Viewing Mode"), GTK_STOCK_JUSTIFY_FILL, tooltips,  _("Switches between 2 and 3 pane mode."), on_toggle_condensed_view_clicked);
	TOOLBAR_ADD(toolbar,  _("Preferences"), GTK_STOCK_PREFERENCES, tooltips,  _("Edit preferences."), on_prefbtn_clicked);

	g_signal_connect ((gpointer) lookup_widget(window, "itemtabs"), "key_press_event",
					  G_CALLBACK (on_key_press_event_null_cb), NULL);

	g_signal_connect ((gpointer) lookup_widget(window, "itemtabs"), "key_release_event",
					  G_CALLBACK (on_key_press_event_null_cb), NULL);
	
	g_signal_connect ((gpointer) lookup_widget(window, "itemtabs"), "scroll_event",
                      G_CALLBACK (on_notebook_scroll_event_null_cb), NULL);
	
	gtk_widget_show_all(GTK_WIDGET(toolbar));

	ui_mainwindow_restore_position(window);
	
	return window;
}

void ui_mainwindow_init(int mainwindowState) {
	GtkWidget	*widget;
	int		i;
	gchar		*buffer = NULL;

	debug_enter("ui_mainwindow_init");

	mainwindow = ui_mainwindow_new();
	ui_tabs_init();

	/* load pane proportions */
	if(0 != getNumericConfValue(LAST_VPANE_POS))
		gtk_paned_set_position(GTK_PANED(lookup_widget(mainwindow, "leftpane")), getNumericConfValue(LAST_VPANE_POS));
	if(0 != getNumericConfValue(LAST_HPANE_POS))
		gtk_paned_set_position(GTK_PANED(lookup_widget(mainwindow, "rightpane")), getNumericConfValue(LAST_HPANE_POS));

	/* order important !!! */
	ui_feedlist_init(lookup_widget(mainwindow, "feedlist"));
	ui_itemlist_init(lookup_widget(mainwindow, "Itemlist"));

	/* necessary to prevent selection signals when filling the feed list
	   and setting the 2/3 pane mode view */
	gtk_widget_set_sensitive(GTK_WIDGET(lookup_widget(mainwindow, "feedlist")), FALSE);

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
	ui_enclosure_init();

	feedlist_init();
			
	if(mainwindowState == MAINWINDOW_ICONIFIED || 
	   (mainwindowState == MAINWINDOW_HIDDEN && ui_tray_get_count() == 0)) {
		gtk_window_iconify(GTK_WINDOW(mainwindow));
		gtk_widget_show(mainwindow);
	} else if(mainwindowState == MAINWINDOW_SHOWN) {
		gtk_widget_show(mainwindow);
	} else {
		/* Needed so that the window structure can be
		   accessed... otherwise will GTK warning when window is
		   shown by clicking on notification icon. */
		gtk_widget_realize(GTK_WIDGET(mainwindow)); 
	}

	ui_mainwindow_set_three_pane_mode(FALSE); /* Initializes the htmlviews */

	/* force two pane mode */
	/*   For some reason, this causes the first item to be selected and then
	     unselected... strange. */
	ui_feedlist_select(NULL);
	itemlist_set_two_pane_mode(TRUE);
	
	/* set zooming properties */	
	zoom = getNumericConfValue(LAST_ZOOMLEVEL)/100.;
	ui_htmlview_set_zoom(htmlview, zoom);
	
	/* create welcome text */
	ui_htmlview_start_output(&buffer, NULL, FALSE);
	addToHTMLBuffer(&buffer, _("<div style=\"background-color:#eee;padding:5px;border:solid 1px #aaa\">"
				   "<table border=0 cellpadding=5px><tr><td>"
				   // add application icon
				   "<img src=\""
				   PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S
				   "liferea.png\">"
				   "</td><td>"
				   "<h3>Liferea - Linux Feed Reader</h3>"
				   "</td></tr><tr><td colspan=2>"
				   "<p>Welcome to <b>Liferea</b>, a desktop news aggregator for online news "
				   "feeds.</p>"
				   "<p>The left pane contains the list of your subscriptions. To add a "
				   "subscription select Feeds -&gt; New Subscription. To browse the headlines "
				   "of a feed select it in the feed list and the headlines will be loaded "
				   "into the right pane.</p>"
				   "</tr></table>"
				   "</div>"));

	addToHTMLBuffer(&buffer, _("<div style=\"background-color:#f7f0a3;padding:5px;border:solid 1px black;margin: 5px 0 5px 0\">"
	                           "This is a highly unstable version of Liferea 1.1. This code might "
				   "be in an unusable state. It should be used by developers only! "
				   "If you want to use Liferea regularily please download the last "
				   "stable version from SourceForge!"
				   "</div>"));

	addToHTMLBuffer(&buffer, _("<iframe src=\"http://liferea.sf.net/11progress.htm\" width=100% height=150px>"
				   "</iframe>"));
				   
	ui_htmlview_finish_output(&buffer);
	ui_htmlview_write(ui_mainwindow_get_active_htmlview(), buffer, NULL);
	g_free(buffer);

	gtk_widget_set_sensitive(GTK_WIDGET(lookup_widget(mainwindow, "feedlist")), TRUE);

	debug_exit("ui_mainwindow_init");
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

/* Set the main window status bar to the text given as 
   statustext. statustext is freed afterwards. */
void ui_mainwindow_set_status_bar(const char *format, ...) {
	va_list		args;
	char 		*str = NULL;
	GtkWidget	*statusbar;
	
	g_return_if_fail(format != NULL);

	va_start(args, format);
	str = g_strdup_vprintf(format, args);
	va_end(args);

	g_assert(NULL != mainwindow);
	statusbar = lookup_widget(mainwindow, "statusbar");
	g_assert(NULL != statusbar);

	gtk_label_set_text(GTK_LABEL(GTK_STATUSBAR(statusbar)->label), str);
	g_free(str);
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

}

void ui_mainwindow_tray_remove() {
	
	if (ui_tray_get_count() == 0)
		if (!GTK_WIDGET_VISIBLE(mainwindow)) {
			ui_mainwindow_restore_position(mainwindow);
			gtk_window_present(GTK_WINDOW(mainwindow));
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

gboolean on_close(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	
	if(ui_tray_get_count() == 0)
		return on_quit(widget, event, user_data);
	ui_mainwindow_save_position();
	gtk_widget_hide(mainwindow);
	return TRUE;
}

void ui_mainwindow_toggle_visibility(GtkMenuItem *menuitem, gpointer data) {
	
	if((gdk_window_get_state(GTK_WIDGET(mainwindow)->window) & GDK_WINDOW_STATE_ICONIFIED) || !GTK_WIDGET_VISIBLE(mainwindow)) {
		ui_mainwindow_restore_position(mainwindow);
		gtk_window_present(GTK_WINDOW(mainwindow));
	} else {
		ui_mainwindow_save_position();
		gtk_widget_hide(mainwindow);
	}
}

gboolean on_mainwindow_window_state_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	if((event->type) == (GDK_WINDOW_STATE)) {
		GdkWindowState changed = ((GdkEventWindowState*)event)->changed_mask, state = ((GdkEventWindowState*)event)->new_window_state;
		
		if (changed == GDK_WINDOW_STATE_MAXIMIZED && !(state & GDK_WINDOW_STATE_WITHDRAWN)) {
			if(state & GDK_WINDOW_STATE_MAXIMIZED)
				setBooleanConfValue(LAST_WINDOW_MAXIMIZED, TRUE);
			else
				setBooleanConfValue(LAST_WINDOW_MAXIMIZED, FALSE);
		}
		if(state & GDK_WINDOW_STATE_ICONIFIED)
			session_set_cmd(NULL, MAINWINDOW_ICONIFIED);
		else if(state & GDK_WINDOW_STATE_WITHDRAWN)
			session_set_cmd(NULL, MAINWINDOW_HIDDEN);
		else
			session_set_cmd(NULL, MAINWINDOW_SHOWN);
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

static void ui_choose_file_or_dir(gchar *title, GtkWindow *parent, gchar *buttonName, gboolean saving, gboolean directory, fileChoosenCallback callback, const gchar *currentFilename, const gchar *filename, gpointer user_data) {
	GtkWidget			*dialog;
	struct file_chooser_tuple	*tuple;
	GtkWidget			*button;

	g_assert(TRUE != (saving & directory));

#if GTK_CHECK_VERSION(2,4,0)
	dialog = gtk_file_chooser_dialog_new(title,
	                                     parent,
	                                     (directory?GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
					      (saving ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN)),
	                                     GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
	                                     NULL);
	gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
	if(NULL != currentFilename)
		gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), currentFilename);
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

void ui_choose_file(gchar *title, GtkWindow *parent, gchar *buttonName, gboolean saving, fileChoosenCallback callback, const gchar *currentFilename, const gchar *filename, gpointer user_data) {

	ui_choose_file_or_dir(title, parent, buttonName, saving, FALSE, callback, currentFilename, filename, user_data);
}

void ui_choose_directory(gchar *title, GtkWindow *parent, gchar *buttonName, fileChoosenCallback callback, const gchar *currentFilename, const gchar *filename, gpointer user_data) {

	ui_choose_file_or_dir(title, parent, buttonName, FALSE, TRUE, callback, currentFilename, filename, user_data);
}
