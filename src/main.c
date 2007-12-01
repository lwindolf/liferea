/**
 * @file main.c Liferea main program
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "conf.h"
#include "common.h"
#include "db.h"
#include "dbus.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "script.h"
#include "social.h"
#include "update.h"
#include "vfolder.h"
#include "xml.h"
#include "ui/ui_feedlist.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_session.h"
#include "ui/ui_shell.h"

#include "bacon-message-connection.h"

static BaconMessageConnection *bacon_connection = NULL;

gboolean lifereaStarted = FALSE;

gboolean on_quit(GtkWidget *widget, GdkEvent *event, gpointer user_data);

static void show_help(void) {
	GString	*str = g_string_new(NULL);
	
	g_string_append_c(str, '\n');
	g_string_append_printf(str, "Liferea %s\n\n", VERSION);
	g_string_append_printf(str, "%s\n", _("  --version        Print version information and exit"));
	g_string_append_printf(str, "%s\n", _("  --help           Print this help and exit"));
	g_string_append_printf(str, "%s\n", _("  --mainwindow-state=STATE"));
	g_string_append_printf(str, "%s\n", _("                   Start Liferea with its main window in STATE."));
	g_string_append_printf(str, "%s\n", _("                   STATE may be `shown', `iconified', or `hidden'"));
	g_string_append_c(str, '\n');
	g_string_append_printf(str, "%s\n", _("  --debug-<topic>  Print debugging messages for the given topic"));
	g_string_append_printf(str, "%s\n", _("                   Possible topics are: all,cache,conf,db,gui,html"));
	g_string_append_printf(str, "%s\n", _("                   net,parsing,plugins,trace,update,verbose"));
	g_string_append_c(str, '\n');
	g_print("%s", str->str);
	g_string_free(str, TRUE);
}

/* bacon message callback */
static void on_bacon_message_received(const char *message, gpointer data) {

	debug1(DEBUG_GUI, "bacon message received >>>%s<<<", message);
	
	/* Currently we only know a single simple command "raise"
	   which tells the program to raise the window because
	   another instance was requested which is not supported */
	   
	if(g_str_equal(message, "raise")) {
		debug0(DEBUG_GUI, "-> raise window requested");
		gtk_window_present(GTK_WINDOW(mainwindow));
	} else {
		g_warning("Received unknown bacon command: >>>%s<<<", message);
	}
}

static void fatal_signal_handler(int sig) {
	sigset_t sigset;

	sigemptyset(&sigset);
	sigprocmask(SIG_SETMASK, &sigset, NULL);

	g_print("\nLiferea did receive signal %d (%s).\n", sig, g_strsignal(sig));

	if(debug_level) {
		g_print("You have propably triggered a program bug. I will now try to \n");
		g_print("create a backtrace which you can attach to any support requests.\n\n");
		g_on_error_stack_trace(PACKAGE);
	}

	_exit(1);
}

static void
signal_handler (int sig)
{
	g_idle_add (quit, NULL);
}

int main(int argc, char *argv[]) {	
	gulong		debug_flags = 0;
	const char 	*arg;
	gint		i;
	LifereaDBus	*dbus = NULL;
	int mainwindowState = MAINWINDOW_SHOWN;
	
#ifdef USE_SM
	gchar *opt_session_arg = NULL;
#endif

#ifdef ENABLE_NLS
	bindtextdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
	textdomain(GETTEXT_PACKAGE);
	setlocale(LC_ALL, "");
#endif
	/* Do not set program name here as it would be overwritten by Gecko! */
	
	gtk_set_locale();
	g_thread_init(NULL);
#ifdef USE_DBUS
	dbus_g_thread_init();
#endif
#ifdef USE_NM
	update_nm_initialize();
#endif
	gtk_init(&argc, &argv);
	
	/* GTK theme support */
	g_set_application_name(_("Liferea"));
	gtk_window_set_default_icon_name("liferea");
	
	/* parse arguments  */
	debug_flags = 0;
	for(i = 1; i < argc; ++i) {
		arg = argv[i];
		
		if (!strcmp (arg, "--debug-cache"))
			debug_flags |= DEBUG_CACHE;
		else if (!strcmp (arg, "--debug-conf"))
			debug_flags |= DEBUG_CONF;
		else if (!strcmp (arg, "--debug-update"))
			debug_flags |= DEBUG_UPDATE;
		else if (!strcmp (arg, "--debug-parsing"))
			debug_flags |= DEBUG_PARSING;
		else if (!strcmp (arg, "--debug-gui"))
			debug_flags |= DEBUG_GUI;
		else if (!strcmp (arg, "--debug-html"))
			debug_flags |= DEBUG_HTML;
		else if (!strcmp (arg, "--debug-plugins"))
			debug_flags |= DEBUG_PLUGINS;
		else if (!strcmp (arg, "--debug-net"))
			debug_flags |= DEBUG_NET;
		else if (!strcmp (arg, "--debug-db"))
			debug_flags |= DEBUG_DB;
		else if (!strcmp (arg, "--debug-perf"))
			debug_flags |= DEBUG_PERF;
		else if (!strcmp (arg, "--debug-trace"))
			debug_flags |= DEBUG_TRACE;
		else if (!strcmp (arg, "--debug-all"))
			debug_flags |= DEBUG_TRACE|DEBUG_CACHE|DEBUG_CONF|DEBUG_UPDATE|DEBUG_PARSING|DEBUG_GUI|DEBUG_PLUGINS|DEBUG_NET|DEBUG_DB;
		else if (!strcmp (arg, "--debug-verbose"))
			debug_flags |= DEBUG_VERBOSE;
		else if (!strcmp (arg, "--version") || !strcmp (arg, "-v")) {
			g_print ("liferea %s\n", VERSION);
			return 0;
		}
		else if(!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
			show_help();
			return 0;
		}
		else if(!strcmp(arg, "--iconify")) {
			mainwindowState = MAINWINDOW_ICONIFIED;
		} else if(!strncmp(arg, "--mainwindow-state=",19)) {
			const gchar *param = arg + 19;
			if (g_str_equal(param, "iconified"))
				mainwindowState = MAINWINDOW_ICONIFIED;
			else if (g_str_equal(param, "hidden"))
				mainwindowState = MAINWINDOW_HIDDEN;
			else if (g_str_equal(param, "shown"))
				mainwindowState = MAINWINDOW_SHOWN;
			else
				fprintf(stderr, _("The --mainwindow-state argument must be given a parameter.\n"));
#ifdef USE_SM
		}
		else if (!strcmp(arg, "--session")) {
			i++;
			if (i < argc) {
				opt_session_arg = g_strdup(argv[i]);
			} else
				fprintf(stderr, _("The --session argument must be given a parameter.\n"));
#endif
		} else {
			fprintf(stderr, _("Liferea encountered an unknown argument: %s\n"), arg);
		}
	}
	set_debug_level(debug_flags);

	/* Note: bacon connection check needs to be done after the
	   command line parameter checking to allow help and version
	   switches to be used when we are already running */
	bacon_connection = bacon_message_connection_new("liferea");
	if(bacon_connection)	{
		if(!bacon_message_connection_get_is_server(bacon_connection)) {
			g_warning(_("Liferea seems to be running already!"));
			
		  	debug0(DEBUG_VERBOSE, "Startup as bacon client...");
			bacon_message_connection_send(bacon_connection, "raise");
			bacon_message_connection_free(bacon_connection);
			
			gdk_notify_startup_complete();
			exit(0);
		} else {
		  	debug0(DEBUG_VERBOSE, "Startup as bacon server...");
			bacon_message_connection_set_callback(bacon_connection,
							      on_bacon_message_received,
							      NULL);
		}
	} else {
		g_warning("Cannot create IPC connection for Liferea!");
	}
		
	debug_start_measurement (DEBUG_DB);

	add_pixmap_directory(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps");

	/* order is important! */
	rule_init ();
	db_init (TRUE);			/* initialize sqlite */
	xml_init ();			/* initialize libxml2 */
	conf_init ();			/* initialize gconf */
	update_init ();			/* initialize the download subsystem */
	plugin_mgmt_init ();		/* get list of plugins and initialize them */
	feed_init ();			/* register feed types */
	conf_load ();			/* load global feed settings */
	script_init ();			/* setup scripting if supported */
	social_init ();			/* initialized social bookmarking */
#ifdef USE_DBUS	
	dbus = liferea_dbus_new ();	
#else
	debug0(DEBUG_GUI, "Compiled without DBUS support.");
#endif
	ui_mainwindow_init(mainwindowState);	/* setup mainwindow and initialize gconf configured GUI behaviour */
	g_set_prgname("liferea");
#ifdef USE_SM
	/* This must be after feedlist reading because some session
	   managers will tell Liferea to exit if Liferea does not
	   respond to SM requests within a minute or two. This starts
	   the main loop soon after opening the SM connection. */
	session_init(BIN_DIR G_DIR_SEPARATOR_S "liferea", opt_session_arg);
	session_set_cmd(NULL, mainwindowState);
#endif
	signal(SIGTERM, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGHUP, signal_handler);

#ifndef G_OS_WIN32
	signal(SIGBUS, fatal_signal_handler);
	signal(SIGSEGV, fatal_signal_handler);
#endif

	/* Note: we explicitely do not use the gdk_thread_*
	   locking in Liferea because it freezes the program
	   when running Flash applets in gtkmozembed */

	lifereaStarted = TRUE;
	
	debug_end_measurement (DEBUG_DB, "startup");
	
	gtk_main();
	
	g_object_unref (G_OBJECT (dbus));
	bacon_message_connection_free (bacon_connection);
	return 0;
}

gboolean
on_quit (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
	debug_enter ("on_quit");

	/* order is important ! */
		
	script_run_for_hook (SCRIPT_HOOK_SHUTDOWN);
	
	ui_mainwindow_save_position ();
	gtk_widget_hide (mainwindow);

	ui_feedlist_select (NULL);
	feedlist_save ();
	feedlist_free ();
	itemlist_free ();
	update_deinit ();
	db_deinit ();
	script_deinit ();
	social_free ();

	gtk_widget_destroy (mainwindow);
#ifdef USE_SM
	/* unplug */
	session_end ();
#endif
	conf_deinit ();
	
	gtk_main_quit ();
	
	debug_exit ("on_quit");
	return FALSE;
}

gboolean
quit (gpointer user_data)
{
	on_quit (NULL, NULL, NULL);
	return FALSE;
}
