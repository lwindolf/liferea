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


/** Loads or merges the passed feeds items into the itemlist.  If the
 * selected feed is equal to the passed one we do merging. Otherwise
 * we can just clear the list and load the new items.
 */
void itemlist_load(nodePtr node);

void itemlist_set_two_pane_mode(gboolean new_mode);

/* item handling functions */

void itemlist_add_item(feedPtr fp, itemPtr ip);

void itemlist_update_item(itemPtr ip);

void itemlist_remove_item(itemPtr ip);

void itemlist_remove_items(feedPtr fp);

/**
 * To be called whenever a feed was updated. If it is a somehow
 * displayed feed it is loaded this method decides if the
 * and how the item list GUI needs to be updated.
 */
void itemlist_reload(nodePtr node);

/**
 * Toggle the flag of the item currently selected in the itemlist
 */
void itemlist_toggle_flag(itemPtr ip);

void itemlist_toggle_read_status(itemPtr ip);

void itemlist_set_read_status(itemPtr ip, gboolean newStatus);

/**
 * Unsets bot the unread and update flag for all items
 * of the given feed.
 */
void itemlist_mark_all_read(nodePtr fp);

void itemlist_update_vfolder(feedPtr vp);

void itemlist_sort_column_changed_cb(GtkTreeSortable *treesortable, gpointer user_data);

void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data);

/** Force the itemlist to re-create the displayed dates based on the
 *  current date format setting 
 */
void itemlist_reset_date_format(void);

#endif
