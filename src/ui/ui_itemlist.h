/**
 * @file ui_itemlist.h item list GUI handling
 *
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include "item.h"

/** Enumeration of the columns in the itemstore. */
enum is_columns {
	IS_TIME,		/**< Time of item creation */ /* This is set to the first item so that default sorting is by time */
	IS_TIME_STR,		/**< Time of item creation as a string*/
	IS_LABEL,		/**< Displayed name */
	IS_STATEICON,		/**< Pixbuf reference to the item's state icon */
	IS_NR,			/**< Item id, to lookup item ptr from parent feed */
	IS_PARENT,		/**< Parent node pointer */
	IS_FAVICON,		/**< Pixbuf reference to the item's feed's icon */
	IS_ENCICON,		/**< Pixbuf reference to the item's enclosure icon */
	IS_ENCLOSURE,		/**< Flag wether enclosure is attached or not */
	IS_SOURCE,		/**< Source node pointer */
	IS_STATE,		/**< Original item state (unread, flagged...) for sorting */
	IS_LEN			/**< Number of columns in the itemstore */
};

/**
 * Initializes the itemlist. For example, it creates the various
 * columns and renderers needed to show the list.
 */
GtkWidget* ui_itemlist_new (void);

// FIXME: use GObject
void ui_itemlist_destroy (void);

/**
 * Checks wether the given id is currently displayed.
 *
 * @param id	the item id
 *
 * @returns TRUE if the item is in the tree view 
 */
gboolean ui_itemlist_contains_item(gulong id);

void ui_itemlist_reset_tree_store(void);

/**
 * This returns the GtkTreeStore that is internal to the
 * ui_itemlist. This is currently used for setting and getting the
 * sort column.
 */
GtkTreeStore * ui_itemlist_get_tree_store(void);

/**
 * Unselect all items in the list and scroll to top. This is typically
 * called when changing feed.
 */
void ui_itemlist_prefocus(void);

/**
 * Selects the given item (if it is in the current item list).
 *
 * @param ip	the item to select
 */
void ui_itemlist_select(itemPtr ip);

/**
 * Add an item to the itemlist
 *
 * @param ip	the item to add
 */
void ui_itemlist_add_item(itemPtr item);

/**
 * Remove an item from the itemlist
 */
void ui_itemlist_remove_item(itemPtr ip);

/**
 * Enable the favicon column of the currently displayed itemlist
 */
void ui_itemlist_enable_favicon_column(gboolean enabled);

/**
 * Enable the enclosure column of the currently displayed itemlist
 */
void ui_itemlist_enable_encicon_column(gboolean enabled);

/**
 * Remove the items from the itemlist.
 */
void ui_itemlist_clear(void);


/**
 * When switching tabs, the horizontal scrolling sometimes gets messed
 * up. This reverses that.
 */
void ui_itemlist_scroll_left();

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

/**
 * Callback for column selection change.
 */
void itemlist_sort_column_changed_cb(GtkTreeSortable *treesortable, gpointer user_data);

/**
 * Callback for item list selection change.
 */
void on_itemlist_selection_changed(GtkTreeSelection *selection, gpointer data);

/* menu callbacks */

/**
 * Toggles the unread status of the selected item. This is called from
 * a menu.
 */
void on_toggle_unread_status(GtkMenuItem *menuitem, gpointer user_data);

/**
 * Toggles the flag of the selected item. This is called from a menu.
 */
void on_toggle_item_flag(GtkMenuItem *menuitem, gpointer user_data);

/**
 * Opens the selected item in a browser.
 */
void on_popup_launchitem_selected(void);

/**
 * Opens the selected item in a browser.
 */
void on_popup_launchitem_in_tab_selected(void);

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
void on_remove_items_activate(GtkMenuItem *menuitem, gpointer user_data);

/**
 * Removes the selected item from the selected feed.
 *
 * @param menuitem The menuitem that was selected.
 * @param user_data Unused.
 */  
void on_remove_item_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_popup_remove_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);

/**
 * Finds and selects the next unread item starting at the given
 * item in the current item list according to the current 
 * GtkTreeView sorting order.
 *
 * @param startId	0 or the item id to start from
 *
 * @returns item if an unread item was found
 */
itemPtr ui_itemlist_find_unread_item(gulong startId);

/**
 * Searches the displayed feed and then all feeds for an unread
 * item. If one it found, it is displayed.
 *
 * @param menuitem The menuitem that was selected.
 * @param user_data Unused.
 */
void on_next_unread_item_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_popup_next_unread_item_selected(gpointer callback_data, guint callback_action, GtkWidget *widget);

void on_nextbtn_clicked(GtkButton *button, gpointer user_data);

/**
 * Update a single item of the currently displayed item list.
 *
 * @param item	the item
 */
void ui_itemlist_update_item (itemPtr item);

/**
 * Update all items of the currently displayed item list.
 */
void ui_itemlist_update_all_items (void);

/**
 * Launches the configured social bookmarking site for the given item
 *
 * @param item	the item
 */
void ui_itemlist_add_item_bookmark(itemPtr item);

/**
 * Copies the selected items URL to the clipboard.
 */
void on_popup_copy_URL_clipboard(void);

void on_popup_social_bm_item_selected(void);

void on_popup_social_bm_link_selected(gpointer selectedUrl, guint callback_action, GtkWidget *widget);

/*@}*/

#endif
