/**
 * @file itemlist.h itemlist handling
 *
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _ITEMLIST_H
#define _ITEMLIST_H

#include <gtk/gtk.h>
#include "item.h"
#include "feed.h"

/* FIXME */

void itemlist_add_item(feedPtr fp, itemPtr ip);

void itemlist_remove_item(itemPtr ip);

void itemlist_remove_items(feedPtr fp);

/**
 * Unsets bot the unread and update flag for all items
 * of the given feed.
 */
void itemlist_mark_all_read(nodePtr fp);

void itemlist_sort_column_changed_cb(GtkTreeSortable *treesortable, gpointer user_data);

void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data);

#endif
