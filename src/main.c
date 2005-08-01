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

#include <sys/types.h> /* Three includes for open(2). My BSD manual says */
#include <sys/stat.h>  /* to include only <sys/file.h>. I wonder if this */
#include <fcntl.h>     /* will break any systems. */

#include <sys/types.h> /* For getpid(2) */

#include <unistd.h> /* For gethostname(), readlink(2) and symlink(2) */
#include <string.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include "interface.h"
#include "support.h"
#include "callbacks.h"
#include "feed.h"
#include "conf.h"
#include "common.h"
#include "ui_htmlview.h"
#include "update.h"
#include "debug.h"
#include "ui_queue.h"
#include "ui_mainwindow.h"
#include "metadata.h"
#include "ui_session.h"

GThread	*mainThread = NULL;
gboolean lifereaStarted = FALSE;
gboolean	startIconified = FALSE;

static void show_help(void) {
	GString	*str = g_string_new(NULL);
	
	g_string_append_c(str, '\n');
	g_string_append_printf(str, "Liferea %s\n\n", VERSION);
	g_string_append_printf(str, "%s\n", _("  --version        Print version information and exit"));
	g_string_append_printf(str, "%s\n", _("  --help           Print this help and exit"));
	g_string_append_printf(str, "%s\n", _("  --iconify        Start the program iconified"));
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
/**
 * Tries to create a lock file for Liferea.
 *
 * @returns -1 if the lock failed and is locked by someone else. -2
 * for general failures. -3 if there was a stale lockfile. Some
 * non-negative number means success.
 */

static gboolean main_lock() {
	gchar *filename, *filename2;
	gchar hostname[256];
	gint fd;
	int retval, len;
	pid_t pid;
	gchar tmp[300], *host, *pidstr;
	
	if (gethostname(hostname, 256) == -1)
		return -2; /* Skip locking if this happens, which it should not.... */
	hostname[255] = '\0';
	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "lock-%s:%d", common_get_cache_path(), hostname, getpid());
	retval = fd = open(filename, O_CREAT|O_EXCL, S_IRUSR | S_IWUSR);
	if (fd == -1) {
		g_free(filename);
		return -2;
	}
	
	filename2 = g_strdup_printf("%s" G_DIR_SEPARATOR_S "lock", common_get_cache_path());
	if (-1 == symlink(filename, filename2)) {
		if (errno == EEXIST) {
			if ((len = readlink(filename2, tmp, 299)) == -1)
				retval = -2;
			else {
				tmp[len] = '\0';
				host = tmp;
				while (*host != '\0' && *host != '-') /* Step host to point to the dash */
					host = &(host[1]);
				if (*host == '\0')
					retval = -3;
				else {
					host = &(host[1]);
					pidstr = host;
					while (*pidstr != '\0' && *pidstr != ':') /* Step pidstr to point to the colon */
						pidstr = &(pidstr[1]);
					if (*pidstr == '\0')
						retval = -3;
					else {
						*pidstr = '\0';
						pidstr = &(pidstr[1]);
						if (!strcmp(hostname, host)) {
							pid = atoi(pidstr); /* get PID */
							if (kill(pid, 0) == 0 || errno != ESRCH)
								retval = -1;
							else
								retval = -3;
						} else
							retval = -1;
					}
				}
			}
		} else 
			retval = -2;
	}

	if (retval == -3) { /* Stale lockfile */
		unlink(filename2);
		symlink(filename, filename2); /* Hopefully this will work. If not, screw it. */
	}
	close(fd);
	unlink(filename);
	g_free(filename);
	g_free(filename2);
	
	return retval;
}

static void main_unlock() {
	gchar *filename;
	
	filename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "lock", common_get_cache_path());
	unlink(filename);
	g_free(filename);
}

enum mainwindowState {
	MAINWINDOW_SHOWN,
	MAINWINDOW_ICONIFIED,
	MAINWINDOW_HIDDEN
};

int main(int argc, char *argv[]) {	
	gulong		debug_flags = 0;
	const char 	*arg;
	gint		i;
	GtkWidget	*dialog;
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

	/* parse arguments  */
	debug_flags = 0;
	for(i = 1; i < argc; ++i) {
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
			g_print("liferea %s\n", VERSION);
			return 0;
		}
		else if(!strcmp(arg, "--help") || !strcmp(arg, "-h")) {
			show_help();
			return 0;
		}
		else if(!strcmp(arg, "--iconify")) {
			mainwindowState = MAINWINDOW_ICONIFIED;
		} else if(!strncmp(arg, "--mainwindow-state=",19)) {
			gchar *param = arg + 19;
			if (g_str_equal(param, "iconified"))
				mainwindowState = MAINWINDOW_ICONIFIED;
			if (g_str_equal(param, "iconified"))
				mainwindowState = MAINWINDOW_ICONIFIED;

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

	g_set_prgname("liferea");
	gtk_set_locale();
	g_thread_init(NULL);
	gdk_threads_init();	
	gtk_init(&argc, &argv);
	mainThread = g_thread_self();	/* we need to now this when locking in ui_queue.c */

	add_pixmap_directory(PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps");

	if(main_lock() == -1) {
		dialog = gtk_message_dialog_new(GTK_WINDOW(mainwindow),
								  0,
								  GTK_MESSAGE_ERROR,
								  GTK_BUTTONS_OK,
								  _("Another copy of Liferea was found to be running. Please use it instead. "
								  "If there is no other copy of Liferea running, please delete the "
								  "\"~/.liferea/lock\" lock file."));
		gdk_threads_enter();
		gtk_dialog_run(GTK_DIALOG (dialog));
		gdk_threads_leave();
		gtk_widget_destroy(dialog);

	} else {
		ui_queue_init();		/* set up callback queue for other threads */
		
		/* order is important! */
		conf_init();			/* initialize gconf */
#ifdef USE_SM
		session_init(BIN_DIR G_DIR_SEPARATOR_S "liferea", opt_session_arg);
		session_set_cmd(NULL, startIconified);
#endif
		ui_htmlview_init();		/* setup HTML widgets */
		download_init();		/* Initialize the download subsystem */
		metadata_init();
		feed_init();			/* register feed types */
		conf_load();			/* load global feed settings */
		ui_init(startIconified);	/* setup mainwindow and initialize gconf configured GUI behaviour */
		
		gdk_threads_enter();
		lifereaStarted = TRUE;
		gtk_main();
		gdk_threads_leave();
	}

	return 0;
}

gboolean on_quit(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
	GtkWidget	*pane;
	gint x, y;

	debug_enter("on_quit");

	ui_mainwindow_save_position();
	gtk_widget_hide(mainwindow);

	ui_itemlist_clear();	/* to free all item list related data */
	conf_feedlist_save();	/* should save feedlist and folder states */

	/* should save all feeds still in memory */	
	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, feedlist_unload_feed);
	
	/* save pane proportions */
	if(NULL != (pane = lookup_widget(mainwindow, "leftpane"))) {
		x = gtk_paned_get_position(GTK_PANED(pane));
		setNumericConfValue(LAST_VPANE_POS, x);
	}
	
	if(NULL != (pane = lookup_widget(mainwindow, "rightpane"))) {
		y = gtk_paned_get_position(GTK_PANED(pane));
		setNumericConfValue(LAST_HPANE_POS, y);
	}
	
	/* save itemlist properties */
	setNumericConfValue(LAST_ZOOMLEVEL, (gint)(100.*ui_htmlview_get_zoom(ui_mainwindow_get_active_htmlview())));

	gtk_widget_destroy(mainwindow);
	ui_htmlview_deinit();
	
#ifdef USE_SM
	/* unplug */
	session_end();
#endif
	
	gtk_main_quit();
	main_unlock();
	
	debug_exit("on_quit");
	
	return FALSE;
}
