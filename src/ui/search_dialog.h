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

#include <glib.h>

/**
 * search_dialog_open:
 * 
 * Open the complex singleton search dialog.
 *
 * @query:		optional query string to create a rule for
 */
void search_dialog_open (const gchar *query);

/**
 * simple_search_dialog_open:
 *
 * Open the simple (one keyword entry) singleton search dialog.
 */
void simple_search_dialog_open (void);

#endif
