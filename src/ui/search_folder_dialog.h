/**
 * @file search_folder_dialog.h  Search folder properties dialog
 *
 * Copyright (C) 2007-2008 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _SEARCH_FOLDER_DIALOG_H
#define _SEARCH_FOLDER_DIALOG_H

#include <gtk/gtk.h> 
#include <glib-object.h>
#include <glib.h>

#include "node.h"

G_BEGIN_DECLS

#define SEARCH_FOLDER_DIALOG_TYPE		(search_folder_dialog_get_type ())
#define SEARCH_FOLDER_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), SEARCH_FOLDER_DIALOG_TYPE, SearchFolderDialog))
#define SEARCH_FOLDER_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), SEARCH_FOLDER_DIALOG_TYPE, SearchFolderDialogClass))
#define IS_SEARCH_FOLDER_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEARCH_FOLDER_DIALOG_TYPE))
#define IS_SEARCH_FOLDER_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SEARCH_FOLDER_DIALOG_TYPE))

typedef struct SearchFolderDialog		SearchFolderDialog;
typedef struct SearchFolderDialogClass		SearchFolderDialogClass;
typedef struct SearchFolderDialogPrivate	SearchFolderDialogPrivate;

struct SearchFolderDialog
{
	GObject		parent;
	
	/*< private >*/
	SearchFolderDialogPrivate	*priv;
};

struct SearchFolderDialogClass 
{
	GObjectClass parent_class;
};

GType search_folder_dialog_get_type	(void);

/**
 * Open a new search folder properties dialog 
 * for an existing search folder.
 *
 * @param node		the search folder node
 *
 * @returns new dialog
 */
SearchFolderDialog * search_folder_dialog_new (nodePtr node);

G_END_DECLS
 
#endif
