/**
 * @file ui_itemlist.h item list/view handling
 *
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
 * 		      Nathan J. Conrad <t98502@users.sourceforge.net>
 *	      
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef _UI_ITEMLIST_H
#define _UI_ITEMLIST_H

#include <gtk/gtk.h>
#include <time.h>
#include "feed.h"

/** Enumeration of the columns in the itemstore. */
enum is_columns {
	IS_TITLE,		/**< Name of the item */
	IS_LABEL,		/**< Displayed name */
	IS_ICON,		/**< Pixbuf reference to the item's icon */
	IS_PTR,			/**< Pointer to item sturuct */
	IS_TIME,		/**< Time of item creation */
	IS_TIME_STR,		/**< Time of item creation as a string*/
	IS_TYPE,		/**< Type of feed that the item came from */
	IS_ICON2,		/**< Pixbuf reference to the item's feed's icon */
	IS_LEN			/**< Number of columns in the itemstore */
};

extern feedPtr	displayed_fp;

/**
 * Returns the itemstore, creating it if needed.
 *
 * @return The Itemstore.
 */
GtkTreeStore * getItemStore(void);

/**
 * Initializes the itemlist. For example, it creates the various
 * columns and renderers needed to show the list.
 */
void ui_itemlist_init(GtkWidget *itemlist);

/**
 * Method to reset the format string of the date column.
 * Should be called upon initializaton and each time the
 * date format changes.
 */
void ui_itemlist_reset_date_format(void);

/* methods needs to but should not be exposed... */
gchar * ui_itemlist_format_date(time_t t);

/**
 * Unselect all items in the list and scroll to top. This is typically
 * called when changing feed.
 */
void ui_itemlist_prefocus(void);

/**
 * Adds content to the htmlview after a new feed has been selected and
 * sets an item as read.
 */
void ui_itemlist_display(void);

/**
 * Display a feed's items
 *
 * @param fp The feed to display.
 * @param searchstring The string to search for, or NULL to include all of a feed's items.
 */
void ui_itemlist_load(feedPtr fp, gchar *searchstring);

/**
 * Remove the items from the itemlist.
 */
void ui_itemlist_clear(void);

/**
 * Mark all items in the displayed itemlist as read.
 */
void ui_itemlist_mark_all_as_read(void);

/**
 * @name Callbacks used from interface.c
 * @{
 */

/**
 * Callback activated when an item is double-clicked. It opens the URL
 * of the item in a web browser.
 */

void
on_Itemlist_row_activated              (GtkTreeView     *treeview,
                                        GtkTreePath     *path,
                                        GtkTreeViewColumn *column,
                                        gpointer         user_data);


/* menu callbacks */

/**
 * Toggles the unread status of the selected item. This is called from
 * a menu.
 */
void on_toggle_unread_status(void);

/**
 * Toggles the flag of the selected item. This is called from a menu.
 */
void on_toggle_item_flag(void);

/**
 * Opens the selected item in a browser.
 */
  void on_popup_launchitem_selected(void);

/**
 * Sets all displayed items as read
 */
  void on_popup_allunread_selected(void);

/**
 * Toggles the read status of right-clicked item.
 *
 * @param callback_data An itemPtr that points to the clicked item.
 * @param callback_action Unused.
 * @param widget The GtkTreeView that contains the clicked item.
 */
void on_popup_toggle_read(gpointer callback_data,
					 guint callback_action,
					 GtkWidget *widget);
/**
 * Toggles the flag of right-clicked item.
 *
 * @param callback_data An itemPtr that points to the clicked item.
 * @param callback_action Unused.
 * @param widget The GtkTreeView that contains the clicked item.
 */
void on_popup_toggle_flag(gpointer callback_data,
					 guint callback_action,
					 GtkWidget *widget);

/**
 * Removes all items from the selected feed.
 *
 * @param menuitem The menuitem that was selected.
 * @param user_data Unused.
 */
  void on_remove_items_activate(GtkMenuItem *menuitem, gpointer  user_data);
  
/**
 * Searches the displayed feed and then all feeds for an unread
 * item. If one it found, it is displayed.
 *
 * @param menuitem The menuitem that was selected.
 * @param user_data Unused.
 */
void on_next_unread_item_activate(GtkMenuItem *menuitem, gpointer user_data);
  
/*@}*/

#endif
