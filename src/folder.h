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

#include <gtk/gtk.h>
#include <string.h>
#include "feed.h"

/* ------------------------------------------------------------ */
/* functions to create/change/remove folder			*/
/* ------------------------------------------------------------ */
void 	initFolders(void);

/* to create/delete folders */
void	addFolder(gchar *keyprefix, gchar *title, gint type);
void	removeFolder(gchar *keyprefix);

/* to read/change folder properties */
gchar *	getFolderTitle(gchar *keyprefix);
void	setFolderTitle(gchar *keyprefix, gchar *title);
void	setFolderCollapseState(gchar *keyprefix, gboolean collapsed);

/* necessary for drag&drop actions */
void	checkForEmptyFolders(void);
void	moveInFeedList(gchar *oldkeyprefix, gchar *oldkey);

/* save functions */
void	folder_state_save(gchar *keyprefix, GtkTreeIter *iter);
void	saveFolderFeedList(gchar *keyprefix);

#endif
