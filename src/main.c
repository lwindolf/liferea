/**
 * @file main.c Liferea startup
 *
 * Copyright (C) 2003-2012 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "common.h"
#include "liferea_application.h"

static LifereaApplication *liferea_app = NULL;

gboolean
liferea_shutdown_source_func (gpointer userdata)
{
	g_application_quit (G_APPLICATION (liferea_app));
	return FALSE;
}

void liferea_shutdown ()
{
	g_idle_add (liferea_shutdown_source_func, NULL);
}

static void
signal_handler (int sig)
{
	liferea_shutdown ();
}

int
main (int argc, char *argv[])
{
	gint status;

	signal (SIGTERM, signal_handler);
	signal (SIGINT, signal_handler);
#ifdef SIGHUP
	signal (SIGHUP, signal_handler);
#endif

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	liferea_app = liferea_application_new ();
	g_set_prgname ("liferea");
	g_set_application_name (_("Liferea"));
	gtk_window_set_default_icon_name ("liferea");	/* GTK theme support */

	status = g_application_run (G_APPLICATION (liferea_app), argc, argv);
	g_object_unref (liferea_app);

	return status;
}
