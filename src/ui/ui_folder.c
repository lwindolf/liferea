/**
 * @file ui_folder.c GUI folder handling
 * 
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2006 Lars Lindner <lars.lindner@gmx.net>
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
#include "support.h"
#include "interface.h"
#include "callbacks.h"
#include "debug.h"
#include "ui/ui_folder.h"

static GtkWidget	*newfolderdialog = NULL;
static GtkWidget	*foldernamedialog = NULL;

void ui_folder_add(nodePtr parent) {
	GtkWidget	*foldernameentry;
	
	if(!newfolderdialog || !G_IS_OBJECT(newfolderdialog))
		newfolderdialog = create_newfolderdialog();

	foldernameentry = lookup_widget(newfolderdialog, "foldertitleentry");
	gtk_entry_set_text(GTK_ENTRY(foldernameentry), "");
	gtk_object_set_data(GTK_OBJECT(newfolderdialog), "parent", parent);
		
	gtk_widget_show(newfolderdialog);
}

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data) {
	GtkWidget	*foldertitleentry;
	gchar		*foldertitle;
	nodePtr		folder, parentNode;
	int		pos;
	
	g_assert(newfolderdialog != NULL);
	
	foldertitleentry = lookup_widget(newfolderdialog, "foldertitleentry");
	foldertitle = (gchar *)gtk_entry_get_text(GTK_ENTRY(foldertitleentry));

	parentNode = (nodePtr)gtk_object_get_data(GTK_OBJECT(newfolderdialog), "parent");

	/* create folder node */
	folder = node_new();
	folder->handler = parentNode->handler;
	node_set_title(folder, foldertitle);
	node_add_data(folder, FST_FOLDER, NULL);

	/* add the new folder to the model */
	parentNode = ui_feedlist_get_target_folder(&pos);
	node_add_child(parentNode, folder, pos);
	ui_feedlist_select(folder);
}

void ui_folder_properties(nodePtr folder) {
	GtkWidget	*foldernameentry;
	
	if(!folder || (FST_FOLDER != folder->type)) {
		ui_show_error_box(_("A folder must be selected."));
		return;
	}
	
	if(!foldernamedialog || !G_IS_OBJECT(foldernamedialog))
		foldernamedialog = create_foldernamedialog();
	
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	gtk_entry_set_text(GTK_ENTRY(foldernameentry), node_get_title(folder));
	gtk_object_set_data(GTK_OBJECT(foldernamedialog), "folder", folder);

	gtk_widget_show(foldernamedialog);
}

void on_foldernamechangebtn_clicked(GtkButton *button, gpointer user_data) {
	nodePtr		folder;
	GtkWidget	*foldernameentry;
	
	folder = (nodePtr)gtk_object_get_data(GTK_OBJECT(foldernamedialog), "folder");
	foldernameentry = lookup_widget(foldernamedialog, "foldernameentry");
	node_set_title(folder, (gchar *)gtk_entry_get_text(GTK_ENTRY(foldernameentry)));
	ui_node_update(folder);
	gtk_widget_hide(foldernamedialog);
}

