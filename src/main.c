/*
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gtk/gtk.h>
#include "interface.h"
#include "support.h"
#include "feed.h"
#include "callbacks.h"
#include "htmlview.h"
#include "conf.h"
#include "update.h"

extern GtkWidget	*mainwindow;

GThread	*mainThread = NULL;
GThread	*updateThread = NULL;

/* icons for itemlist */
GdkPixbuf	*readIcon = NULL;
GdkPixbuf	*unreadIcon = NULL;
/* icons for feedlist */
GdkPixbuf	*availableIcon = NULL;
GdkPixbuf	*unavailableIcon = NULL;
/* icons for OCS */
GdkPixbuf	*listIcon = NULL;
/* icons for grouping */
GdkPixbuf	*directoryIcon = NULL;
GdkPixbuf	*helpIcon = NULL;
GdkPixbuf	*emptyIcon = NULL;
/* VFolder */
GdkPixbuf	*vfolderIcon = NULL;

int main (int argc, char *argv[]) {	
	GtkTooltips	*button_bar_tips;
   	GtkWidget	*viewport;
	
#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	setlocale(LC_ALL, "");
#endif
	g_set_prgname("liferea");
	gtk_set_locale();
	g_thread_init(NULL);
	gdk_threads_init();	
	gtk_init(&argc, &argv);
	gnome_vfs_init();

	add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");
	readIcon = create_pixbuf("read.xpm");
	unreadIcon = create_pixbuf("unread.xpm");
	availableIcon = create_pixbuf("available.xpm");
	unavailableIcon = create_pixbuf("unavailable.xpm");
	listIcon = create_pixbuf("ocs.xpm");	
	directoryIcon = create_pixbuf("directory.xpm");
	helpIcon = create_pixbuf("help.xpm");
	vfolderIcon = create_pixbuf("vfolder.xpm");
	emptyIcon = create_pixbuf("empty.xpm");
	
	mainwindow = create_mainwindow();
	setupHTMLViews(mainwindow, lookup_widget(mainwindow, "itemview"),
			 	   lookup_widget(mainwindow, "itemlistview"));
	gtk_widget_show(mainwindow);
	     			     			     	
	setupFeedList(lookup_widget(mainwindow, "feedlist"));
	setupItemList(lookup_widget(mainwindow, "Itemlist"));
		
	/* order is important! */
	updateThread = initUpdateThread();
	initConfig();
	loadConfig();
	initBackend();
	loadEntries();
	loadVFolders();
		
	/* FIXME: move to somewhere else :) */
	gtk_tree_view_expand_all(GTK_TREE_VIEW(lookup_widget(mainwindow, "feedlist")));

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	
	return 0;
}

