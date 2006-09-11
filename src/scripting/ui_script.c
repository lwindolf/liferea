/**
 * @file ui_script.c UI dialogs concerning script configuration
 *
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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

#include <string.h>
#include "ui_script.h"
#include "common.h"
#include "interface.h"
#include "script.h"
#include "support.h"

typedef struct hookInfo {
	gchar		*name;
	gint		type;
} *hookInfoPtr;

static struct hookInfo availableHooks[] = {
	{ "startup",		SCRIPT_HOOK_STARTUP },
	/* update events */
	{ "feed updated",	SCRIPT_HOOK_FEED_UPDATED },
	/* selection hooks */
	{ "item selected",	SCRIPT_HOOK_ITEM_SELECTED },
	{ "feed selected",	SCRIPT_HOOK_FEED_SELECTED },
	{ "item unselect",	SCRIPT_HOOK_ITEM_UNSELECT },
	{ "feed unselect",	SCRIPT_HOOK_FEED_UNSELECT },
	{ NULL, 0 }
};

static GtkWidget *scriptdialog = NULL;
static gint selectedHookType = -1;
static gchar *selectedScript = NULL;

static void ui_script_reload_script_list(void) {
	GtkTreeIter	treeiter;
	GtkTreeStore	*treestore;
	GSList		*list;

	treestore = GTK_TREE_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(lookup_widget(scriptdialog, "scriptlist"))));
	gtk_tree_store_clear(treestore);
	
	list = script_hook_get_list(selectedHookType);
	while(list) {
		gtk_tree_store_append(treestore, &treeiter, NULL);
		gtk_tree_store_set(treestore, &treeiter, 
		                   0, list->data, 
				   1, list->data,
				   -1);
		list = g_slist_next(list);
	}
	selectedScript = NULL;
	gtk_widget_set_sensitive(lookup_widget(scriptdialog, "scriptSaveBtn"), FALSE);
	gtk_widget_set_sensitive(lookup_widget(scriptdialog, "scriptRunBtn"), FALSE);
}

static void ui_script_load_code(void) {
	GtkWidget	*edit = lookup_widget(scriptdialog, "scriptedit");
	gchar		*filename, *buffer = NULL;
	gint		len = 0;
	
	if(selectedScript) {
		filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "scripts", selectedScript, "lua");
		g_file_get_contents(filename, &buffer, &len, NULL);
		g_free(filename);
		gtk_widget_set_sensitive(edit, TRUE);
		gtk_widget_set_sensitive(lookup_widget(scriptdialog, "scriptSaveBtn"), TRUE);
		gtk_widget_set_sensitive(lookup_widget(scriptdialog, "scriptRunBtn"), TRUE);
	} else {
		gtk_widget_set_sensitive(edit, FALSE);
	}

	if(!buffer)
		buffer = g_strdup("");
	
	gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(edit)), buffer, len);
	g_free(buffer);
}

static void ui_script_add(gchar *name) {

	script_hook_add(selectedHookType, name);
	ui_script_reload_script_list();
	ui_script_load_code();
}

static void ui_script_script_selection_changed(GtkTreeSelection *selection, gpointer data) {
	GtkTreeIter	iter;
	GtkTreeModel	*model;

	if(gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gtk_tree_model_get(model, &iter, 1, &selectedScript, -1);
		ui_script_load_code();
	}
}

static void on_hook_selection_changed(GtkOptionMenu *optionmenu, gpointer user_data) {

	selectedHookType = GPOINTER_TO_INT(user_data);
	ui_script_reload_script_list();
	ui_script_load_code();
}

static void on_script_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {
	
	gtk_widget_destroy(scriptdialog);
	scriptdialog = NULL;
}

void on_menu_show_script_manager(GtkWidget *widget, gpointer user_data) {
	GtkWidget		*menu, *entry;
	GtkTreeStore		*treestore;
	GtkTreeView		*treeview;
	GtkTreeViewColumn	*column;
	GtkCellRenderer		*renderer;
	hookInfoPtr		hook;

	if(!scriptdialog) {
		scriptdialog = create_scriptdialog();
		gtk_widget_show(scriptdialog);
		
		/* Set up hook selection popup */
		menu = gtk_menu_new();
		hook = availableHooks;
		while(hook->name) {
			entry = gtk_menu_item_new_with_label(hook->name);
			gtk_widget_show(entry);
			gtk_container_add(GTK_CONTAINER(menu), entry);
			gtk_signal_connect(GTK_OBJECT(entry), "activate", GTK_SIGNAL_FUNC(on_hook_selection_changed), GINT_TO_POINTER(hook->type));
			hook++;
		}
		gtk_option_menu_set_menu(GTK_OPTION_MENU(lookup_widget(scriptdialog, "scripthooksmenu")), menu);
		
		/* Set up script list tree store */
		treestore = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_POINTER);
		treeview = GTK_TREE_VIEW(lookup_widget(scriptdialog, "scriptlist"));

		column = gtk_tree_view_column_new();
		renderer = gtk_cell_renderer_text_new();
		gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(treeview), -1, "Script Name", renderer, "text", 0, NULL);
		gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(treestore));
		g_object_unref(treestore);

		gtk_tree_selection_set_mode(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview)),
	                        	    GTK_SELECTION_SINGLE);
		g_signal_connect(G_OBJECT(gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview))), 
		                 "changed", G_CALLBACK(ui_script_script_selection_changed), NULL);
		
		/* Per default the first hook is selected so preload its script list */
		selectedHookType = availableHooks[0].type;
		ui_script_reload_script_list();
		ui_script_load_code();
		
		g_signal_connect(G_OBJECT(scriptdialog), "response", G_CALLBACK(on_script_dialog_response), NULL);
	}
}

/* UI callbacks */

void on_scriptCmdExecBtn_clicked(GtkButton *button, gpointer user_data) {

	script_run_cmd(gtk_entry_get_text(GTK_ENTRY(lookup_widget(scriptdialog, "cmdEntry"))));
}

void on_cmdEntry_activate(GtkEntry *entry, gpointer user_data) {

	script_run_cmd(gtk_entry_get_text(GTK_ENTRY(lookup_widget(scriptdialog, "cmdEntry"))));
}

static void on_script_add_dialog_response(GtkDialog *dialog, gint response_id, gpointer user_data) {

	if(GTK_RESPONSE_OK == response_id) {	
		if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(lookup_widget(GTK_WIDGET(dialog), "scriptAddNewRadioBtn")))) {
			/* add new */
			ui_script_add(g_strdup(gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "scriptAddEntry")))));
		} else {
			/* reuse existing */
		}
	}
	gtk_widget_destroy(GTK_WIDGET(dialog));
}

void on_scriptAddBtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*dialog;
	GtkWidget	*menu, *entry;
	GDir		*dir;
	gchar		*dirname, *file, *tmp;
	
	/* setup script add dialog */
	dialog = create_scriptadddialog();
	
	/* add existing script files to reuse popup */
	menu = gtk_menu_new();
	
	dirname = g_strdup_printf("%s" G_DIR_SEPARATOR_S "cache" G_DIR_SEPARATOR_S "scripts", common_get_cache_path());
	g_print("%s\n",dirname);
	dir = g_dir_open(dirname, 0, NULL);
	while(NULL != (file = (gchar *)g_dir_read_name(dir))) {
		tmp = strstr(file, ".lua");
		if(tmp) {
			*tmp = 0;
			entry = gtk_menu_item_new_with_label(file);
			gtk_widget_show(entry);
			gtk_container_add(GTK_CONTAINER(menu), entry);		
		}
	}
	g_dir_close(dir);
	g_free(dirname);
	
	gtk_option_menu_set_menu(GTK_OPTION_MENU(lookup_widget(dialog, "scriptAddMenu")), menu);
	
	g_signal_connect(G_OBJECT(dialog), "response", G_CALLBACK(on_script_add_dialog_response), NULL);
	gtk_widget_show(dialog);
}

void on_scriptRemoveBtn_clicked(GtkButton *button, gpointer user_data) {

	script_hook_remove(selectedHookType, selectedScript);
	ui_script_reload_script_list();
	ui_script_load_code();
	selectedScript = NULL;
}

void on_scriptSaveBtn_clicked(GtkButton *button, gpointer user_data) {
	GtkTextIter	start, end;
	GtkTextBuffer	*textbuf;
	gchar		*content, *filename;
	
	textbuf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(lookup_widget(scriptdialog, "scriptedit")));
	gtk_text_buffer_get_start_iter(textbuf, &start);
	gtk_text_buffer_get_end_iter(textbuf, &end);
	content = gtk_text_buffer_get_text(textbuf, &start, &end, TRUE);
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "scripts", selectedScript, "lua");
	if(!g_file_set_contents(filename, content, strlen(content), NULL))
		g_warning("Saving script %s failed!", filename);
		
	g_free(filename);
	g_free(content);
}

void on_scriptRunBtn_clicked(GtkButton *button, gpointer user_data) {

	if(selectedScript)
		script_run(selectedScript);
}
