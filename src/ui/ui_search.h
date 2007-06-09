/**
 * @file ui_search.h everything about searching
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _UI_SEARCH_H
#define _UI_SEARCH_H

#include <gtk/gtk.h>

/**
 * Search initialization (search engine registration).
 * Needs to be called before the main menu is created.
 */
void ui_search_init(void);

/**
 * Adds available search engines to the Search menu.
 *
 * @param ui_manager	an GTK UI Manager
 */
void ui_search_engines_setup_menu(GtkUIManager *ui_manager);

void on_searchbtn_clicked(GtkButton *button, gpointer user_data);
void on_searchentry_activate(GtkEntry *entry, gpointer user_data);
void on_searchentry_changed(GtkEditable *editable, gpointer user_data);

void on_newVFolder_clicked(GtkButton *button, gpointer user_data);

void on_new_vfolder_activate(GtkMenuItem *menuitem, gpointer user_data);

void on_search_engine_btn_clicked(GtkButton *button, gpointer user_data);

#endif
