/*
   everything concerning Liferea and DnD
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

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

#include "support.h"
#include "callbacks.h"
#include "feed.h"
#include "ui_dnd.h"

extern gchar	*selected_keyprefix;
extern gint	selected_type;
extern feedPtr	selected_fp;

/* flag to check if DND should be aborted (e.g. on folders and help feeds) */
static gboolean	drag_successful = FALSE;

/* Flag to allow both url and tree iter drops in the feed list. This flag
   is set when feedlist DnD is active. And the feedlist drop handler will
   only work if this flag is set. This leaves still a chance that the
   user simultanously drags in the feed list and drops a URL...
   How? Hmm, maybe with two mices! Anyway the data type check in the URL
   drop handler should not handle the dropped tree iter. */
static gboolean is_feedlist_drop = FALSE;

/*------------------------------------------------------------------------------*/
/* feed list DND handling							*/
/*------------------------------------------------------------------------------*/

void on_feedlist_drag_end(GtkWidget *widget, GdkDragContext  *drag_context, gpointer user_data) {

	g_assert(NULL != selected_keyprefix);
	
	if(drag_successful) {	
		moveInFeedList(selected_keyprefix, getFeedKey(selected_fp));
		checkForEmptyFolders();	/* to add an "(empty)" entry */
	}
	
	preFocusItemlist();
}

void on_feedlist_drag_begin(GtkWidget *widget, GdkDragContext  *drag_context, gpointer user_data) {

	drag_successful = FALSE;
	is_feedlist_drop = TRUE;
}

/* reacts on drops of feed list tree iters */
gboolean on_feedlist_drag_drop(GtkWidget *widget, GdkDragContext *drag_context, gint x, gint y, guint time, gpointer user_data) {
	gboolean	stop = FALSE;

	g_assert(NULL != selected_keyprefix);

	/* don't process URL drops... */
	if(!is_feedlist_drop)
		return FALSE;
		
	is_feedlist_drop = FALSE;

	/* don't allow folder DND */
	if(IS_NODE(selected_type)) {
		showErrorBox(_("Sorry Liferea does not yet support drag&drop of folders!"));
		stop = TRUE;
	} 
	/* also don't allow "(empty)" entry moving */
	else if(FST_EMPTY == selected_type) {
		stop = TRUE;
	} 
	/* also don't allow help feed dragging */
	else if(0 == strncmp(getFeedKey(selected_fp), "help", 4)) {
		showErrorBox(_("you cannot modify the special help folder contents!"));
		stop = TRUE;
	}
	
	drag_successful = !stop;
	
	return stop;
}

/* ---------------------------------------------------------------------------- */
/* receiving URLs 								*/
/* ---------------------------------------------------------------------------- */

/* method to receive URLs which were dropped anywhere in the main window */
static void feedURLReceived(GtkWidget *mainwindow, GdkDragContext *context, gint x, gint y, GtkSelectionData *data, guint info, guint time) {	
	gchar	*tmp1, *tmp2, *tmp3;
	
	g_return_if_fail (data->data != NULL);
	
	if((data->length >= 0) && (data->format == 8)) {
		/* extra handling to accept multiple drops */	
		tmp3 = tmp2 = tmp1 = g_strdup(data->data);
		while(*tmp1) {
			while(*tmp1 != '\n')
				tmp1++;
			*(tmp1 - 1) = 0;
			
			subscribeTo(FST_AUTODETECT, tmp2, g_strdup(selected_keyprefix), TRUE);			
			tmp2 = ++tmp1;
		}
		g_free(tmp3);
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
