/*
   item list/view handling
   
   Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>

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

#ifndef _UI_ITEMLIST_H
#define _UI_ITEMLIST_H

#include <gtk/gtk.h>
#include "feed.h"

GtkTreeStore * getItemStore(void);
void setupItemList(GtkWidget *itemlist);
void preFocusItemlist(void);
void loadItemList(feedPtr fp, gchar *searchstring);
void clearItemList(void);

/* mouse/keyboard interaction callbacks */
void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data);

gboolean
on_Itemlist_move_cursor                (GtkTreeView     *treeview,
                                        GtkMovementStep  step,
                                        gint             count,
                                        gpointer         user_data);

void
on_Itemlist_row_activated              (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data);

/* menu callbacks */					
void on_toggle_item_flag(void);
void on_toggle_unread_status(void);
void on_popup_launchitem_selected(void);
void on_popup_allunread_selected(void);
void on_remove_items_activate(GtkMenuItem *menuitem, gpointer  user_data);
void on_toggle_condensed_view_activate(GtkMenuItem *menuitem, gpointer user_data);
void on_popup_toggle_condensed_view(gpointer cb_data, guint cb_action, GtkWidget *item);

/* Resets the horizontal and vertical scrolling of the items HTML view. */
void resetItemViewScrolling(GtkScrolledWindow *itemview);

/* Function scrolls down the item views scrolled window.
   This function returns FALSE if the scrolled window
   vertical scroll position is at the maximum and TRUE
   if the vertical adjustment was increased. */
gboolean scrollItemView(GtkWidget *itemView);

#endif
