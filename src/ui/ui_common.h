/**
 * @file ui_common.h  UI helper functions
 *
 * Copyright (C) 2008-2011 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _UI_COMMON_H
#define _UI_COMMON_H

#include <gtk/gtk.h>

/**
 * Helper function to set up a combo box option menu.
 * To be used to initialize dialogs.
 *
 * @param widget	the option menu widget
 * @param options	list of option literals
 * @param callback	"changed" callback for the widget (or NULL)
 * @param defaultValue	the default menu value
 */
void ui_common_setup_combo_menu (GtkWidget *widget, const gchar **options, GCallback callback, gint defaultValue);

/**
 * Helper function to set up a combo to display the text from
 * a column of the model.
 *
 * @param combo the combo widget
 * @param col the column to use in the model
 */
void ui_common_setup_combo_text (GtkComboBox *combo, gint col);

/**
 * Move cursor to nth next iter in a tree view.
 *
 * @param treeview	the tree view
 * @param step		how many iters to skip (negative values move backwards)
 */
void ui_common_treeview_move_cursor (GtkTreeView *treeview, gint step);

/**
 * Move cursor to 1st iter in a tree view.
 *
 * @param treeview	the tree view
 */
void ui_common_treeview_move_cursor_to_first (GtkTreeView *treeview);

/**
 * Presents an "Info" type box with the given message.
 *
 * @param printf like format
 */
void ui_show_info_box (const char *format, ...);

/**
 * Presents an "Error" type box with the given message.
 * Does not do error tracing or console output.
 *
 * @param printf like format
 */
void ui_show_error_box (const char *format, ...);


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
 * @param filterstring 	a pattern for a GtkFileFilter or NULL
 * @param filtername	a human readable name for the pattern or NULL (if pattern is NULL)
 * @param user_data	user data passed to the callback
 */
void ui_choose_file (gchar *title, const gchar *buttonName, gboolean saving, fileChoosenCallback callback, const gchar *currentPath, const gchar *defaultFilename, const char *filterstring, const char *filtername, gpointer user_data);

#endif
