/**
 * @file main.c Liferea startup
 *
 * Copyright (C) 2003-2020 Lars Windolf <lars.windolf@gmx.de>
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

#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

#include "common.h"
#include "liferea_application.h"

static void
signal_handler (int sig)
{
	liferea_application_shutdown ();
}

static void
rebuild_css (int sig)
{
	liferea_application_rebuild_css ();
}

int
main (int argc, char *argv[])
{
	signal (SIGTERM, signal_handler);
	signal (SIGINT, signal_handler);

#ifdef SIGHUP
	signal (SIGHUP, rebuild_css);
#endif

#ifdef ENABLE_NLS
	bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);
#endif

	return liferea_application_new (argc, argv);
}
