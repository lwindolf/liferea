/**
 * @file preferences_dialog.h Liferea preferences
 *
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2012 Lars Windolf <lars.windolf@gmx.de>
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
#define PREFERENCES_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), PREFERENCES_DIALOG_TYPE, PreferencesDialog))
#define PREFERENCES_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), PREFERENCES_DIALOG_TYPE, PreferencesDialogClass))
#define IS_PREFERENCES_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), PREFERENCES_DIALOG_TYPE))
#define IS_PREFERENCES_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), PREFERENCES_DIALOG_TYPE))

typedef struct PreferencesDialog	PreferencesDialog;
typedef struct PreferencesDialogClass	PreferencesDialogClass;
typedef struct PreferencesDialogPrivate	PreferencesDialogPrivate;

extern PreferencesDialog *preferences_dialog;

struct PreferencesDialog
{
	GObject		parent;
	
	/*< private >*/
	PreferencesDialogPrivate	*priv;
};

struct PreferencesDialogClass 
{
	GObjectClass parent_class;
};

GType preferences_dialog_get_type	(void);

/**
 * Returns a download tool definition.
 *
 * @return the download command definition
 */
const gchar * prefs_get_download_command (void);

/**
 * Show the preferences dialog.
 */
void preferences_dialog_open (void);

/* functions used in glade/prefs.ui */
void on_folderdisplaybtn_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_folderhidereadbtn_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_trayiconoptionbtn_clicked (GtkButton *button, gpointer user_data);
void on_popupwindowsoptionbtn_clicked (GtkButton *button, gpointer user_data);
void on_startupactionbtn_toggled (GtkButton *button, gpointer user_data);
void on_browsercmd_changed (GtkEditable *editable, gpointer user_data);
void on_openlinksinsidebtn_clicked (GtkToggleButton *button, gpointer user_data);
void on_disablejavascript_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_enableplugins_toggled (GtkToggleButton *togglebutton, gpointer user_data);
void on_itemCountBtn_value_changed (GtkSpinButton *spinbutton, gpointer user_data);
void on_default_update_interval_value_changed (GtkSpinButton *spinbutton, gpointer user_data);
void on_useProxyAuth_toggled (GtkToggleButton *button, gpointer user_data);
void on_enc_action_change_btn_clicked (GtkButton *button, gpointer user_data);
void on_enc_action_remove_btn_clicked (GtkButton *button, gpointer user_data);
void on_newcountintraybtn_clicked (GtkButton *button, gpointer user_data);
void on_minimizetotraybtn_clicked (GtkButton *button, gpointer user_data);
void on_startintraybtn_clicked (GtkButton *button, gpointer user_data);
void on_hidetoolbar_toggled (GtkToggleButton *button, gpointer user_data);

G_END_DECLS

#endif
