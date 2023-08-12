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
 * liferea_shell_save_position
 *
 * Save the position of the Liferea main window.
 */
void
liferea_shell_save_position (void);

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
 * liferea_shell_set_toolbar_style:
 * @toolbar_style: text string containing the type of style to use
 *
 * Sets the toolbar to a particular style
 */
void liferea_shell_set_toolbar_style (const gchar *toolbar_style);

/**
 * liferea_shell_update_toolbar: (skip)
 *
 * According to the preferences this function enables/disables the toolbar.
 *
 * TODO: use signal instead
 */
void liferea_shell_update_toolbar (void);

/**
 * liferea_shell_update_history_actions: (skip)
 *
 * Update item history menu actions and toolbar buttons.
 *
 * TODO: use signal instead
 */
void liferea_shell_update_history_actions (void);

/**
 * liferea_shell_update_item_menu: (skip)
 * @enabled:	TRUE if item actions are to be enabled
 *
 * Update the sensitivity of options affecting single items.
 *
 * TODO: use signal instead
 */
void liferea_shell_update_item_menu (gboolean enabled);

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
 * liferea_shell_get_window:
 *
 * Returns the Liferea main window.
 *
 * Returns: (transfer none): the main window widget found or NULL
 */
GtkWidget * liferea_shell_get_window (void);

/**
 * liferea_shell_rebuild_css:
 *
 * Invokes a rebuild of the WebView CSS.
 */
void liferea_shell_rebuild_css (void);

G_END_DECLS

#endif
