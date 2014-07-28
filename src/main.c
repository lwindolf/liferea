/**
 * @file main.c Liferea startup
 *
 * Copyright (C) 2003-2012 Lars Windolf <lars.windolf@gmx.de>
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

#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <girepository.h>

#include "conf.h"
#include "common.h"
#include "db.h"
#include "dbus.h"
#include "debug.h"
#include "feedlist.h"
#include "social.h"
#include "update.h"
#include "xml.h"
#include "ui/liferea_shell.h"

static enum {
	STATE_STARTING,
	STATE_STARTED,
	STATE_SHUTDOWN
} runState = STATE_STARTING;

static const gchar *initialStateOption = NULL;

/* GApplication "open" callback for receiving feed-add requests from remote */
static void
on_feed_add (GApplication *application,
             gpointer      files,
             gint          n_files,
             gchar        *hint,
             gpointer      user_data)
{
	GFile **uris = (GFile **)files;	/* we always expect only one URI */

	g_assert(uris);
	g_assert(n_files == 1);
	feedlist_add_subscription (g_file_get_uri (uris[0]), NULL, NULL, 0);
}

/* GApplication "activate" callback for normal startup and also processing
   local feed-add requests (URI passed in user_data). */
static void
on_app_activate (GtkApplication *app, gpointer user_data)
{
	GList		*list;
	gchar		*feedUri = (gchar *)user_data;

	list = gtk_application_get_windows (app);

	if (list) {
		gtk_window_present (GTK_WINDOW (list->data));
	} else {
		liferea_shell_create (app, initialStateOption);

		if (feedUri)
			feedlist_add_subscription (feedUri, NULL, NULL, 0);
	}
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
	} else if (g_str_equal (option_name, "--debug-vfolder")) {
		*debug_flags |= DEBUG_VFOLDER;
	} else if (g_str_equal (option_name, "--debug-verbose")) {
		*debug_flags |= DEBUG_VERBOSE;
	} else {
		return FALSE;
	}

	return TRUE;
}

static gboolean G_GNUC_NORETURN
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
	GtkApplication	*app;
	GError		*error = NULL;
	GOptionContext	*context;
	GOptionGroup	*debug;
	gulong		debug_flags = 0;
	LifereaDBus	*dbus = NULL;
	gchar		*feedUri = NULL;
	gint 		status;

	GOptionEntry entries[] = {
		{ "mainwindow-state", 'w', 0, G_OPTION_ARG_STRING, &initialStateOption, N_("Start Liferea with its main window in STATE. STATE may be `shown' or `hidden'"), N_("STATE") },
		{ "version", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, show_version, N_("Show version information and exit"), NULL },
		{ "add-feed", 'a', 0, G_OPTION_ARG_STRING, &feedUri, N_("Add a new subscription"), N_("uri") },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

	GOptionEntry debug_entries[] = {
		{ "debug-all", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all types"), NULL },
		{ "debug-cache", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages for the cache handling"), NULL },
		{ "debug-conf", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages for the configuration handling"), NULL },
		{ "debug-db", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of the database handling"), NULL },
		{ "debug-gui", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all GUI functions"), NULL },
		{ "debug-html", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Enables HTML rendering debugging. Each time Liferea renders HTML output it will also dump the generated HTML into ~/.cache/liferea/output.xhtml"), NULL },
		{ "debug-net", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all network activity"), NULL },
		{ "debug-parsing", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of all parsing functions"), NULL },
		{ "debug-performance", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages when a function takes too long to process"), NULL },
		{ "debug-trace", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages when entering/leaving functions"), NULL },
		{ "debug-update", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of the feed update processing"), NULL },
		{ "debug-vfolder", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print debugging messages of the search folder matching"), NULL },
		{ "debug-verbose", 0, G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK, debug_entries_parse_callback, N_("Print verbose debugging messages"), NULL },
		{ NULL, 0, 0, 0, NULL, NULL, NULL }
	};

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	debug = g_option_group_new ("debug",
				    _("Print debugging messages for the given topic"),
				    _("Print debugging messages for the given topic"),
				    &debug_flags,
				    NULL);
	g_option_group_set_translation_domain(debug, GETTEXT_PACKAGE);
	g_option_group_add_entries (debug, debug_entries);

	context = g_option_context_new (NULL);
	g_option_context_set_summary (context, N_("Liferea, the Linux Feed Reader"));
	g_option_context_set_description (context, N_("For more information, please visit http://lzone.de/liferea/"));
	g_option_context_add_main_entries (context, entries, GETTEXT_PACKAGE);
	g_option_context_set_translation_domain(context, GETTEXT_PACKAGE);
	g_option_context_add_group (context, debug);
	g_option_context_add_group (context, gtk_get_option_group (FALSE));
	g_option_context_add_group (context, g_irepository_get_option_group ());

	g_option_context_parse (context, &argc, &argv, &error);
	g_option_context_free (context);
	if (error) {
		g_print ("Error parsing options: %s\n", error->message);
	}

	set_debug_level (debug_flags);

	/* Configuration necessary for network options, so it
	   has to be initialized before update_init() */
	conf_init ();

	/* We need to do the network initialization here to allow
	   network-manager to be setup before gtk_init() */
	update_init ();

	gtk_init (&argc, &argv);

	/* Single instance checks, also note that we pass or only RPC (add-feed)
	   as activate signal payload as it is simply an URI string. */
	app = gtk_application_new ("net.sourceforge.liferea", G_APPLICATION_HANDLES_OPEN);
	g_signal_connect (app, "activate", G_CALLBACK (on_app_activate), feedUri);
	g_signal_connect (app, "open", G_CALLBACK (on_feed_add), NULL);

	g_set_prgname ("liferea");
	g_set_application_name (_("Liferea"));
	gtk_window_set_default_icon_name ("liferea");	/* GTK theme support */

	debug_start_measurement (DEBUG_DB);

	/* order is important! */
	db_init ();			/* initialize sqlite */
	xml_init ();			/* initialize libxml2 */
	social_init ();			/* initialize social bookmarking */

	dbus = liferea_dbus_new ();

	signal (SIGTERM, signal_handler);
	signal (SIGINT, signal_handler);
	signal (SIGHUP, signal_handler);

	/* Note: we explicitely do not use the gdk_thread_*
	   locking in Liferea because it freezes the program
	   when running Flash applets */

	runState = STATE_STARTING;
	
	debug_end_measurement (DEBUG_DB, "startup");

	status = g_application_run (G_APPLICATION (app), 0, NULL);

	/* Trigger RPCs if we are not primary instance (currently only feed-add) */
	if (feedUri && g_application_get_is_remote (G_APPLICATION (app))) {
		GFile *uris[2];

		uris[0] = g_file_new_for_uri (feedUri);
		uris[1] = NULL;
		g_application_open (G_APPLICATION (app), uris, 1, "feed-add");
		g_object_unref (uris[0]);
	}
	
	g_object_unref (G_OBJECT (dbus));
	g_object_unref (app);

	return status;
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
	update_deinit ();

	liferea_shell_destroy ();

	db_deinit ();
	social_free ();
	conf_deinit ();
	
	debug_exit ("liferea_shutdown");
	return FALSE;
}

void
liferea_shutdown (void)
{
	g_idle_add (on_shutdown, NULL);
}
