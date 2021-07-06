/**
 * @file preferences_dialog.h Liferea preferences
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2018 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _PREFERENCES_DIALOG_H
#define _PREFERENCES_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define PREFERENCES_DIALOG_TYPE		(preferences_dialog_get_type ())
G_DECLARE_FINAL_TYPE (PreferencesDialog, preferences_dialog, PREFERENCES, DIALOG, GObject)

/**
 * prefs_get_download_command:
 *
 * Returns: (transfer full): The download command.
 */
gchar * prefs_get_download_command (void);

/**
 * preferences_dialog_open:
 * Show the preferences dialog.
 */
void preferences_dialog_open (void);

/* functions used in glade/prefs.ui */
void on_folderdisplaybtn_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_folderhidereadbtn_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_deferdeletemode_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_popupwindowsoptionbtn_clicked (GtkButton *button, gpointer user_data);
void on_startupactionbtn_toggled (GtkButton *button, gpointer user_data);
void on_browsercmd_changed (GtkEditable *editable, gpointer user_data);
void on_openlinksinsidebtn_clicked (GtkToggleButton *button, gpointer user_data);
void on_disablejavascript_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_enableplugins_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_itemCountBtn_value_changed (GtkSpinButton *spinbutton, gpointer user_data);
void on_default_update_interval_value_changed (GtkSpinButton *spinbutton, gpointer user_data);
void on_useProxyAuth_toggled (GtkToggleButton *button, gpointer user_data);
void on_enclosure_download_custom_command_changed (GtkEditable *entry, gpointer user_data);
void on_enclosure_download_predefined_toggled (GtkToggleButton *button, gpointer user_data);
void on_enc_action_change_btn_clicked (GtkButton *button, gpointer user_data);
void on_enc_action_remove_btn_clicked (GtkButton *button, gpointer user_data);
void on_hidetoolbar_toggled (GtkToggleButton *button, gpointer user_data);

G_END_DECLS

#endif
