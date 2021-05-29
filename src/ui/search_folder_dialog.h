/**
 * @file search_folder_dialog.h  Search folder properties dialog
 *
 * Copyright (C) 2007-2018 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _SEARCH_FOLDER_DIALOG_H
#define _SEARCH_FOLDER_DIALOG_H

#include <gtk/gtk.h>

#include "node.h"

G_BEGIN_DECLS

#define SEARCH_FOLDER_DIALOG_TYPE (search_folder_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SearchFolderDialog, search_folder_dialog, SEARCH_FOLDER, DIALOG, GObject)

/**
 * search_folder_dialog_new:
 * Open a new search folder properties dialog
 * for an existing search folder.
 *
 * @node:		the search folder node
 *
 * Returns: (transfer none): a new dialog
 */
SearchFolderDialog * search_folder_dialog_new (nodePtr node);

G_END_DECLS

#endif
