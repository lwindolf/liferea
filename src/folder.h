/*
   folder handling interface

   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

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

#ifndef _FOLDER_H
#define _FOLDER_H

#include <string.h>
#include "common.h"
#include "feed.h"

typedef struct folder {
	gint type;
	gpointer *ui_data;
	struct folder *parent;
	gchar *title;
	gchar *id; /* The gconf key name of the directory, for example "dir5" */
	GSList *children; /* These can be nodePtrs */
} *folderPtr;

struct feed;

void folder_set_pos(folderPtr folder, folderPtr destFolder, int position);
/* ------------------------------------------------------------ */
/* functions to create/change/remove folder			*/
/* ------------------------------------------------------------ */
void 	initFolders(void);
folderPtr folder_get_root();
gchar* folder_get_conf_path(folderPtr folder);
void folder_add_feed(folderPtr foldr, struct feed *feed, gint position);
void folder_remove(folderPtr folder);

/* to create/delete folders */
folderPtr get_new_folder(folderPtr parent, gint position, gchar *title, gint type);
folderPtr restore_folder(folderPtr parent, gint position, gchar *title, gchar *key, gint type);
void removeFolder(folderPtr folder);

/* to read/change folder properties */
gchar* folder_get_title(folderPtr folder);
void	setFolderTitle(folderPtr folder, gchar *title);
void	setFolderCollapseState(struct folder *folder, gboolean collapsed);

/* save functions */
void folder_state_save(nodePtr ptr);

/* Misc */
struct feed* folder_find_unread_feed(folderPtr folder);
#endif
