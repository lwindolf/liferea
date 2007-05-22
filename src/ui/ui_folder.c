/**
 * @file ui_folder.c GUI folder handling
 * 
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
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
#include "debug.h"
#include "feedlist.h"
#include "folder.h"
#include "ui/ui_dialog.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_folder.h"

static GtkWidget	*newfolderdialog = NULL;

void ui_folder_add(nodePtr parent) {
	GtkWidget	*foldernameentry;
	
	if(!newfolderdialog || !G_IS_OBJECT(newfolderdialog))
		newfolderdialog = liferea_dialog_new (NULL, "newfolderdialog");

	foldernameentry = liferea_dialog_lookup(newfolderdialog, "foldertitleentry");
	gtk_entry_set_text(GTK_ENTRY(foldernameentry), "");
		
	gtk_widget_show(newfolderdialog);
}

void on_newfolderbtn_clicked(GtkButton *button, gpointer user_data) {
	nodePtr		folder;
	
	/* create folder node */
	folder = node_new();
	node_set_title(folder, (gchar *)gtk_entry_get_text(GTK_ENTRY(liferea_dialog_lookup(newfolderdialog, "foldertitleentry"))));
	node_set_type(folder, folder_get_node_type());

	/* add the new folder to the model */
	node_add_child(NULL, folder, 0);
	feedlist_schedule_save();
	ui_feedlist_select(folder);
}
