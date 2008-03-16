/**
 * @file search_enging_dialog.h  Search engine subscription dialog
 *
 * Copyright (C) 2007-2008 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _SEARCH_ENGINE_DIALOG_H
#define _SEARCH_ENGINE_DIALOG_H

#include <gtk/gtk.h> 
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define SEARCH_ENGINE_DIALOG_TYPE		(search_engine_dialog_get_type ())
#define SEARCH_ENGINE_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), SEARCH_ENGINE_DIALOG_TYPE, SearchEngineDialog))
#define SEARCH_ENGINE_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), SEARCH_ENGINE_DIALOG_TYPE, SearchEngineDialogClass))
#define IS_SEARCH_ENGINE_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEARCH_ENGINE_DIALOG_TYPE))
#define IS_SEARCH_ENGINE_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SEARCH_ENGINE_DIALOG_TYPE))

typedef struct SearchEngineDialog		SearchEngineDialog;
typedef struct SearchEngineDialogClass		SearchEngineDialogClass;
typedef struct SearchEngineDialogPrivate	SearchEngineDialogPrivate;

struct SearchEngineDialog
{
	GObject		parent;
	
	/*< private >*/
	SearchEngineDialogPrivate	*priv;
};

struct SearchEngineDialogClass 
{
	GtkObjectClass parent_class;	
};

GType search_engine_dialog_get_type	(void);

/**
 * Open search engine dialog to create a new search feed.
 *
 * @param uriFmt		search feed URI format string
 * @param limitSupported	search count limit supported
 *
 * @returns the new dialog
 */
SearchEngineDialog * search_engine_dialog_new (const gchar *uriFmt, gboolean limitSupported);

G_END_DECLS
 
#endif
