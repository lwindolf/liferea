/**
 * @file main.c Liferea main program
 *
 * Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>
 * 
 * Some code like the command line handling was inspired by 
 *
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
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
#include "htmlview.h"
#include "update.h"
#include "debug.h"
#include "ui_queue.h"
#include "ui_mainwindow.h"

GThread	*mainThread = NULL;

static void show_help(void) {
	GString	*str = g_string_new(NULL);
	
	g_string_append_c(str, '\n');
	g_string_append_printf(str, "Liferea %s\n\n", VERSION);
	g_string_append_printf(str, "%s\n", _("  --version        Prints Liferea's version number"));
	g_string_append_printf(str, "%s\n", _("  --help           Prints this message and exits"));
	g_string_append_printf(str, "%s\n", _("  --iconify        Starts the program iconified"));
	g_string_append_c(str, '\n');
	g_string_append_printf(str, "%s\n", _("  --debug-cache    Print debugging messages for the cache handling"));
	g_string_append_printf(str, "%s\n", _("  --debug-conf     Print debugging messages of the configuration handling"));
	g_string_append_printf(str, "%s\n", _("  --debug-update   Print debugging messages of the feed update processing"));
	g_string_append_printf(str, "%s\n", _("  --debug-parsing  Print debugging messages of all parsing functions"));
	g_string_append_printf(str, "%s\n", _("  --debug-gui      Print debugging messages of all GUI functions"));
	g_string_append_printf(str, "%s\n", _("  --debug-trace    Print debugging messages when entering/leaving functions"));
	g_string_append_printf(str, "%s\n", _("  --debug-all      Print debugging messages of all types"));
	g_string_append_printf(str, "%s\n", _("  --debug-verbose  Print verbose debugging messages"));

	g_string_append_c(str, '\n');
	g_print("%s", str->str);
	g_string_free(str, TRUE);
}

int main(int argc, char *argv[]) {	
	gulong		debug_flags = 0;
	gboolean	start_iconified = FALSE;
	const char 	*arg;
	gint		i;
	
#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	setlocale(LC_ALL, "");
#endif

	/* parse arguments  */
	debug_flags = 0;
	for(i = 0; i < argc; ++i) {		
		arg = argv[i];

		if(!strcmp(arg, "--debug-cache"))
			debug_flags |= DEBUG_CACHE;
		else if(!strcmp(arg, "--debug-conf"))
			debug_flags |= DEBUG_CONF;
		else if(!strcmp(arg, "--debug-update"))
			debug_flags |= DEBUG_UPDATE;
		else if(!strcmp(arg, "--debug-parsing"))
			debug_flags |= DEBUG_PARSING;
		else if(!strcmp(arg, "--debug-gui"))
			debug_flags |= DEBUG_GUI;
		else if(!strcmp(arg, "--debug-trace"))
			debug_flags |= DEBUG_TRACE;
		else if(!strcmp(arg, "--debug-all"))
			debug_flags |= DEBUG_TRACE|DEBUG_CACHE|DEBUG_CONF|DEBUG_UPDATE|DEBUG_PARSING|DEBUG_GUI;
		else if(!strcmp(arg, "--debug-verbose"))
			debug_flags |= DEBUG_VERBOSE;		
		else if(!strcmp(arg, "--version") || !strcmp(arg, "-v")) {
			g_print("Liferea %s\n", VERSION);
			return 0;
		}
		else if(!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
			show_help();
			return 0;
		}
		else if(!strcmp(arg, "--iconify")) {
			start_iconified = TRUE;
		}
	}
	set_debug_level(debug_flags);

	g_set_prgname("liferea");
	gtk_set_locale();
	g_thread_init(NULL);
	gdk_threads_init();	
	gtk_init(&argc, &argv);
	mainThread = g_thread_self();	/* we need to now this when locking in ui_queue.c */

	add_pixmap_directory(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps");

	ui_queue_init();		/* set up callback queue for other threads */

	/* order is important! */
	initConfig();			/* initialize gconf */
	ui_htmlview_init();		/* setup HTML widgets */
	download_init();		/* Initialize the download subsystem */
	mainwindow = ui_mainwindow_new();
	loadConfig();			/* maybe this should be merged with initConfig() */
	feed_init();			/* register feed types */
	ui_init();			/* initialize gconf configured GUI behaviour */

	gtk_widget_show(mainwindow);
	ui_mainwindow_finish(mainwindow); /* Ugly hack to make mozilla work */
	
	if(start_iconified)
		gtk_widget_hide(mainwindow);
	
	if(getBooleanConfValue(UPDATE_ON_STARTUP))
		ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (nodeActionFunc)feed_schedule_update);

	gdk_threads_enter();
	gtk_main();
	gdk_threads_leave();
	
	return 0;
}
