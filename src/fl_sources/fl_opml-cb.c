/**
 * @file fl_opml.h OPML Planet/Blogroll feed list provider
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>

#include "common.h"
#include "debug.h"
#include "node.h"
#include "support.h"
#include "update.h"
#include "feedlist.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_mainwindow.h"
#include "fl_sources/fl_plugin.h"
#include "fl_sources/fl_opml.h"
#include "fl_sources/fl_opml-cb.h"
#include "fl_sources/fl_opml-ui.h"

extern GtkWidget *mainwindow;

extern flPluginPtr fl_plugin_get_info();

static void on_fl_opml_source_selected(GtkDialog *dialog, gint response_id, gpointer user_data) {
	nodePtr		node, parent = (nodePtr)user_data;

	if(response_id != GTK_RESPONSE_OK)
		return;

	node = node_new();
	node_set_title(node, _("New OPML Subscription"));
	fl_plugin_new_source(node, fl_plugin_get_info(), gtk_entry_get_text(GTK_ENTRY(lookup_widget(GTK_WIDGET(dialog), "location_entry"))));
	fl_opml_source_setup(parent, node);
	fl_opml_source_update(node);

	gtk_widget_destroy(GTK_WIDGET(dialog));
}

void ui_fl_opml_get_source_url(nodePtr parent) {
	GtkWidget	*dialog;

	dialog = create_instance_dialog();

	g_signal_connect(G_OBJECT(dialog), "response",
			 G_CALLBACK(on_fl_opml_source_selected), 
			 (gpointer)parent);

	gtk_widget_show_all(dialog);
}

static void on_file_select_clicked(const gchar *filename, gpointer user_data) {
	GtkWidget	*dialog = GTK_WIDGET(user_data);

	gtk_entry_set_text(GTK_ENTRY(lookup_widget(dialog, "location_entry")), g_strdup(filename));
}

void on_select_button_clicked(GtkButton *button, gpointer user_data) {

	ui_choose_file(_("Choose OPML File"), GTK_WINDOW(mainwindow), GTK_STOCK_OPEN, FALSE, on_file_select_clicked, NULL, NULL, user_data);
}

