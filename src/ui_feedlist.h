/*
   GUI feed list handling
   
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA 
*/

#ifndef _UI_FEEDLIST_H
#define _UI_FEEDLIST_H

#include <gtk/gtk.h>
#include "feed.h"

GtkTreeStore * getFeedStore(void);
void setupFeedList(GtkWidget *mainview);
gboolean getFeedListIter(GtkTreeIter *iter);

void feedlist_selection_changed_cb(GtkTreeSelection *selection, gpointer data);

void addToFeedList(feedPtr fp, gboolean startup);
void subscribeTo(gint type, gchar *source, gchar * keyprefix, gboolean showPropDialog);

/* menu and dialog callbacks */
void on_popup_refresh_selected(void);
void on_popup_delete_selected(void);
void on_popup_prop_selected(void);
void on_propchangebtn_clicked(GtkButton *button, gpointer user_data);
void on_newbtn_clicked(GtkButton *button, gpointer user_data);
void on_newfeedbtn_clicked(GtkButton *button, gpointer user_data);

void on_fileselect_clicked(GtkButton *button, gpointer user_data);
void on_localfilebtn_pressed(GtkButton *button, gpointer user_data);

/* helper function to find next unread item */
feedPtr findUnreadFeed(GtkTreeIter *iter);

#endif
