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
#include "htmlview.h"
#include "feed.h"
#include "conf.h"
#include "update.h"
#include "netio.h"
#include "common.h"
#include "ui_mainwindow.h"

GThread	*mainThread = NULL;
GThread	*updateThread = NULL;

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
	gnome_vfs_init();

	add_pixmap_directory(PACKAGE_DATA_DIR "/" PACKAGE "/pixmaps");
	add_pixmap_directory(getCachePath());

	mainwindow = create_mainwindow();

	/* order is important! */
	initConfig();		/* initialize gconf */
	loadConfig();		/* maybe this should be merged with initConfig() */
	initGUI();		/* initialize gconf configured GUI behaviour */
	initBackend();
	updateThread = initUpdateThread();	/* start thread for update processing */
	initAutoUpdateThread();	/* start thread for automatic updating */

	loadEntries();		/* load feed list from gconf */

	if(getBooleanConfValue(UPDATE_ON_STARTUP))
		updateAllFeeds();

	gtk_widget_show(mainwindow);
			
	/* load window position */
	if((0 != getNumericConfValue(LAST_WINDOW_X)) && 
	   (0 != getNumericConfValue(LAST_WINDOW_Y)))
	   	gtk_window_move(GTK_WINDOW(mainwindow), getNumericConfValue(LAST_WINDOW_X),
					 		getNumericConfValue(LAST_WINDOW_Y));

	/* load window size */
	if((0 != getNumericConfValue(LAST_WINDOW_WIDTH)) && 
	   (0 != getNumericConfValue(LAST_WINDOW_HEIGHT)))
	   	gtk_window_resize(GTK_WINDOW(mainwindow), getNumericConfValue(LAST_WINDOW_WIDTH),
					 		  getNumericConfValue(LAST_WINDOW_HEIGHT));
	
	/* load pane proportions */
	if(0 != getNumericConfValue(LAST_VPANE_POS))
		gtk_paned_set_position(GTK_PANED(lookup_widget(mainwindow, "leftpane")), getNumericConfValue(LAST_VPANE_POS));
	if(0 != getNumericConfValue(LAST_HPANE_POS))
		gtk_paned_set_position(GTK_PANED(lookup_widget(mainwindow, "rightpane")), getNumericConfValue(LAST_HPANE_POS));

	/* add timeout function to check for update results from the update thread */
	gtk_timeout_add(100, checkForUpdateResults, NULL);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	
	return 0;
}
