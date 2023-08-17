/*
 * Debugging output support.
 *
 * Copyright (C) 2023  Lars Windolf <lars.windolf@gmx.de>
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

#include <stdarg.h>
#include <stdio.h>
#include <glib.h>

#include "debug.h"

#if defined (G_OS_WIN32) && !defined (HAVE_LOCALTIME_R)
#define localtime_r(t,o) localtime_s (o,t)
#endif

static gulong debug_flags = 0;

static const char *
debug_get_prefix (unsigned long flag) 
{
       if (flag & DEBUG_CACHE)         return "CACHE  ";
       if (flag & DEBUG_CONF)          return "CONF   ";
       if (flag & DEBUG_UPDATE)        return "UPDATE ";
       if (flag & DEBUG_PARSING)       return "PARSING";
       if (flag & DEBUG_GUI)           return "GUI    ";
       if (flag & DEBUG_HTML)          return "HTML   ";
       if (flag & DEBUG_NET)           return "NET    ";
       if (flag & DEBUG_DB)            return "DB     ";
       if (flag & DEBUG_VFOLDER)       return "VFOLDER";
       return "";      
}

void
debug_set_flags (gulong flags)
{
	debug_flags = flags;
}

gulong
debug_get_flags (void)
{
	return debug_flags;
}

void
debug (gulong          flags,
       const gchar   * fmt,
       ...)
{
	char	timebuf[64];
	gchar	*string;
	time_t	now_time_t;
	struct tm now_tm;
	va_list	args;

	g_return_if_fail (fmt != NULL);

	if (!(debug_flags & flags))
		return;

	va_start (args, fmt);
	string = g_strdup_vprintf (fmt, args);
	va_end (args);

	time (&now_time_t);
	localtime_r (&now_time_t, &now_tm);
	strftime (timebuf, sizeof(timebuf), "%H:%M:%S", &now_tm);

	g_print ("%s %s: %s\n", timebuf, debug_get_prefix (flags), string);
}
