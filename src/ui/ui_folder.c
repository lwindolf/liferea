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
		
	gtk_widget_show(newfolderdialog);
}

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data) {
	nodePtr		folder;
	
	/* create folder node */
	folder = node_new();
	node_set_title(folder, (gchar *)gtk_entry_get_text(GTK_ENTRY(lookup_widget(newfolderdialog, "foldertitleentry"))));
	node_add_data(folder, NODE_TYPE_FOLDER, NULL);

	/* add the new folder to the model */
	node_add_child(NULL, folder, 0);
	ui_feedlist_select(folder);
}

void ui_folder_properties(nodePtr folder) {
	
	g_assert(!folder || (NODE_TYPE_FOLDER != folder->type));
	
	if(!foldernamedialog || !G_IS_OBJECT(foldernamedialog))
		foldernamedialog = create_foldernamedialog();
	
	gtk_entry_set_text(GTK_ENTRY(lookup_widget(foldernamedialog, "foldernameentry")), node_get_title(folder));
	gtk_object_set_data(GTK_OBJECT(foldernamedialog), "folder", folder);

	gtk_widget_show(foldernamedialog);
}

void on_foldernamechangebtn_clicked(GtkButton *button, gpointer user_data) {
	nodePtr		folder;
	
	folder = (nodePtr)gtk_object_get_data(GTK_OBJECT(foldernamedialog), "folder");
	node_set_title(folder, (gchar *)gtk_entry_get_text(GTK_ENTRY(lookup_widget(foldernamedialog, "foldernameentry"))));
	ui_node_update(folder);
	gtk_widget_hide(foldernamedialog);
}

