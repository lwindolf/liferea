/**
 * @file ui_feedlist.h GUI feed list handling
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
void ui_update_feed(feedPtr fp); 

/* Selections */
nodePtr ui_feedlist_get_selected();
void ui_feedlist_select(nodePtr fp);


typedef void 	(*nodeActionFunc)		(nodePtr fp);
typedef void 	(*nodeActionDataFunc)	(nodePtr fp, gpointer user_data);

/**
 * Returns the feed store, creating it if needed.
 *
 * @return feed store pointer.
 */
GtkTreeStore * getFeedStore(void);

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
 * Adds a feed to the feed list.
 *
 * @param fp		the feed to add
 * @param startup	should be TRUE on initial subscriptions loading
 */
void ui_feedlist_load_subscription(feedPtr fp, gboolean startup);

/**
 * Create a new subscription.
 *
 * @param type		feed type
 * @param source	feed source URL or local file name or piped command
 * @param folder	parent of the new feed
 * @param showPropDialog TRUE if the property dialog should popup
 */
void ui_feedlist_new_subscription(gint type, gchar *source, folderPtr folder, gboolean showPropDialog);

enum {
	ACTION_FILTER_FEED,		/** Only matches nodes where !IS_FOLDER(node->type) */
						/* FIXME: Should this be changed to IS_FEED(type)? */	
	ACTION_FILTER_FOLDER,	/** Only matches nodes where IS_FOLDER(node->type) */
	ACTION_FILTER_ANY,		/** Matches any node */
	ACTION_FILTER_CHILDREN	/** Matches immediate children of the given node */
};

/**
 * Helper function to recursivly call feed_save() for all
 * elements of the given type in the feed list.
 *
 * @param ptr	node pointer whose children should be processed (NULL defaults to root)
 * @param filter specifies the types of nodes for which func should be called
 * @param func	the function to process all found elements
 */
void ui_feedlist_do_for_all(nodePtr ptr, gint filter, nodeActionFunc func);

/**
 * Helper function to recursivly call feed_save() for all
 * elements of the given type in the feed list.
 *
 * @param ptr	node pointer whose children should be processed (NULL defaults to root)
 * @param filter specifies the types of nodes for which func should be called
 * @param func	the function to process all found elements
 * @param user_data specifies the second argument that func should be passed
 */
void ui_feedlist_do_for_all_data(nodePtr ptr, gint filter, nodeActionDataFunc func, gpointer user_data);
/**
 * helper function to find next unread item 
 *
 * @param iter	an iterator of the feed list to be processed
 */
feedPtr ui_feedlist_find_unread(GtkTreeIter *iter);

/**
 * marks all items of the feed of the given tree iter as read 
 *
 * @param iter	an iterator of the feed list to be processed
 */
void ui_feedlist_mark_items_as_unread(GtkTreeIter *iter);

/** 
 * @name menu and dialog callbacks 
 * @{
 */
void on_popup_refresh_selected(gpointer callback_data,
						 guint callback_action,
						 GtkWidget *widget);
void on_popup_delete_selected(gpointer callback_data,
						guint callback_action,
						GtkWidget *widget);
void on_popup_prop_selected(gpointer callback_data,
					   guint callback_action,
					   GtkWidget *widget);
void on_propchangebtn_clicked(GtkButton *button, gpointer user_data);
void on_newbtn_clicked(GtkButton *button, gpointer user_data);
void on_newfeedbtn_clicked(GtkButton *button, gpointer user_data);

void on_fileselect_clicked(GtkButton *button, gpointer user_data);
void on_localfilebtn_pressed(GtkButton *button, gpointer user_data);
void feedlist_selection_changed_cb(GtkTreeSelection *selection, gpointer data);

/*@}*/

/* UI folder stuff */
void verify_iter(nodePtr node);

#endif
