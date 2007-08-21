/**
 * @file newsbin.h  news bin node type implementation
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

#ifndef _NEWSBIN_H
#define _NEWSBIN_H

#include <gtk/gtk.h>
#include "node_type.h"

/**
 * Returns a list of the names of all news bins
 */
GSList * newsbin_get_list(void);

/* UI callbacks */

void on_newnewsbinbtn_clicked(GtkButton *button, gpointer user_data);

void on_newsbinnamechange_clicked(GtkButton *button, gpointer user_data);

void on_popup_copy_to_newsbin(gpointer user_data, guint callback_action, GtkWidget *widget);

/* implementation of the node type interface */

#define IS_NEWSBIN(node) (node->type == newsbin_get_node_type ())

/** 
 * Returns the news bin node type implementation.
 */
nodeTypePtr newsbin_get_node_type (void);

#endif
