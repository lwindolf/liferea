/*
   folder handling

   Copyright (C) 2003,2004 Lars Lindner <lars.lindner@gmx.net>
   
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include "support.h"
#include "common.h"
#include "conf.h"
#include "feed.h"
#include "folder.h"
#include "callbacks.h"

folderPtr rootFolder = NULL;

/* ---------------------------------------------------------------------------- */
/* folder handling stuff (thats not the VFolder handling!)			*/
/* ---------------------------------------------------------------------------- */

void initFolders(void) {
	g_assert(rootFolder == NULL);
	rootFolder = g_new0(struct folder, 1);
	rootFolder->type = FST_FOLDER;
	rootFolder->title = g_strdup("Root Feed");
	rootFolder->id = g_strdup("root");
}

folderPtr folder_get_root() {
	return rootFolder;
}

/* Used to add a folder without adding it to the config */
folderPtr restore_folder(folderPtr parent, gint position, gchar *title, gchar *id, gint type) {
	folderPtr folder;

	g_assert(IS_FOLDER(type));

	folder = g_new0(struct folder, 1);
	folder->type = type;
	folder->parent = parent;
	folder->parent->children = g_slist_insert(folder->parent->children, folder, position);
	folder->title = g_strdup(title);
	if (id == NULL)
		folder->id = conf_new_id();
	else
		folder->id = g_strdup(id);

	conf_feedlist_schedule_save();
	return folder;
}

gchar* folder_get_title(folderPtr folder) {
	return folder->title;
}

gchar* folder_get_conf_path(folderPtr folder) {
	if (folder->parent) {
		gchar *parentPath = folder_get_conf_path(folder->parent);
		gchar *path = g_strdup_printf("%s/%s", parentPath, folder->id);
		g_free(parentPath);
		return path;
	} else {
		return g_strdup_printf("%s", folder->id);
	}
}

void setFolderTitle(folderPtr folder, gchar *title) {
	g_assert(folder != NULL);
	g_assert(title != NULL);
	
	folder->title = g_strdup(title);

	/* topiter must not be NULL! because we cannot rename the root folder ! */
	conf_feedlist_schedule_save();
	if (folder->ui_data)
		ui_update_folder(folder);
	conf_feedlist_schedule_save();
}

void folder_add_feed(folderPtr folder, feedPtr feed, gint position) {
	folder->children = g_slist_insert(folder->children, feed, position);
	conf_feedlist_schedule_save();
}

void folder_remove(folderPtr folder) {
	if (folder->ui_data)
		ui_remove_folder(folder);
	folder->parent->children = g_slist_remove(folder->parent->children, folder);
	if (folder->title)
		g_free(folder->title);
	if (folder->id)
		g_free(folder->id);
	g_free(folder);
	conf_feedlist_schedule_save();
}

void folder_state_save(nodePtr ptr) {
	folderPtr folder = (folderPtr)ptr;
	g_assert(folder);
	g_assert(IS_FOLDER(folder->type));
}

void folder_set_pos(folderPtr folder, folderPtr dest_folder, int position) {
	folderPtr newFolder;
	gboolean expanded=FALSE;
	nodePtr ptr;
	GSList *iter;

	g_assert(NULL != folder);
	g_assert(NULL != dest_folder);
	g_assert(folder->ui_data);
	g_assert(dest_folder == folder_get_root() || dest_folder->ui_data);

	verify_iter((nodePtr)folder);
	expanded = ui_is_folder_expanded(folder);

	// Make new folder
	newFolder = restore_folder(dest_folder, position, folder->title, folder->id, folder->type);
	// FIXME: Save feedlist here
	ui_add_folder(newFolder);
	g_message("folder_set_pos");
	verify_iter((nodePtr)newFolder);
	// Recursivly move children
	while ((iter = folder->children)) {
		ptr = (nodePtr)(iter->data);
		if (IS_FOLDER(ptr->type)) {
			g_message("5");
			folder_set_pos((folderPtr)ptr, newFolder, -1);
		} else if (FEED_MENU(ptr->type)) {
			g_message("4");
			feed_set_pos((feedPtr)ptr, newFolder, -1);
		} else {
			g_warning("An unknown type=%d is trying to be moved inside a folder!", ptr->type);
			folder->children = g_slist_delete_link(folder->children, folder->children);
		}
	}
	if (expanded)
		ui_folder_set_expansion(newFolder, TRUE);

	/* Delete (empty) old folder */
	ui_remove_folder(folder);
	g_assert(folder->ui_data == NULL);
	folder_remove(folder);
	conf_feedlist_schedule_save();
}

/*------------------------------------------------------------------------------*/
/* function for finding next unread item 					*/
/*------------------------------------------------------------------------------*/
 
feedPtr folder_find_unread_feed(folderPtr folder) {
	feedPtr			fp;
	nodePtr			ptr;

	GSList *iter;
	if (folder == NULL)
		folder = folder_get_root();

	iter = folder->children;
	while(iter) {
		ptr = (nodePtr)iter->data;
		if (IS_FEED(ptr->type)) {
			if (feed_get_unread_counter((feedPtr)ptr) > 0)
				return (feedPtr)ptr;
		} else if (IS_FOLDER(ptr->type)) {
			if ((fp = folder_find_unread_feed((folderPtr)ptr)))
				return fp;
		}
		iter = g_slist_next(iter);
	}
	return NULL;	
}
