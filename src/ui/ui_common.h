/**
 * @file ui_common.h  UI helper functions
 *
 * Copyright (C) 2008-2025 Lars Windolf <lars.windolf@gmx.de>
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
 * Enable or disable an action in the action map.
 *
 * @param group 	the action group
 * @param name		the action name
 * @param enabled	TRUE to enable, FALSE to disable
 */
void ui_common_action_enable (GActionGroup *group, const gchar *name, gboolean enabled);

/**
 * Helper function to enable/disable all actions in a group.
 *
 * @param group		the action group
 * @param enabled	TRUE to enable, FALSE to disable
 */
void ui_common_action_group_enable (GActionGroup *group, gboolean enabled);

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

typedef void (*ConfirmCallback) (gpointer user_data);

/**
 * ui_confirm_box:
 * Presents a "Cancel" / "OK" dialog with the given message.
 *
 * @title:              title of the dialog
 * @message:            message to display
 * @acceptButtonText:   text for the accept button
 * @acceptCb:           callback
 * @cancelCb:           callback
 * @userdata:           user data to pass to the callback
 */
void ui_confirm_box (const gchar *title, const gchar *message, const gchar *acceptButtonText, ConfirmCallback acceptCb, ConfirmCallback cancelCb, gpointer userdata);

/**
 * ui_show_info_box:
 * Presents an "Info" type box with the given message.
 *
 * @format: vaargs
 */
void ui_show_info_box (const char *format, ...);

/**
 * ui_show_error_box:
 * Presents an "Error" type box with the given message.
 * Does not do error tracing or console output.
 *
 * @format: vaargs
 */
void ui_show_error_box (const char *format, ...);


/** Callback to be used with the filename choosing dialog */
typedef void (*fileChoosenCallback) (const gchar *title, gpointer user_data);

/**
 * ui_choose_file:
 * Open up a file selector
 *
 * @title:		window title
 * @buttonName:	        Text to be used as the name of the accept button
 * @saving:	        TRUE if saving, FALSE if opening
 * @callback:           that will be passed the filename (in the system's locale (NOT UTF-8), and some user data
 * @currentPath:        This file or directory will be selected in the chooser
 * @filename:   	When saving, this is the suggested filename
 * @filterstring: 	a pattern for a GtkFileFilter or NULL
 * @filtername:	        a human readable name for the pattern or NULL (if pattern is NULL)
 * @user_data:	        user data passed to the callback
 */
void ui_choose_file (gchar *title, const gchar *buttonName, gboolean saving, fileChoosenCallback callback, const gchar *currentPath, const gchar *defaultFilename, const char *filterstring, const char *filtername, gpointer user_data);

#endif
