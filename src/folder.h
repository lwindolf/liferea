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
	gchar *title;
	gchar *id;
} *folderPtr;

/* ------------------------------------------------------------ */
/* functions to create/change/remove folder			*/
/* ------------------------------------------------------------ */
void initFolders(void);
void folder_add_feed(folderPtr foldr, struct feed *feed, gint position);

/**
 * Free an empty folder structure, removing it from the feedlist if
 * necessary.
 */
void folder_free(folderPtr folder);

/* to create/delete folders */
folderPtr restore_folder(folderPtr parent, gchar *title, gchar *id, gint type);
void removeFolder(folderPtr folder);

/* to read/change folder properties */
gchar* folder_get_title(folderPtr folder);
void	setFolderTitle(folderPtr folder, gchar *title);
void	setFolderCollapseState(struct folder *folder, gboolean collapsed);

/* save functions */
void folder_state_save(nodePtr ptr);

#endif
