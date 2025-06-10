/*
 * @file liferea_shell.h  UI layout handling
 *
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2007-2022 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _LIFEREA_SHELL_H
#define _LIFEREA_SHELL_H

#include <string.h>
#include <glib-object.h>
#include <glib.h>
#include <gtk/gtk.h>

#include "node.h"

/* possible main window states */
enum mainwindowState {
	MAINWINDOW_SHOWN,		/*<< main window is visible */
	MAINWINDOW_MAXIMIZED,	/*<< main window is visible and maximized */
	MAINWINDOW_ICONIFIED,	/*<< main window is iconified */
	MAINWINDOW_HIDDEN		/*<< main window is not visible at all */
};

G_BEGIN_DECLS

#define LIFEREA_SHELL_TYPE		(liferea_shell_get_type ())
G_DECLARE_FINAL_TYPE (LifereaShell, liferea_shell, LIFEREA, SHELL, GObject)

/**
 * liferea_shell_get_instance:
 *
 * Get the LifereaShell instance
 *
 * Returns: the LifereaShell instance
 */
LifereaShell * liferea_shell_get_instance (void);

/**
 * liferea_shell_lookup_action:
 * @name: the action name
 *
 * Searches the links action map for the given action
 * name and returns the found action.
 *
 * Returns: (transfer none) (nullable): the action found or NULL
 */
GAction * liferea_shell_lookup_link_action (const gchar *name);

/**
 * liferea_shell_lookup:
 * @name: the widget name
 *
 * Searches the glade XML UI tree for the given widget
 * name and returns the found widget.
 *
 * Returns: (transfer none) (nullable): the widget found or NULL
 */
GtkWidget * liferea_shell_lookup (const gchar *name);

/**
 * liferea_shell_create: (skip)
 * @app:	                the GtkApplication to attach the main window to
 * @overrideWindowState:	optional parameter for window state (or NULL)
 * @pluginsDisabled		1 if plugins are not to be loaded
 *
 * Set up the Liferea main window.
 */
void liferea_shell_create (GtkApplication *app, const gchar *overrideWindowState, gint pluginsDisabled);

/**
 * liferea_shell_destroy: (skip)
 *
 * Destroys the global liferea_shell object.
 */
void liferea_shell_destroy (void);

/**
 * liferea_shell_add_actions: (skip)
 * @entries:	array of actions to add
 * @count:	the number of actions
 *
 * Adds the given actions to the given action map.
 * 
 * Returns: newly create GActionGroup (owned by LifereaShell)
 */
GActionGroup * liferea_shell_add_actions (const GActionEntry *entries, int count);

/**
 * liferea_shell_show_window:
 *
 * Show the main window.
 */
void liferea_shell_show_window (void);

/**
 * liferea_shell_toggle_visibility:
 *
 * Toggles main window visibility.
 */
void liferea_shell_toggle_visibility (void);

/**
 * liferea_shell_set_status_bar:
 *
 * Sets the status bar text. Takes printf() like parameters.
 */
void liferea_shell_set_status_bar (const char *format, ...);

/**
 * liferea_shell_set_important_status_bar:
 *
 * Similar to liferea_shell_set_status_message(), but ensures
 * that messages stay visible and avoids that those messages
 * are overwritten by unimportant ones.
 */
void liferea_shell_set_important_status_bar (const char *format, ...);

/**
 * liferea_shell_copy_to_clipboard:
 * @str: the string to copy to the clipboard
 *
 * Copy a text to clipboard
 */
void liferea_shell_copy_to_clipboard (const gchar *str);

/**
 * liferea_shell_get_window:
 *
 * Returns the Liferea main window.
 *
 * Returns: (transfer none): the main window widget found or NULL
 */
GtkWidget * liferea_shell_get_window (void);

/**
 * liferea_shell_find_next_unread:
 * @startId:	the item id to start at (or NULL for starting at the top)
 *
 * Finds the next unread item.
 *
 * Returns: (transfer none): the item found (or NULL)
 */
itemPtr liferea_shell_find_next_unread (gulong startId);

/**
 * liferea_shell_set_layout:
 * @newMode:	new view mode (NODE_VIEW_MODE_*)
 *
 * Switches the layout for the given viewing mode.
 */
void liferea_shell_set_layout (nodeViewType newMode);

G_END_DECLS

#endif
