/*
   Liferea main program

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
#include <locale.h> /* For setlocale */
#include "interface.h"
#include "support.h"
#include "callbacks.h"
#include "feed.h"
#include "conf.h"
#include "common.h"
#include "update.h"
#include "ui_queue.h"
#include "ui_mainwindow.h"

GThread	*mainThread = NULL;

int main (int argc, char *argv[]) {	

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

	add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");
	add_pixmap_directory(getCachePath());

	mainwindow = create_mainwindow();

	/* order is important! */
	initConfig();		/* initialize gconf */
	loadConfig();		/* maybe this should be merged with initConfig() */
	ui_init();		/* initialize gconf configured GUI behaviour */
	feed_init();

	/* we need to now this when locking in ui_queue.c */
	mainThread = g_thread_self();	

	/* setup the processing of feed update results */
	ui_timeout_add(100, checkForUpdateResults, NULL);

	gtk_widget_show(mainwindow);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	
	return 0;
}
