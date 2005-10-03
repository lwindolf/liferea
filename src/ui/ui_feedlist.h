/**
 * @file ui_feedlist.h GUI feed list handling
 * 
 * Copyright (C) 2004-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2005 Raphaël Slinckx <raphael@slinckx.net>
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

#ifndef _UI_FEEDLIST_H
#define _UI_FEEDLIST_H

#include <gtk/gtk.h>
#include "feed.h"
#include "folder.h"

/* constants for attributes in feedstore */
enum {
	FS_LABEL, /* Displayed name */
	FS_ICON,  /* Icon to use */
	FS_PTR,   /* pointer to the folder or feed */
	FS_UNREAD, /* Number of unread items */
	FS_LEN
};

typedef struct ui_data {
	GtkTreeIter row;
} ui_data;

extern GtkTreeStore	*feedstore;

/* Add/remove/update nodes */

/**
 * Updates all childrens of the given GtkTreeIter
 *
 * @param iter	a folder of the feed list
 */
void ui_feedlist_update_iter(GtkTreeIter *iter);

/**
 * Update the labels of all of the nodes of the feedlist, and update
 * their GtkTreeIter pointers.
 */
#define ui_feedlist_update() (ui_feedlist_update_iter(NULL))

/* Selections */
nodePtr ui_feedlist_get_selected();
void ui_feedlist_select(nodePtr fp);

typedef void 	(*nodeActionFunc)	(nodePtr fp);
typedef void 	(*nodeActionDataFunc)	(nodePtr fp, gpointer user_data);

/**
 * Initializes the feed list. For example, it creates the various
 * columns and renderers needed to show the list.
 */
void ui_feedlist_init(GtkWidget *mainview);

/** 
 * Determines the currently selected feed list iterator.
 *
 * @param iter	pointer to iter structure to return selected iter
 */
gboolean ui_feedlist_get_iter(GtkTreeIter *iter);

/**
 * Returns the parent folder of a node, or NULL if the root folder is
 * the parent.
 * 
 * @returns the parent folder.
 */

folderPtr ui_feedlist_get_parent(nodePtr ptr);

/* Selects the proper destination for a new feed based on which feed
 * is currently selected.
 *
 * @param pos a pointer to an integer that will be set to where the
 * item should be created. If it is set to -1, then it should be
 * appended. 0 means prepend.
 *
 * @returns folder into which the feed should be inserted
 */
folderPtr ui_feedlist_get_target_folder(int *pos);

/**
 * Create a new subscription in the currently selected folder.
 *
 * @param source	feed source URL or local file name or piped command
 * @param filter    filename of the filter to use for the new feed
 * @param flags feed request flags to pass to the update requesting subsystem
 */
void ui_feedlist_new_subscription(const gchar *source, const gchar *filter, gint flags);


#define	ACTION_FILTER_FEED	1	/** Only matches nodes where IS_FEED(node->type) */	
#define	ACTION_FILTER_DIRECTORY	2	/** Only matches nodes where IS_DIRECTORY(node->type) */	
#define	ACTION_FILTER_FOLDER	4	/** Only matches nodes where IS_FOLDER(node->type) */
#define	ACTION_FILTER_ANY	7	/** Matches any node */
#define	ACTION_FILTER_CHILDREN	8	/** Matches immediate children of the given node */

/**
 * Helper function to recursivly call feed_save() for all
 * elements of the given type in the feed list.
 *
 * @param ptr	node pointer whose children should be processed (NULL defaults to root)
 * @param filter specifies the types of nodes for which func should be called
 * @param func	the function to process all found elements
 * @param params Set to 1 if there will be user_data. Set to 0 for no user data
 * @param user_data specifies the second argument that func should be passed
 */
void ui_feedlist_do_for_all_full(nodePtr ptr, gint filter, gpointer func, gint params, gpointer user_data);

/**
 * Helper function to recursivly call feed_save() for all
 * elements of the given type in the feed list.
 *
 * @param ptr	node pointer whose children should be processed (NULL defaults to root)
 * @param filter specifies the types of nodes for which func should be called
 * @param func	the function to process all found elements
 */
#define ui_feedlist_do_for_all(ptr, filter, func) ui_feedlist_do_for_all_full(ptr,filter,func,0,NULL)

/**
 * Helper function to recursivly call feed_save() for all
 * elements of the given type in the feed list.
 *
 * @param ptr	node pointer whose children should be processed (NULL defaults to root)
 * @param filter specifies the types of nodes for which func should be called
 * @param func	the function to process all found elements
 * @param user_data specifies the second argument that func should be passed
 */
#define ui_feedlist_do_for_all_data(ptr, filter, func, user_data) ui_feedlist_do_for_all_full(ptr,filter,func,1,user_data)

/** 
 * Tries to find the first unread feed in the given folder.
 * 
 * @return feed pointer or NULL
 */
feedPtr	ui_feedlist_find_unread_feed(nodePtr folder);

/**
 * marks all items of the feed of the given tree iter as read 
 *
 * @param iter	an iterator of the feed list to be processed
 */
void ui_feedlist_mark_items_as_unread(GtkTreeIter *iter);

/**
 * Start listening on dbus for new subscriptions
 */
void ui_feedlist_dbus_connect(void);

/**
 * Add a node to the feedlist
 *
 * @param parent	the parent of the new folder, or NULL to 
 *			insert in the root folder
 * @param node		the node to add
 * @param position	the position in which the folder should be 
 *			added, or -1 to append the folder to the parent.
 */
void ui_feedlist_add(nodePtr parent, nodePtr node, gint position);

/**
 * Prompt the user for confirmation of a folder or feed, and
 * recursively remove the feed or folder if the user accepts. This
 * function does not block, so the folder/feeds will not have
 * been deleted when this function returns.
 *
 * @param ptr the node to delete
 */
void ui_feedlist_delete_prompt(nodePtr ptr);

/**
 * Remove a feed without confirmation
 * Compare with ui_feedlist_delete()
 *
 * @param node pointer to the feed node to be deleted.
 */
void ui_feedlist_remove_node(nodePtr node);

/** 
 * @name menu and dialog callbacks 
 * @{
 */
void on_popup_mark_as_read(gpointer callback_data,
                           guint callback_action,
                           GtkWidget *widget);

void on_popup_prop_selected(gpointer callback_data,
					   guint callback_action,
					   GtkWidget *widget);
void on_propchangebtn_clicked(GtkButton *button, gpointer user_data);
void on_newbtn_clicked(GtkButton *button, gpointer user_data);

void on_fileselect_clicked(GtkButton *button, gpointer user_data);
void on_localfilebtn_pressed(GtkButton *button, gpointer user_data);

void on_filter_feeds_without_unread_headlines_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_menu_properties(GtkMenuItem *menuitem, gpointer user_data);
void on_menu_feed_new(GtkMenuItem *menuitem, gpointer user_data);
void on_menu_folder_new(GtkMenuItem *menuitem, gpointer user_data);

/*@}*/

/* UI folder stuff */
void verify_iter(nodePtr node);
GtkWidget *ui_feedlist_build_prop_dialog(void);

#endif
