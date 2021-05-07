/**
 * @file ui_common.c  UI helper functions
 *
 * Copyright (C) 2008-2014 Lars Windolf <lars.windolf@gmx.de>
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

#include "common.h"
#include "conf.h"
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
	gboolean	ret;

	gtk_widget_grab_focus (GTK_WIDGET (treeview));
	g_signal_emit_by_name (treeview, "move-cursor", GTK_MOVEMENT_DISPLAY_LINES, step, &ret);
}

void
ui_common_treeview_move_cursor_to_first (GtkTreeView *treeview)
{
	GtkTreePath	*path;

	path = gtk_tree_path_new_first ();
	gtk_tree_view_set_cursor (treeview, path, NULL, FALSE);
	gtk_tree_path_free(path);
}

void
ui_show_error_box (const char *format, ...)
{
	GtkWidget	*dialog;
	va_list		args;
	gchar		*msg;

	g_return_if_fail (format != NULL);

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	dialog = gtk_message_dialog_new (GTK_WINDOW (liferea_shell_get_window ()),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_ERROR,
                  GTK_BUTTONS_CLOSE,
                  "%s", msg);
	(void)gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_free (msg);
}

void
ui_show_info_box (const char *format, ...)
{
	GtkWidget	*dialog;
	va_list		args;
	gchar		*msg;

	g_return_if_fail (format != NULL);

	va_start (args, format);
	msg = g_strdup_vprintf (format, args);
	va_end (args);

	dialog = gtk_message_dialog_new (GTK_WINDOW (liferea_shell_get_window ()),
                  GTK_DIALOG_DESTROY_WITH_PARENT,
                  GTK_MESSAGE_INFO,
                  GTK_BUTTONS_CLOSE,
                  "%s", msg);
	(void)gtk_dialog_run (GTK_DIALOG (dialog));
	gtk_widget_destroy (dialog);
	g_free (msg);
}

struct file_chooser_tuple {
	fileChoosenCallback func;
	gpointer user_data;
};

static void
ui_choose_file_save_cb (GtkNativeDialog *dialog, gint response_id, gpointer user_data)
{
	struct file_chooser_tuple *tuple = (struct file_chooser_tuple*)user_data;
	gchar *filename;

	if (response_id == GTK_RESPONSE_ACCEPT) {
		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (dialog));
		tuple->func (filename, tuple->user_data);
		g_free (filename);
	} else {
		tuple->func (NULL, tuple->user_data);
	}

	gtk_native_dialog_destroy (dialog);
	g_free (tuple);
}

static void
ui_choose_file_or_dir(gchar *title, const gchar *buttonName, gboolean saving, gboolean directory, fileChoosenCallback callback, const gchar *currentPath, const gchar *defaultFilename, const char *filterstring, const char *filtername, gpointer user_data)
{
	GtkFileChooserNative		*native;
	GtkFileChooser 			*chooser;
	struct file_chooser_tuple	*tuple;
	gchar				*path = NULL;

	g_assert (!(saving & directory));
	g_assert (!(defaultFilename && !saving));

	if (!currentPath)
		path = g_strdup (g_get_home_dir ());
	else
		path = g_strdup (currentPath);

	native = gtk_file_chooser_native_new (title, GTK_WINDOW (liferea_shell_get_window ()),
	                                      (directory?GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER:
					       (saving ? GTK_FILE_CHOOSER_ACTION_SAVE : GTK_FILE_CHOOSER_ACTION_OPEN)),
	                                      buttonName, NULL);
	chooser = GTK_FILE_CHOOSER (native);

	if (saving)
		gtk_file_chooser_set_do_overwrite_confirmation (chooser, TRUE);
	gtk_native_dialog_set_modal (GTK_NATIVE_DIALOG (native), TRUE);
	gtk_native_dialog_set_transient_for (GTK_NATIVE_DIALOG (native), GTK_WINDOW (liferea_shell_get_window ()));
	
	tuple = g_new0 (struct file_chooser_tuple, 1);
	tuple->func = callback;
	tuple->user_data = user_data;

	g_signal_connect (G_OBJECT (native), "response",
	                  G_CALLBACK (ui_choose_file_save_cb), tuple);
	if (path && g_file_test (path, G_FILE_TEST_EXISTS)) {
		if (directory || defaultFilename)
			gtk_file_chooser_set_current_folder (chooser, path);
		else
			gtk_file_chooser_set_filename (chooser, path);
	}
	if (defaultFilename)
		gtk_file_chooser_set_current_name (chooser, defaultFilename);

	if (filterstring && filtername) {
		GtkFileFilter *filter, *allfiles;
		gchar **filterstrings, **f;

		filter = gtk_file_filter_new ();

		filterstrings = g_strsplit (filterstring, "|", 0);
		for (f = filterstrings; *f != NULL; f++)
			gtk_file_filter_add_pattern (filter, *f);
		g_strfreev (filterstrings);

		gtk_file_filter_set_name (filter, filtername);
		gtk_file_chooser_add_filter (chooser, filter);

		allfiles = gtk_file_filter_new ();
		gtk_file_filter_add_pattern (allfiles, "*");
		gtk_file_filter_set_name (allfiles, _("All Files"));
		gtk_file_chooser_add_filter (chooser, allfiles);
	}

	gtk_native_dialog_show (GTK_NATIVE_DIALOG (native));
	g_free (path);
}

void
ui_choose_file (gchar *title, const gchar *buttonName, gboolean saving, fileChoosenCallback callback, const gchar *currentPath, const gchar *defaultFilename, const char *filterstring, const char *filtername, gpointer user_data)
{
	ui_choose_file_or_dir (title, buttonName, saving, FALSE, callback, currentPath, defaultFilename, filterstring, filtername, user_data);
}

void
ui_common_simple_action_group_set_enabled (GActionGroup *group, gboolean enabled)
{
	gchar **actions_list = g_action_group_list_actions (group);
	gint i;
	for (i=0;actions_list[i] != NULL;i++) {
		g_simple_action_set_enabled (G_SIMPLE_ACTION (g_action_map_lookup_action (G_ACTION_MAP (group), actions_list [i])), enabled);
	}
	g_strfreev (actions_list);
}

void
ui_common_add_action_group_to_map (GActionGroup *group, GActionMap *map)
{
	gchar **actions_list = g_action_group_list_actions (group);
	gint i;
	for (i=0;actions_list[i] != NULL;i++) {
		g_action_map_add_action (map, g_action_map_lookup_action (G_ACTION_MAP (group), actions_list [i]));
	}
	g_strfreev (actions_list);

}
