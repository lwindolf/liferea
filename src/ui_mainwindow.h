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

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <gtk/gtk.h>

extern GtkWidget	*mainwindow;

/* 2 or 3 pane mode flag from ui_mainwindow.c */
extern gboolean 	itemlist_mode;

/**
 * Create a new main window
 */
GtkWidget* ui_mainwindow_new();

void ui_mainwindow_finish(GtkWidget *window);

void ui_mainwindow_set_three_pane_mode(gboolean threePane);

GtkWidget *ui_mainwindow_get_active_htmlview();

/** Sets the toolbar to a particular style
 * @param window main window containing toolbar
 * @param toolbar_style text string containing the type of style to use
 */
void ui_mainwindow_set_toolbar_style(GtkWindow *window, const gchar *toolbar_style);

/** According to the preferences this function enables/disables the toolbar */
void ui_mainwindow_update_toolbar();

/** Set the sensitivity of items in the feed menu based on the type of item selected */
void ui_mainwindow_update_feed_menu(gint type);

/** According to the preferences this function enables/disables the menubar */
void ui_mainwindow_update_menubar();
/**
 * Sets the status bar text. Takes printf() like parameters 
 */
void ui_mainwindow_set_status_bar(const char *format, ...);

void ui_mainwindow_update_onlinebtn(void);

/* don't save off-screen positioning */

/**
 * Save the current mainwindow position to gconf, if the window is
 * shown and completely on the screen.
 */
void ui_mainwindow_save_position();

/* GUI callbacks */
void on_onlinebtn_clicked(GtkButton *button, gpointer user_data);
void on_work_offline_activate(GtkMenuItem *menuitem, gpointer user_data);

void
on_work_offline_activate               (GtkMenuItem     *menuitem,
								gpointer         user_data);

void ui_mainwindow_toggle_visibility(GtkMenuItem *menuitem, gpointer data);

typedef void (*fileChoosenCallback) (const gchar *title, gpointer user_data);

/**
 * Open up a file selector
 * @param title window title
 * @param parent window
 * @param buttonName Text to be used as the name of the accept button
 * @param saving TRUE if saving, FALSE if opening
 * @param callback that will be passed the filename (in the system's locale (NOT UTF-8), and some user data
 * @param user data passed to the callback
 */
void ui_choose_file(gchar *title, GtkWindow *parent, gchar *buttonName, gboolean savinng, fileChoosenCallback callback, const gchar *filename, gpointer user_data);
#endif
