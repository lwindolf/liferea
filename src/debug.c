/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Debugging output support. This was originally written for
 *
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
 *
 * Liferea specific adaptions
 * Copyright (C) 2004  Lars Lindner <lars.lindner@gmx.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
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

#include <config.h>

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <glib.h>

#include "debug.h"

unsigned long debug_level = 0;

static int depth = 0;
 
void
set_debug_level (unsigned long level)
{
	debug_level = level;
}

void
debug_printf (const char    * strloc,
              const gchar   * function,
              gulong          flag,
              const gchar   * fmt,
              ...)
{
	static GHashTable * t2d = NULL;
	char timebuf[64];
	gchar * string;
	const gchar * prefix;
	time_t now_time_t;
	va_list args;
	struct tm now_tm;
	const gpointer self = g_thread_self ();

	if (t2d == NULL)
		t2d = g_hash_table_new (g_direct_hash, g_direct_equal);

	g_return_if_fail (fmt != NULL);

	/* get prefix */
	if (flag&DEBUG_CACHE) prefix="CACHE";
	else if (flag&DEBUG_CONF) prefix="CONF";
	else if (flag&DEBUG_UPDATE) prefix="UPDATE";
	else if (flag&DEBUG_PARSING) prefix="PARSING";
	else if (flag&DEBUG_GUI) prefix="GUI";
	else if (flag&DEBUG_TRACE) prefix="TRACE";
	else {prefix="";}

	va_start (args, fmt);
	string = g_strdup_vprintf (fmt, args);
	va_end (args);

	time (&now_time_t);
	localtime_r (&now_time_t, &now_tm);
	strftime (timebuf, sizeof(timebuf), "%H:%M:%S", &now_tm);

	if (flag&DEBUG_TRACE)
	{
		const gint old_depth = GPOINTER_TO_INT (g_hash_table_lookup (t2d, self));
		const gint new_depth = old_depth + (*fmt=='+' ? 1 : -1);

		g_hash_table_insert (t2d, 
		                     self,
		                     GINT_TO_POINTER(new_depth));

		if(flag&DEBUG_VERBOSE)
			printf ("(%15s:%20s)(thread %p)(time %s)(depth %3d) %s: %s\n",
				strloc,
				function,
				self,
				timebuf,
				new_depth,
				prefix,
				string);
		else
			printf ("%s: %s\n", prefix, string);


		if (*fmt=='-')
			--depth;
	}
	else
	{
		if(flag&DEBUG_VERBOSE)
			printf ("(%15s:%20s)(thread %p)(time %s) %s: %s\n",
				strloc,
				function,
				self,
				timebuf,
				prefix,
				string);
		else
			printf ("%s: %s\n", prefix, string);
			
	}
	fflush (NULL);

	g_free (string);
}

