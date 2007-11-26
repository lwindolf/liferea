/**
 * @file ui_mainwindow.h some functions concerning the main window 
 *
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <gtk/gtk.h>
#include "ui_htmlview.h"

extern GtkWidget	*mainwindow;

enum mainwindowState {
	MAINWINDOW_SHOWN,
	MAINWINDOW_MAXIMIZED,
	MAINWINDOW_ICONIFIED,
	MAINWINDOW_HIDDEN
};
					
/**
 * Create a new main window with the given display state
 *
 * @param mainwindowState	state code MAINWINDOW_*
 */
void ui_mainwindow_init(int mainwindowState);

/**
 * Switches the mainwindow layout for the given viewing mode.
 *
 * @param newMode	new view mode (0 = normal, 1 = wide, 2 = combined
 */
void ui_mainwindow_set_layout(guint newMode);

/**
 * Method to query the active HTML view
 *
 * @returns active HTML view
 */
LifereaHtmlView * ui_mainwindow_get_active_htmlview (void);

/**
 * Sets the toolbar to a particular style
 *
 * @param toolbar_style text string containing the type of style to use
 */
void ui_mainwindow_set_toolbar_style(const gchar *toolbar_style);

/** According to the preferences this function enables/disables the toolbar */
void ui_mainwindow_update_toolbar(void);

/** 
 * Set the sensitivity of items in the feed menu based on the type of feed selected 
 *
 * @param feedActions	TRUE if feed actions are to be enabled
 * @param readWrite	TRUE if feed list modifying actions are enabled
 */
void ui_mainwindow_update_feed_menu (gboolean feedActions, gboolean readWrite);

/** 
 * Set the sensitivity of items in the item menu based on the type of item selected 
 *
 * @param itemActions	TRUE if feed actions are to be enabled
 */
void ui_mainwindow_update_item_menu (gboolean itemActions);

/** According to the preferences this function enables/disables the menubar */
void ui_mainwindow_update_menubar(void);

/**
 * Sets the status bar text. Takes printf() like parameters 
 */
void ui_mainwindow_set_status_bar(const char *format, ...);

void ui_mainwindow_update_feedsinfo(void);

/**
 * Changes the online state UI representation.
 *
 * @param online	1 = online, 0 = offline
 */
void ui_mainwindow_online_status_changed(int online);

/* don't save off-screen positioning */

/**
 * Save the current mainwindow position to gconf, if the window is
 * shown and completely on the screen.
 */
void ui_mainwindow_save_position(void);


void ui_mainwindow_tray_add(void);

void ui_mainwindow_tray_remove(void);

/**
 * Function to present the main window
 */
void ui_mainwindow_show(void);

/* GUI callbacks */
void on_onlinebtn_clicked(GtkButton *button, gpointer user_data);

void ui_mainwindow_toggle_visibility(GtkMenuItem *menuitem, gpointer data);

/** Callback to be used with the filename choosing dialog */
typedef void (*fileChoosenCallback) (const gchar *title, gpointer user_data);

/**
 * Open up a file selector
 *
 * @param title		window title
 * @param buttonName	Text to be used as the name of the accept button
 * @param saving	TRUE if saving, FALSE if opening
 * @param callback	that will be passed the filename (in the system's locale (NOT UTF-8), and some user data
 * @param currentPath	This file or directory will be selected in the chooser
 * @param filename	When saving, this is the suggested filename
 * @param user_data	user data passed to the callback
 */
void ui_choose_file(gchar *title, gchar *buttonName, gboolean saving, fileChoosenCallback callback, const gchar *currentPath, const gchar *defaultFilename, gpointer user_data);

/** 
 * Like ui_choose_file but allows to select a directory 
 */
void ui_choose_directory(gchar *title, gchar *buttonName, fileChoosenCallback callback, const gchar *currentPath, gpointer user_data);

/**
 * Move cursor to nth next iter in a tree view.
 *
 * @param treeview	tree view widget
 * @param step		how many iters to skip
 */
void on_treeview_move (GtkWidget *treeview, gint step);

void on_popup_quit(gpointer callback_data, guint callback_action, GtkWidget *widget);

void ui_show_info_box(const char *format, ...);
void ui_show_error_box(const char *format, ...);
 
#endif
