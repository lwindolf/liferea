/*
   popup menus

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

#ifndef _UI_POPUP_H
#define _UI_POPUP_H

#include <gtk/gtk.h>
#include <gdk/gdk.h>

#define STDFEED_MENU	0
#define OCS_MENU	1
#define NODE_MENU	2
#define VFOLDER_MENU	3
#define DEFAULT_MENU	4

#define ITEM_MENU	5
#define HTML_MENU	6
#define URL_MENU	7

/* function to generate a generic menu specified by its number */
GtkMenu *make_menu(gint nr);

/* function to generate popup menus for the item list depending
   on the list mode given in itemlist_mode */
GtkMenu *make_item_menu(void);

#endif
