/*
 * Debugging output support. This was originally written for
 *
 * Pan - A Newsreader for Gtk+
 * Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
 *
 * Liferea specific adaptions
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

#ifndef __DEBUG_H__
#define __DEBUG_H__

typedef enum
{
	DEBUG_CACHE		= (1<<0),
	DEBUG_CONF		= (1<<1),
	DEBUG_UPDATE		= (1<<2),
	DEBUG_PARSING		= (1<<3),
	DEBUG_GUI		= (1<<4),
	DEBUG_TRACE		= (1<<5),
	DEBUG_PLUGINS		= (1<<6),
	DEBUG_HTML		= (1<<7),
	DEBUG_NET		= (1<<8),
	DEBUG_DB		= (1<<9),
	DEBUG_PERF		= (1<<10),
	DEBUG_VERBOSE		= (1<<11)
}
DebugFlags;

/**
 * Method to save start time for a measurement.
 *
 * @param level		debugging flags that enable the measurement
 *
 * Not thread-safe!
 */
extern void debug_start_measurement_func (const char * function);

#define debug_start_measurement(level) if ((debug_level) & level) debug_start_measurement_func (PRETTY_FUNCTION)

/**
 * Method to calculate the duration for a measurement.
 * The result will be printed to the debug trace.
 *
 * @param level		debugging flags that enable the measurement
 * @param name		name of the measurement
 *
 * Not thread-safe!
 */
extern void debug_end_measurement_func (const char * function, unsigned long flags, const char *name);

#define debug_end_measurement(level, name) if ((debug_level) & level) debug_end_measurement_func (PRETTY_FUNCTION, level, name)

/**
 * Enable debugging for one or more of the given debugging flags.
 *
 * @param flags		debugging flags (see above)
 */
extern void set_debug_level (unsigned long flags);

/** currently configured debug flag set */
extern unsigned long debug_level;

/** macros for debug output */
extern void debug_printf (const char * strloc, const char * function, unsigned long level, const char* fmt, ...);

#ifdef __GNUC__
#define PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
#define PRETTY_FUNCTION ""
#endif

#define debug0(level, fmt) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt)
#define debug1(level, fmt, A) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A)
#define debug2(level, fmt, A, B) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B)
#define debug3(level, fmt, A, B, C) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B, C)
#define debug4(level, fmt, A, B, C, D) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B, C, D)
#define debug5(level, fmt, A, B, C, D, E) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B, C, D, E)
#define debug6(level, fmt, A, B, C, D, E, F) if ((debug_level) & level) debug_printf (G_STRLOC, PRETTY_FUNCTION, level,fmt, A, B, C, D, E, F)

/**
 * Trace method to trace function entering when function name
 * tracing is enabled (--debug-trace|--debug-all). Also implements
 * slow function detection when performance trace (--debug-perf)
 * is active.
 *
 * @param name		function name
 */
extern void debug_enter (const char *name);

/**
 * Trace method to trace function exiting when function name
 * tracing is enabled (--debug-trace|--debug-all). Also implements
 * slow function detection when performance trace (--debug-perf)
 * is active.
 *
 * @param name		function name
 */
extern void debug_exit (const char *name);

#endif
