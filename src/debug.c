/*
 * Debugging output support. This was originally written for
 *
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
 *
 * Liferea specific adaptations
 * Copyright (C) 2004-2012  Lars Windolf <lars.windolf@gmx.de>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <stdarg.h>
#include <stdio.h>
#include <time.h>
#include <glib.h>

#include "debug.h"

#if defined (G_OS_WIN32) && !defined (HAVE_LOCALTIME_R)
#define localtime_r(t,o) localtime_s (o,t)
#endif

unsigned long debug_level = 0;
static GHashTable * t2d = NULL; /**< per thread call tree depth */
static GHashTable *startTimes = NULL;

static const char *
debug_get_prefix (unsigned long flag) 
{
	if (flag & DEBUG_CACHE)		return "CACHE  ";
	if (flag & DEBUG_CONF)		return "CONF   ";
	if (flag & DEBUG_UPDATE)	return "UPDATE ";
	if (flag & DEBUG_PARSING)	return "PARSING";
	if (flag & DEBUG_GUI)		return "GUI    ";
	if (flag & DEBUG_HTML)		return "HTML   ";
	if (flag & DEBUG_TRACE)		return "TRACE  ";
	if (flag & DEBUG_NET)		return "NET    ";
	if (flag & DEBUG_DB)		return "DB     ";
	if (flag & DEBUG_PERF)		return "PERF   ";
	if (flag & DEBUG_VFOLDER)	return "VFOLDER";
	return "";	
}

static void
debug_set_depth (gint newDepth)
{
	const gpointer self = g_thread_self ();

	/* Track per-thread call tree depth */
	if (t2d == NULL)
		t2d = g_hash_table_new (g_direct_hash, g_direct_equal);

	if (newDepth < 0)
		g_hash_table_insert(t2d, self, GINT_TO_POINTER(0));
	else
		g_hash_table_insert(t2d, self, GINT_TO_POINTER(newDepth));
}

static gint
debug_get_depth (void)
{
	const gpointer self = g_thread_self ();

	if (!t2d)
		return 0;

	return GPOINTER_TO_INT (g_hash_table_lookup (t2d, self));
}

void
debug_start_measurement_func (const char * function)
{
	gint64	*startTime = NULL;
	
	if (!function)
		return;
		
	if (!startTimes)
		startTimes = g_hash_table_new (g_str_hash, g_str_equal);
	
	startTime = (gint64 *) g_hash_table_lookup (startTimes, function);
	
	if (!startTime)
	{
		startTime = g_new0 (gint64, 1);
		g_hash_table_insert (startTimes, g_strdup(function), startTime);
	}

	*startTime = g_get_monotonic_time();
}

void
debug_end_measurement_func (const char * function,
                            unsigned long flags, 
			    const char *name)
{
	gint64	*startTime = NULL;
	gint64	endTime;
	unsigned long	duration = 0;
	int		i;
		
	if (!function)
		return;
		
	if (!startTimes)
		return;
	
	startTime = g_hash_table_lookup (startTimes, function);

	if (!startTime) 
		return;
		
	endTime = g_get_monotonic_time();
	duration = (endTime - *startTime) / 1000;
	if (duration < 1)
		return;

	g_print ("%s: ", debug_get_prefix (flags));
	if (debug_level & DEBUG_TRACE)
		g_print ("[%p] ", g_thread_self ());
	for (i = 0; i < debug_get_depth (); i++) 
		g_print ("   ");
	g_print ("= %s took %01ld,%03lds\n", name, 
					duration / 1000, 
					duration % 1000);

	if (duration > 250)
		debug2 (DEBUG_PERF, "function \"%s\" is slow! Took %dms.", name, duration);
}

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
	char timebuf[64];
	gchar * string;
	const gchar * prefix;
	time_t now_time_t;
	va_list args;
	struct tm now_tm;
	gint depth, i;

	g_return_if_fail (fmt != NULL);

	depth = debug_get_depth ();

	if (*fmt == '-') {
		debug_set_depth (depth - 1);
		depth--;
	}

	/* Get prefix */
	prefix = debug_get_prefix(flag);

	va_start (args, fmt);
	string = g_strdup_vprintf (fmt, args);
	va_end (args);

	time (&now_time_t);
	localtime_r (&now_time_t, &now_tm);
	strftime (timebuf, sizeof(timebuf), "%H:%M:%S", &now_tm);

	if(debug_level & DEBUG_VERBOSE) {
		printf ("(%15s:%20s)(thread %p)(time %s) %s: %s\n",
			strloc,
			function,
			g_thread_self (),
			timebuf,
			prefix,
			string);
	} else {
		g_print ("%s: ", prefix);
		if (debug_level & DEBUG_TRACE)
			g_print ("[%p] ", g_thread_self ());
		for (i = 0; i < depth; i++) 
			g_print ("   ");
		g_print ("%s\n", string);
	}
	fflush (NULL);

	g_free (string);

	if (*fmt == '+')
		debug_set_depth (depth + 1);
}

void
debug_enter (const char *name) 
{
	debug1 (DEBUG_TRACE, "+ %s", name);
	
	if (debug_level & DEBUG_PERF)
		debug_start_measurement_func (name);
}

void
debug_exit (const char *name)
{
	debug1 (DEBUG_TRACE, "- %s", name);
	
	if (debug_level & DEBUG_PERF)
		debug_end_measurement_func (name, DEBUG_PERF, name);
}
