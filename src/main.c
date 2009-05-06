/**
 * @file main.c Liferea main program
 *
 * Copyright (C) 2003-2008 Lars Lindner <lars.lindner@gmail.com>
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
#include <unique/unique.h>

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "conf.h"
#include "common.h"
#include "db.h"
#include "dbus.h"
#include "debug.h"
#include "itemlist.h"
#include "script.h"
#include "social.h"
#include "update.h"
#include "vfolder.h"		// FIXME: should not be necessary
#include "xml.h"
#include "ui/ui_feedlist.h"	// FIXME: should not be necessary
#include "ui/ui_session.h"
#include "ui/liferea_shell.h"
#include "sync/avahi_publisher.h"
#include "notification/notification.h"

static enum {
	STATE_STARTING,
	STATE_STARTED,
	STATE_SHUTDOWN
} runState = STATE_STARTING;

static UniqueResponse
message_received_cb (UniqueApp         *app,
		     UniqueCommand      command,
		     UniqueMessageData *message,
		     guint              time_,
		     gpointer           user_data)
{
	UniqueResponse res;

	debug1(DEBUG_GUI, "libunique command received >>>%d<<<", command);

	switch (command)
	{
		case UNIQUE_ACTIVATE:
			/* Raise the liferea window */
			debug0 (DEBUG_GUI, "-> raise window requested");
			liferea_shell_present ();
			res = UNIQUE_RESPONSE_OK;
			break;
		default:
			g_warning ("Received unknown libunique command: >>>%d<<<", command);
			res = UNIQUE_RESPONSE_OK;
			break;
	}
	return res;
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
	liferea_shutdown ();
}

static gboolean
debug_entries_parse_callback (const gchar *option_name,
			      const gchar *value,
			      gpointer data,
			      GError **error)
{
	gulong *debug_flags = data;

	if (g_str_equal (option_name, "--debug-all")) {
		*debug_flags = 0xffff - DEBUG_VERBOSE - DEBUG_TRACE;
	} else if (g_str_equal (option_name, "--debug-cache")) {
		*debug_flags |= DEBUG_CACHE;
	} else if (g_str_equal (option_name, "--debug-conf")) {
		*debug_flags |= DEBUG_CONF;
	} else if (g_str_equal (option_name, "--debug-db")) {
		*debug_flags |= DEBUG_DB;
	} else if (g_str_equal (option_name, "--debug-gui")) {
		*debug_flags |= DEBUG_GUI;
	} else if (g_str_equal (option_name, "--debug-html")) {
		*debug_flags |= DEBUG_HTML;
	} else if (g_str_equal (option_name, "--debug-net")) {
		*debug_flags |= DEBUG_NET;
	} else if (g_str_equal (option_name, "--debug-parsing")) {
		*debug_flags |= DEBUG_PARSING;
	} else if (g_str_equal (option_name, "--debug-performance")) {
		*debug_flags |= DEBUG_PERF;
	} else if (g_str_equal (option_name, "--debug-trace")) {
		*debug_flags |= DEBUG_TRACE;
	} else if (g_str_equal (option_name, "--debug-update")) {
		*debug_flags |= DEBUG_UPDATE;
	} else if (g_str_equal (option_name, "--debug-verbose")) {
		*debug_flags |= DEBUG_VERBOSE;
	} else {
		return FALSE;
	}

	return TRUE;
}

static gboolean
show_version (const gchar *option_name,
	      const gchar *value,
	      gpointer data,
	      GError **error)
{
	printf ("Liferea %s\n", VERSION);
	exit (0);
}

int
main (int argc, char *argv[])
{
	UniqueApp	*app;
	GError		*error = NULL;
	GOptionContext	*context;
	GOptionGroup	*debug;
	gulong		debug_flags = 0;
	LifereaDBus	*dbus = NULL;
	gchar		*initial_state = "shown";
	int		initialState;

#ifdef USE_SM
	gchar *opt_session_arg = NULL;
#endif

	GOptionEntry entries[] = {
		{ "mainwindow-state", 'w', 0, G_OPTION_ARG_STRING, &initial_state, N_("Start Liferea with its main window in STATE. STATE may be `shown', `iconified', or `hidden'"), N_("STATE") },
#ifdef USE_SM
		{ "session", 0, G_OPTION_FLAG_HIDDEN, G_OPTION_ARG_STRING, &opt_session_arg, NULL, NULL },
#endif
		{ "version", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, show_version, N_("Show version information and exit"), NULL },
		{ NULL }
	};

	GOptionEntry debug_entries[] = {
		{ "debug-all", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all types"), NULL },
		{ "debug-cache", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages for the cache handling"), NULL },
		{ "debug-conf", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of the configuration handling"), NULL },
		{ "debug-db", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of the database handling"), NULL },
		{ "debug-gui", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all GUI functions"), NULL },
		{ "debug-html", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Enables HTML rendering debugging. Each time Liferea renders HTML output it will also dump the generated HTML into ~/.liferea_1.6/output.xhtml"), NULL },
		{ "debug-net", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all network activity"), NULL },
		{ "debug-parsing", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all parsing functions"), NULL },
		{ "debug-performance", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages when a function takes too long to process"), NULL },
		{ "debug-trace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages when entering/leaving functions"), NULL },
		{ "debug-update", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of the feed update processing"), NULL },
		{ "debug-verbose", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print verbose debugging messages"), NULL },
		{ NULL }
	};

	if (!g_thread_supported ()) g_thread_init (NULL);

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	debug = g_option_group_new ("debug",
				    N_("Print debugging messages for the given topic"),
				    N_("Print debugging messages for the given topic"),
				    &debug_flags,
				    NULL);
	g_option_group_add_entries (debug, debug_entries);

	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, N_("Liferea, the Linux Feed Reader"));
	g_option_context_set_description (context, N_("For more information, please visit http://liferea.sourceforge.net/"));
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_group (context, debug);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));

	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);
	if (error) {
		g_print ("Error parsing options: %s\n", error->message);
	}

	set_debug_level (debug_flags);

	/* Configuration necessary for network options, so it
	   has to be initialized before update_init() */
	conf_init ();

#ifdef USE_DBUS
	dbus_g_thread_init ();
#endif

	/* We need to do the network initialization here to allow
	   network-manager to be setup before gtk_init() */
	update_init ();

	gtk_init (&argc, &argv);

	/* Single instance checks */
	app = unique_app_new ("net.sourceforge.liferea", NULL);
	if (unique_app_is_running (app)) {
		g_print ("Liferea is already running\n");
		unique_app_send_message (app, UNIQUE_ACTIVATE, NULL);
		return 1;
	} else {
		g_signal_connect (app, "message-received", G_CALLBACK (message_received_cb), NULL);
	}

	/* GTK theme support */
	g_set_application_name (_("Liferea"));
	gtk_window_set_default_icon_name ("liferea");

	debug_start_measurement (DEBUG_DB);

	/* order is important! */
	db_init ();			/* initialize sqlite */
	xml_init ();			/* initialize libxml2 */
	conf_load ();			/* load global feed settings */
	script_init ();			/* setup scripting if supported */
#ifdef HAVE_LUA
	script_add_impl(&lua_script_impl);
#endif
#ifdef HAVE_LIBNOTIFY
	notification_plugin_register (&libnotify_plugin);
#endif
	social_init ();			/* initialized social bookmarking */
#ifdef USE_DBUS	
	dbus = liferea_dbus_new ();	
#else
	debug0 (DEBUG_GUI, "Compiled without DBUS support.");
#endif

#ifdef USE_AVAHI
	if (conf_get_bool_value (SYNC_AVAHI_ENABLED)) {
		LifereaAvahiPublisher	*avahiPublisher = NULL;

		debug0 (DEBUG_CACHE, "Registering with AVAHI");
		avahiPublisher = liferea_avahi_publisher_new ();
		liferea_avahi_publisher_publish (avahiPublisher, conf_get_str_value (SYNC_AVAHI_SERVICE_NAME), 23632);
	} else {
		debug0 (DEBUG_CACHE, "Avahi support available, but disabled by preferences.");
	}
#else
	debug0 (DEBUG_CACHE, "Compiled without AVAHI support");
#endif

	/* how to start liferea, command line takes precedence over preferences */
	if (g_str_equal(initial_state, "iconified")) {
		initialState = MAINWINDOW_ICONIFIED;
	} else if (g_str_equal(initial_state, "hidden") ||
	    (conf_get_bool_value (SHOW_TRAY_ICON) &&
	     conf_get_bool_value (START_IN_TRAY))) {
		initialState = MAINWINDOW_HIDDEN;
	} else {
		initialState = MAINWINDOW_SHOWN;
	}

	liferea_shell_create (initialState);
	g_set_prgname ("liferea");
	
	script_run_for_hook (SCRIPT_HOOK_STARTUP);
	
#ifdef USE_SM
	/* This must be after feedlist reading because some session
	   managers will tell Liferea to exit if Liferea does not
	   respond to SM requests within a minute or two. This starts
	   the main loop soon after opening the SM connection. */
	session_init (BIN_DIR G_DIR_SEPARATOR_S "liferea", opt_session_arg);
	session_set_cmd (NULL, initialState);
#endif
	signal (SIGTERM, signal_handler);
	signal (SIGINT, signal_handler);
	signal (SIGHUP, signal_handler);

#ifndef G_OS_WIN32
	signal (SIGBUS, fatal_signal_handler);
	signal (SIGSEGV, fatal_signal_handler);
#endif

	/* Note: we explicitely do not use the gdk_thread_*
	   locking in Liferea because it freezes the program
	   when running Flash applets in gtkmozembed */

	runState = STATE_STARTING;
	
	debug_end_measurement (DEBUG_DB, "startup");
	
	gtk_main ();
	
	g_object_unref (G_OBJECT (dbus));
	return 0;
}

static gboolean
on_shutdown (gpointer user_data)
{
	debug_enter ("liferea_shutdown");

	/* prevents signal handler from calling us a second time */
	if (runState == STATE_SHUTDOWN)
		return FALSE;
		
	runState = STATE_SHUTDOWN;

	/* order is important ! */
		
	script_run_for_hook (SCRIPT_HOOK_SHUTDOWN);
	
	itemlist_free ();
	update_deinit ();
	db_deinit ();
	script_deinit ();
	social_free ();

	liferea_shell_destroy ();
#ifdef USE_SM
	/* unplug */
	session_end ();
#endif
	conf_deinit ();
	
	gtk_main_quit ();
	
	debug_exit ("liferea_shutdown");
	return FALSE;
}

void
liferea_shutdown (void)
{
	g_idle_add (on_shutdown, NULL);
}
