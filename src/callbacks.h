/*
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

#include <gtk/gtk.h>
#include <libgtkhtml/gtkhtml.h>
#include "backend.h"

void 
print_status 			       (gchar 		*statustext);

void
showErrorBox			       (gchar 		*msg);

void
redrawFeedList			       (void);

gchar *
getMainFeedListViewSelection	       (void);

void
on_refreshbtn_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_propchangebtn_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_newbtn_clicked                      (GtkButton       *button,
                                        gpointer         user_data);

void
on_propbtn_clicked                     (GtkButton       *button,
                                        gpointer         user_data);

void
on_aboutbtn_clicked                    (GtkButton       *button,
                                        gpointer         user_data);

void
on_newfeedbtn_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
feedlist_selection_changed_cb          (GtkTreeSelection *selection, 
					gpointer data);

void
itemlist_selection_changed_cb          (GtkTreeSelection *selection, 
					gpointer data);

void
url_requested 			       (HtmlDocument 	*doc, 
				        const gchar 	*uri, 
					HtmlStream 	*stream, 
					gpointer 	data);

void
on_newfeedbutton_clicked               (GtkButton       *button,
                                        gpointer         user_data);

void
on_feednamebutton_clicked              (GtkButton       *button,
                                        gpointer         user_data);
					
void	
setupFeedList			       (GtkWidget 	*mainview);

void
setupItemList			       (GtkWidget 	*itemlist);

void
on_deletebtn_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_prefbtn_clicked                     (GtkButton       *button,
                                        gpointer         user_data);

void
on_prefsavebtn_clicked                 (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_mainfeedlist_button_press_event     (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_newfolderbtn_clicked                (GtkButton       *button,
                                        gpointer         user_data);

gboolean
on_itemlist_button_press_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_searchbtn_clicked                   (GtkButton       *button,
                                        gpointer         user_data);

void
on_hidesearch_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_searchentry_activate                (GtkEntry        *entry,
                                        gpointer         user_data);

void
on_feedlist_drag_end                   (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gpointer         user_data);

void
on_feedlist_drag_begin                 (GtkWidget       *widget,
                                        GdkDragContext  *drag_context,
                                        gpointer         user_data);

gboolean
on_Itemlist_move_cursor                (GtkTreeView     *treeview,
                                        GtkMovementStep  step,
                                        gint             count,
                                        gpointer         user_data);

gboolean
on_itemlist_button_press_event         (GtkWidget       *widget,
                                        GdkEventButton  *event,
                                        gpointer         user_data);

void
on_foldernamechangebtn_clicked         (GtkButton       *button,
                                        gpointer         user_data);

void
on_newVFolder_clicked                  (GtkButton       *button,
                                        gpointer         user_data);

void
on_toolbar_newfeed_clicked             (GtkMenuItem     *menuitem,
                                        gpointer         user_data);

void
on_toolbar_newfolder_clicked           (GtkMenuItem     *menuitem,
                                        gpointer         user_data);
