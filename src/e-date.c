/* The following date formatting code is originally from Evolution. */

/*
 * e-util.c
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#include <glib.h>
#include <glib/gi18n.h>

#include "e-date.h"


/**
 * Function to do a last minute fixup of the AM/PM stuff if the locale
 * and gettext haven't done it right. Most English speaking countries
 * except the USA use the 24 hour clock (UK, Australia etc). However
 * since they are English nobody bothers to write a language
 * translation (gettext) file. So the locale turns off the AM/PM, but
 * gettext does not turn on the 24 hour clock. Leaving a mess.
 *
 * This routine checks if AM/PM are defined in the locale, if not it
 * forces the use of the 24 hour clock.
 *
 * The function itself is a front end on strftime and takes exactly
 * the same arguments.
 *
 * TODO: Actually remove the '%p' from the fixed up string so that
 * there isn't a stray space.
 **/

gchar *
e_utf8_strftime_fix_am_pm (const char *fmt, const GDateTime *tm)
{
	gchar *buf;
	char *sp;
	char *ffmt;
	gchar *datestr;

	if (g_strstr_len (fmt, -1, "%p")==NULL && g_strstr_len (fmt, -1, "%P")==NULL) {
		/* No AM/PM involved - can use the fmt string directly */
		datestr = g_date_time_format (tm, fmt);
	} else {
		/* Get the AM/PM symbol from the locale */
		buf = g_date_time_format (tm, "%p");

		if (buf && buf[0]) {
			/**
			 * AM/PM have been defined in the locale
			 * so we can use the fmt string directly
			 **/
			datestr = g_date_time_format (tm, fmt);
		} else {
			/**
			 * No AM/PM defined by locale
			 * must change to 24 hour clock
			 **/
			ffmt=g_strdup(fmt);
			for (sp=ffmt; (sp = g_strstr_len (sp, -1, "%l")); sp++) {
				/**
				 * Maybe this should be 'k', but I have never
				 * seen a 24 clock actually use that format
				 **/
				sp[1]='H';
			}
			for (sp=ffmt; (sp=g_strstr_len (sp, -1, "%I")); sp++) {
				sp[1]='H';
			}
			datestr = g_date_time_format (tm, ffmt);
			g_free(ffmt);
		}
		g_free (buf);
	}
	return datestr;
}
