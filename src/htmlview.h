/*
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef _HTMLVIEW_H
#define _HTMLVIEW_H

#include <glib.h>
#include <libgtkhtml/gtkhtml.h>
#include <libgnomevfs/gnome-vfs.h>

/* creates the HTML widget */
void 	setupHTMLView(GtkWidget *mainwindow);

/* display an items description in the HTML widget */
void	showItem(gpointer ip, gpointer cp);

/* display a feed info in the HTML widget */
void	showFeedInfo(gpointer cp);

/* display an items description in the HTML widget */
void	showCDFItem(gpointer ip, gpointer cp);

/* display a feed info in the HTML widget */
void	showCDFFeedInfo(gpointer cp);

/* display an directory entry description and its formats in the HTML widget */
void	showDirEntry(gpointer dep, gpointer dp);

/* display a directory info in the HTML widget */
void	showDirectoryInfo(gpointer dp);

#endif
