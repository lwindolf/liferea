/*
   everything concerning DnD
   
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <string.h>		/* For strncmp */
#include "net/os-support.h"	/* for strsep */
#include "support.h"
#include "callbacks.h"
#include "feed.h"
#include "folder.h"
#include "ui_dnd.h"

/* ---------------------------------------------------------------------------- */
/* receiving URLs 								*/
/* ---------------------------------------------------------------------------- */

/* method to receive URLs which were dropped anywhere in the main window */
static void feedURLReceived(GtkWidget *mainwindow, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time) {
	gchar	*tmp1, *tmp2, *freeme;
	nodePtr ptr;
	folderPtr parent;
	g_return_if_fail (data->data != NULL);
	
	if((data->length >= 0) && (data->format == 8)) {
		/* extra handling to accept multiple drops */	

		if(ptr && IS_FOLDER(ptr->type)) {
			parent = (folderPtr)ptr;
		} else if (ptr && ptr->parent) {
			parent = ptr->parent;
		} else {
			parent = folder_get_root();
		}

		freeme = tmp1 = g_strdup(data->data);
		while((tmp2 = strsep(&tmp1, "\n\r"))) {
			if(0 != strlen(tmp2))
				ui_feedlist_new_subscription(FST_AUTODETECT, g_strdup(tmp2), parent, TRUE);
		}
		g_free(freeme);
		gtk_drag_finish(context, TRUE, FALSE, time);		
	} else {
		gtk_drag_finish(context, FALSE, FALSE, time);
	}
}

/* sets up URL receiving */
void setupURLReceiver(GtkWidget *mainwindow) {

	GtkTargetEntry target_table[] = {
		{ "STRING",     		0, 0 },
		{ "text/plain", 		0, 0 },
		{ "text/uri-list",		0, 1 },
		{ "_NETSCAPE_URL",		0, 1 },
		{ "application/x-rootwin-drop", 0, 2 }
	};

	/* doesn't work with GTK_DEST_DEFAULT_DROP... */
	gtk_drag_dest_set(mainwindow, GTK_DEST_DEFAULT_ALL,
			target_table, sizeof(target_table)/sizeof(target_table[0]),
			GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
		       
	gtk_signal_connect(GTK_OBJECT(mainwindow), "drag_data_received",
			G_CALLBACK(feedURLReceived), NULL);
}
