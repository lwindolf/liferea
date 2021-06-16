/**
 * @file search_dialog.h  Search engine subscription dialog
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

#ifndef _SEARCH_DIALOG_H
#define _SEARCH_DIALOG_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SEARCH_DIALOG_TYPE (search_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SearchDialog, search_dialog, SEARCH, DIALOG, GObject)

/**
 * search_dialog_open:
 * Open the complex singleton search dialog.
 *
 * @query:		optional query string to create a rule for
 *
 * Returns: (transfer none): the new dialog
 */
SearchDialog * search_dialog_open (const gchar *query);

#define SIMPLE_SEARCH_DIALOG_TYPE		(simple_search_dialog_get_type ())
G_DECLARE_FINAL_TYPE (SimpleSearchDialog, simple_search_dialog, SIMPLE_SEARCH, DIALOG, GObject)

/**
 * simple_search_dialog_open:
 * Open the simple (one keyword entry) singleton search dialog.
 *
 * Returns: (transfer none): the new dialog
 */
SimpleSearchDialog * simple_search_dialog_open (void);

G_END_DECLS

#endif
