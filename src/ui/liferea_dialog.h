/**
 * @file ui_dialog.h UI dialog handling
 *
 * Copyright (C) 2007-2026  Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _LIFEREA_DIALOG_H
#define _LIFEREA_DIALOG_H

#include <libadwaita-1/adwaita.h>

G_BEGIN_DECLS

#define LIFEREA_DIALOG_TYPE (liferea_dialog_get_type ())
G_DECLARE_FINAL_TYPE (LifereaDialog, liferea_dialog, LIFEREA, DIALOG, GObject)

typedef void (*LifereaDialogCallback) (GtkWidget *dialog, gpointer user_data);

/**
 * liferea_dialog_run:
 * @name:		the dialog name
 * @applyCb:		callback when applyBtn was clicked
 * @cancelCb:		callback when cancelBtn was clicked (optional)
 * @userdata:		user data to pass to the callbacks
 *
 * Convenience wrapper to create a new AdwDialog action dialog.
 * Such a dialog needs to have a 'cancelBtn' and a 'applyBtn' to
 * trigger the passed callbacks.
 * 
 * For more complex scenarios and AdwAlertDialogs use liferea_dialog_new()
 */
void liferea_dialog_run (const gchar *name, LifereaDialogCallback applyCb, LifereaDialogCallback cancelCb, gpointer userdata);

/**
 * liferea_dialog_new:
 * @name:		the dialog name
 *
 * Convenience wrapper to create a new dialog and set up its GUI
 * 
 * Returns: the dialog widget
 */
GtkWidget * liferea_dialog_new (const gchar *name);

/**
 * liferea_dialog_lookup:
 * @widget:	the dialog widget
 * @name:	widget name
 *
 * Helper function to look up child widgets of a dialog window.
 * 
 * Returns: found widget (or NULL)
 */
GtkWidget * liferea_dialog_lookup (GtkWidget *widget, const gchar *name);

/**
 * liferea_dialog_entry_get:
 * @widget:	the dialog widget
 * @name:	the GtkEntry name
 * 
 * Returns: the text of the GtkEntry widget with the given name
 */
const gchar * liferea_dialog_entry_get (GtkWidget *widget, const gchar *name);

/**
 * liferea_dialog_entry_set:
 * @widget:	the dialog widget
 * @name:	the GtkEntry name
 * @text:	the text to set
 */
void liferea_dialog_entry_set (GtkWidget *widget, const gchar *name, const gchar *text);

/**
 * liferea_dialog_entryrow_get:
 * @widget:	the dialog widget
 * @name:	the AdwEntryRow name
 * 
 * Returns: the text of the AdwEntryRow widget with the given name
 */
const gchar * liferea_dialog_entryrow_get (GtkWidget *widget, const gchar *name);

/**
 * liferea_dialog_entryrow_set:
 * @widget:	the dialog widget
 * @name:	the AdwEntryRow name
 * @text:	the text to set
 */
void liferea_dialog_entryrow_set (GtkWidget *widget, const gchar *name, const gchar *text);

G_END_DECLS

#endif
