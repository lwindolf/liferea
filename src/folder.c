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

/* ---------------------------------------------------------------------------- */
/* folder handling stuff (thats not the VFolder handling!)			*/
/* ---------------------------------------------------------------------------- */

void initFolders(void) {
}

/* Used to add a folder without adding it to the config */
folderPtr restore_folder(folderPtr parent, gchar *title, gchar *id, gint type) {
	folderPtr folder;

	g_assert(IS_FOLDER(type));

	folder = g_new0(struct folder, 1);
	folder->type = type;
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

void setFolderTitle(folderPtr folder, gchar *title) {
	g_assert(folder != NULL);
	g_assert(title != NULL);
	
	folder->title = g_strdup(title);

	/* topiter must not be NULL! because we cannot rename the root folder ! */
	conf_feedlist_schedule_save();
	conf_feedlist_schedule_save();
}

void folder_add_feed(folderPtr folder, feedPtr feed, gint position) {
	conf_feedlist_schedule_save();
}

void folder_free(folderPtr folder) {
	if (folder->ui_data)
		ui_folder_remove_node((nodePtr)folder);
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
	GtkTreeIter iter;
	
	g_assert(NULL != folder);
	g_assert(NULL != dest_folder);
	g_assert(folder->ui_data);
	g_assert(dest_folder->ui_data);

	expanded = ui_is_folder_expanded(folder);

	// Make new folder
	newFolder = restore_folder(dest_folder, folder->title, folder->id, folder->type);

	ui_add_folder(dest_folder, newFolder, position);

	// Recursivly move children
	while (gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &iter, &((ui_data*)(folder->ui_data))->row)) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter,
					    FS_PTR, &ptr,
					    -1);
		if (ptr) {
			if (IS_FOLDER(ptr->type)) {
				folder_set_pos((folderPtr)ptr, newFolder, -1);
			} else if (FEED_MENU(ptr->type)) {
				feed_set_pos((feedPtr)ptr, newFolder, -1);
			} else {
				g_warning("An unknown type=%d is trying to be moved with a folder! It is now deleted.", ptr->type);
				gtk_tree_store_remove(feedstore, &iter); 
			}
		} else /* EMPTY */
			gtk_tree_store_remove(feedstore, &iter); 
	}
	if (expanded)
		ui_folder_set_expansion(newFolder, TRUE);

	/* Delete (empty) old folder */
	ui_folder_remove_node((nodePtr)folder);
	g_assert(folder->ui_data == NULL);
	folder_free(folder);
	conf_feedlist_schedule_save();
}

/*------------------------------------------------------------------------------*/
/* function for finding next unread item 					*/
/*------------------------------------------------------------------------------*/
 
feedPtr folder_find_unread_feed(folderPtr folder) {
	feedPtr			fp;
	nodePtr			ptr;
	GtkTreeIter		iter, *parent = NULL;
	gboolean valid;
	gint count;

	if (folder != NULL)
		parent = &((ui_data*)(folder->ui_data))->row;
	
	valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &iter, parent);
	while(valid) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter,
				   FS_PTR, &ptr,
				   FS_UNREAD, &count,
				   -1);
		if (count > 0) {
			if (IS_FEED(ptr->type)) {
				return (feedPtr)ptr;
			} else if (IS_FOLDER(ptr->type)) {
				if ((fp = folder_find_unread_feed((folderPtr)ptr)))
					return fp;
			} /* Directories are never checked */
		}
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	}
	return NULL;	
}
