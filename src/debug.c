/*
 * Debugging output support. This was originally written for
 *
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
 *
 * Liferea specific adaptations
 * Copyright (C) 2004-2007  Lars Lindner <lars.lindner@gmail.com>
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

unsigned long debug_level = 0;

static int depth = 0;

static GHashTable *startTimes = NULL;

static const char *
debug_get_prefix (unsigned long flag) 
{
	if (flag & DEBUG_CACHE)		return "CACHE";
	if (flag & DEBUG_CONF)		return "CONF";
	if (flag & DEBUG_UPDATE)	return "UPDATE";
	if (flag & DEBUG_PARSING)	return "PARSING";
	if (flag & DEBUG_GUI)		return "GUI";
	if (flag & DEBUG_HTML)		return "HTML";
	if (flag & DEBUG_PLUGINS)	return "PLUGINS";
	if (flag & DEBUG_TRACE)		return "TRACE";
	if (flag & DEBUG_NET)		return "NET";
	if (flag & DEBUG_DB)		return "DB";
	if (flag & DEBUG_PERF)		return "PERF";
	return "";	
}

void
debug_start_measurement_func (const char * function)
{
	GTimeVal	*startTime = NULL;
	
	if (!function)
		return;
		
	if (!startTimes)
		startTimes = g_hash_table_new (g_str_hash, g_str_equal);
	
	startTime = (GTimeVal *) g_hash_table_lookup (startTimes, function);
	
	if (!startTime)
	{
		startTime = g_new0 (GTimeVal, 1);
		g_hash_table_insert (startTimes, g_strdup(function), startTime);
	}

	g_get_current_time (startTime);
}

void
debug_end_measurement_func (const char * function,
                            unsigned long flags, 
			    const char *name)
{
	GTimeVal	*startTime = NULL;
	GTimeVal	endTime;
	unsigned long	duration = 0;
		
	if (!function)
		return;
		
	if (!startTimes)
		return;
	
	startTime = g_hash_table_lookup (startTimes, function);

	if (!startTime) 
		return;
		
	g_get_current_time (&endTime);
	g_time_val_add (&endTime, (-1) * startTime->tv_usec);
	
	if ((0 == endTime.tv_sec - startTime->tv_sec) &&
	    (0 == endTime.tv_usec/1000))
		return;
	
	g_print ("%s: %s took %01ld,%03lds\n", debug_get_prefix (flags), name, 
	                                     endTime.tv_sec - startTime->tv_sec, 
					     endTime.tv_usec/1000);
					     
	duration = (endTime.tv_sec - startTime->tv_sec)*1000 + endTime.tv_usec/1000;
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
	prefix = debug_get_prefix(flag);

	va_start (args, fmt);
	string = g_strdup_vprintf (fmt, args);
	va_end (args);

	time (&now_time_t);
	localtime_r (&now_time_t, &now_tm);
	strftime (timebuf, sizeof(timebuf), "%H:%M:%S", &now_tm);

	if(flag & DEBUG_TRACE) {
		const gint old_depth = GPOINTER_TO_INT (g_hash_table_lookup (t2d, self));
		const gint new_depth = old_depth + (*fmt=='+' ? 1 : -1);

		g_hash_table_insert(t2d, 
		                    self,
		                    GINT_TO_POINTER(new_depth));

		if(debug_level & DEBUG_VERBOSE)
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


		if(*fmt=='-')
			--depth;
	} else {
		if(debug_level & DEBUG_VERBOSE)
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
