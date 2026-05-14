/**
 * @file ui_common.c  UI helper functions
 *
 * Copyright (C) 2008-2026 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2009 Hubert Figuiere <hub@figuiere.net>
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
 
#include "ui/ui_common.h"

#include <libadwaita-1/adwaita.h>

#include "common.h"
#include "conf.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_shell.h"

void
ui_common_setup_combo_menu (GtkWidget *widget,
                     const gchar **options,
                     GCallback callback,
                     gint defaultValue)
{
	GtkListStore	*listStore;
	GtkTreeIter	treeiter;
	guint		i;
	
	listStore = gtk_list_store_new (2, G_TYPE_STRING, G_TYPE_INT);
	g_assert (NULL != widget);
	for (i = 0; options[i] != NULL; i++) {
		gtk_list_store_append (listStore, &treeiter);
		gtk_list_store_set (listStore, &treeiter, 0, _(options[i]), 1, i, -1);
	}
	gtk_combo_box_set_model (GTK_COMBO_BOX (widget), GTK_TREE_MODEL (listStore));
	if (-1 <= defaultValue)
		gtk_combo_box_set_active (GTK_COMBO_BOX (widget), defaultValue);
	
	if (callback)	
		g_signal_connect (G_OBJECT (widget), "changed", callback, widget);
}

void 
ui_common_setup_combo_text (GtkComboBox *combo, gint col)
{
	GtkCellRenderer *rend = gtk_cell_renderer_text_new ();
	gtk_cell_layout_pack_start (GTK_CELL_LAYOUT (combo), rend, TRUE);
	gtk_cell_layout_add_attribute (GTK_CELL_LAYOUT (combo), rend, "text", col);
}

void
ui_common_treeview_move_cursor (GtkTreeView *treeview, gint step)
{
	gboolean unused;

	gtk_widget_grab_focus (GTK_WIDGET (treeview));
	g_signal_emit_by_name (treeview, "move-cursor", GTK_MOVEMENT_DISPLAY_LINES, step, FALSE /* extend selection */, FALSE /* modify selection */, &unused);
}

void
ui_common_treeview_move_cursor_to_first (GtkTreeView *treeview)
{
	GtkTreePath	*path;

	path = gtk_tree_path_new_first ();
	gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
	gtk_tree_path_free(path);
}

static void
on_confirm_dialog_response (AdwDialog *dialog, gchar *response_id, gpointer user_data)
{
	ConfirmCallback acceptCb = g_object_get_data (G_OBJECT (dialog), "accept");
	ConfirmCallback cancelCb = g_object_get_data (G_OBJECT (dialog), "cancel");

	if (g_strcmp0 (response_id, "accept") == 0)
		acceptCb (user_data);
	else if (cancelCb)
		cancelCb (user_data);

	adw_dialog_close (dialog);
}

void
ui_confirm_box (const gchar *title, const gchar *message, const gchar *acceptButtonText, ConfirmCallback acceptCb, ConfirmCallback cancelCb, gpointer userdata)
{
	AdwDialog *dialog;

	dialog = adw_alert_dialog_new (title, NULL);

	adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog), "%s", message);

	adw_alert_dialog_add_responses (ADW_ALERT_DIALOG (dialog),
					"cancel",  _("_Cancel"),
					"accept", acceptButtonText,
					NULL);

	if (strstr (acceptButtonText, _("Delete"))) {
		adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
		                                          "accept",
		                                          ADW_RESPONSE_DESTRUCTIVE);
	} else {
		adw_alert_dialog_set_response_appearance (ADW_ALERT_DIALOG (dialog),
		                                          "accept",
		                                          ADW_RESPONSE_SUGGESTED);
	}

	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "accept");
	adw_alert_dialog_set_close_response (ADW_ALERT_DIALOG (dialog), "cancel");

	g_object_set_data (G_OBJECT (dialog), "accept", acceptCb);
	g_object_set_data (G_OBJECT (dialog), "cancel", cancelCb);
	g_signal_connect (dialog, "response", G_CALLBACK (on_confirm_dialog_response), userdata);

	adw_dialog_present (ADW_DIALOG (dialog), liferea_shell_get_window ());
}

void
ui_show_error_box (const char *format, ...)
{
	va_list			args;
	g_autofree gchar	*msg = NULL;
	AdwDialog		*dialog;

	g_return_if_fail (format != NULL);

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	dialog = adw_alert_dialog_new (_("Error"), NULL);
	adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog), "%s", msg);
	adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("_Close"));
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "close");

	adw_dialog_present (dialog, liferea_shell_get_window ());
}

void
ui_show_info_box (const char *format, ...)
{
	va_list			args;
	g_autofree gchar	*msg = NULL;
	AdwDialog		*dialog;

	g_return_if_fail (format != NULL);

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	dialog = adw_alert_dialog_new (_("Note"), NULL);
	adw_alert_dialog_format_body (ADW_ALERT_DIALOG (dialog), "%s", msg);
	adw_alert_dialog_add_response (ADW_ALERT_DIALOG (dialog), "close", _("_Close"));
	adw_alert_dialog_set_default_response (ADW_ALERT_DIALOG (dialog), "close");

	adw_dialog_present (dialog, liferea_shell_get_window ());
}

struct file_chooser_tuple {
	fileChoosenCallback func;
	gpointer user_data;
};

static void
ui_choose_file_save_cb (GObject *dialog, GAsyncResult *result, gpointer user_data)
{
	struct file_chooser_tuple *tuple = (struct file_chooser_tuple*)user_data;
	GtkFileDialog *file_dialog = GTK_FILE_DIALOG (dialog);
	g_autoptr(GFile) file = gtk_file_dialog_open_finish (file_dialog, result, NULL);

	if (file) {
		g_autofree gchar *filename = g_file_get_path (file);
		tuple->func (filename, tuple->user_data);
	} else {
		tuple->func (NULL, tuple->user_data);
	}

	g_free (tuple);
}

static void
ui_choose_file_or_dir (gchar *title, const gchar *buttonName, gboolean saving, gboolean directory, fileChoosenCallback callback, const gchar *currentPath, const gchar *defaultFilename, const char *filterstring, const char *filtername, gpointer user_data)
{
	GtkFileDialog 			*dialog;
	struct file_chooser_tuple	*tuple;
	gchar				*path = NULL;

	g_assert (!(saving & directory));
	g_assert (!(defaultFilename && !saving));

	if (!currentPath)
		path = g_strdup (g_get_home_dir ());
	else
		path = g_strdup (currentPath);

	dialog = gtk_file_dialog_new ();
	gtk_file_dialog_set_title (dialog, title);
	gtk_file_dialog_set_modal (dialog, TRUE);
	gtk_file_dialog_set_accept_label (dialog, buttonName);

	tuple = g_new0 (struct file_chooser_tuple, 1);
	tuple->func = callback;
	tuple->user_data = user_data;

	if (path && g_file_test (path, G_FILE_TEST_EXISTS)) {
		g_autoptr(GFile) file = g_file_new_for_path (path);

		if (directory || defaultFilename)
			gtk_file_dialog_set_initial_folder (dialog, file);
		else
			gtk_file_dialog_set_initial_file (dialog, file);
	}
	if (defaultFilename)
		gtk_file_dialog_set_initial_name (dialog, defaultFilename);

	if (filterstring && filtername) {
		GtkFileFilter *filter, *allfiles;
		GListStore *filters;
		gchar **filterstrings, **f;

		filter = gtk_file_filter_new ();

		filterstrings = g_strsplit (filterstring, "|", 0);
		for (f = filterstrings; *f != NULL; f++)
			gtk_file_filter_add_pattern (filter, *f);
		g_strfreev (filterstrings);

		gtk_file_filter_set_name (filter, filtername);

		allfiles = gtk_file_filter_new ();
		gtk_file_filter_add_pattern (allfiles, "*");
		gtk_file_filter_set_name (allfiles, _("All Files"));

		filters = g_list_store_new (GTK_TYPE_FILE_FILTER);
		g_list_store_append (filters, filter);
		g_list_store_append (filters, allfiles);
		gtk_file_dialog_set_filters (dialog, G_LIST_MODEL (filters));
		g_object_unref (filters);
	}

	if (directory) {
		gtk_file_dialog_select_folder (dialog, GTK_WINDOW (liferea_shell_get_window ()),
									   NULL, ui_choose_file_save_cb, tuple);
	} else if (saving) {
		gtk_file_dialog_save (dialog, GTK_WINDOW (liferea_shell_get_window ()),
							  NULL, ui_choose_file_save_cb, tuple);
	} else {
		gtk_file_dialog_open (dialog, GTK_WINDOW (liferea_shell_get_window ()),
							  NULL, ui_choose_file_save_cb, tuple);
	}

	g_free (path);
}

void
ui_choose_file (gchar *title, const gchar *buttonName, gboolean saving, fileChoosenCallback callback, const gchar *currentPath, const gchar *defaultFilename, const char *filterstring, const char *filtername, gpointer user_data)
{
	ui_choose_file_or_dir (title, buttonName, saving, FALSE, callback, currentPath, defaultFilename, filterstring, filtername, user_data);
}

void
ui_common_action_group_enable (GActionGroup *group, gboolean enabled)
{
	gchar **actions_list = g_action_group_list_actions (group);
	gint i;
	for (i=0;actions_list[i] != NULL;i++) {
		g_action_group_action_enabled_changed (group, actions_list[i], enabled);
	}
	g_strfreev (actions_list);
}