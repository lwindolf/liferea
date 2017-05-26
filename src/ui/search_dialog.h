/**
 * @file search_dialog.h  Search engine subscription dialog
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
 
#ifndef _SEARCH_DIALOG_H
#define _SEARCH_DIALOG_H

#include <gtk/gtk.h> 
#include <glib-object.h>
#include <glib.h>

G_BEGIN_DECLS

#define SEARCH_DIALOG_TYPE		(search_dialog_get_type ())
#define SEARCH_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), SEARCH_DIALOG_TYPE, SearchDialog))
#define SEARCH_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), SEARCH_DIALOG_TYPE, SearchDialogClass))
#define IS_SEARCH_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SEARCH_DIALOG_TYPE))
#define IS_SEARCH_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SEARCH_DIALOG_TYPE))

typedef struct SearchDialog		SearchDialog;
typedef struct SearchDialogClass	SearchDialogClass;
typedef struct SearchDialogPrivate	SearchDialogPrivate;

struct SearchDialog
{
	GObject		parent;
	
	/*< private >*/
	SearchDialogPrivate	*priv;
};

struct SearchDialogClass 
{
	GObjectClass parent_class;
};

GType search_dialog_get_type	(void);

/**
 * Open the complex singleton search dialog.
 *
 * @param query		optional query string to create a rule for
 *
 * @returns the new dialog
 */
SearchDialog * search_dialog_open (const gchar *query);

#define SIMPLE_SEARCH_DIALOG_TYPE		(simple_search_dialog_get_type ())
#define SIMPLE_SEARCH_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), SIMPLE_SEARCH_DIALOG_TYPE, SimpleSearchDialog))
#define SIMPLE_SEARCH_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), SIMPLE_SEARCH_DIALOG_TYPE, SimpleSearchDialogClass))
#define IS_SIMPLE_SEARCH_DIALOG(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), SIMPLE_SEARCH_DIALOG_TYPE))
#define IS_SIMPLE_SEARCH_DIALOG_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), SIMPLE_SEARCH_DIALOG_TYPE))

typedef struct SimpleSearchDialog		SimpleSearchDialog;
typedef struct SimpleSearchDialogClass		SimpleSearchDialogClass;
typedef struct SimpleSearchDialogPrivate	SimpleSearchDialogPrivate;

struct SimpleSearchDialog
{
	GObject		parent;
	
	/*< private >*/
	SimpleSearchDialogPrivate	*priv;
};

struct SimpleSearchDialogClass 
{
	GObjectClass parent_class;
};

GType simple_search_dialog_get_type	(void);

/**
 * Open the simple (one keyword entry) singleton search dialog.
 *
 * @returns the new dialog
 */
SimpleSearchDialog * simple_search_dialog_open (void);

G_END_DECLS
 
#endif
