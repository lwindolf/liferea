/**
 * @file ui_script.h UI dialogs concerning script configuration
 *
 * Copyright (C) 2006-2007 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _UI_SCRIPT_H
#define _UI_SCRIPT_H

#include <gtk/gtk.h>

void on_menu_show_script_manager(GtkWidget *widget, gpointer user_data);

void on_scriptCmdExecBtn_clicked(GtkButton *button, gpointer user_data);

void on_cmdEntry_activate(GtkEntry *entry, gpointer user_data);

void on_scriptAddBtn_clicked(GtkButton *button, gpointer user_data);

void on_scriptRemoveBtn_clicked(GtkButton *button, gpointer user_data);

void on_scriptSaveBtn_clicked(GtkButton *button, gpointer user_data);

void on_scriptRunBtn_clicked(GtkButton *button, gpointer user_data);

#endif
