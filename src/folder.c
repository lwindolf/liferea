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
