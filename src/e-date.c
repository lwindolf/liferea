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

#include <config.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <string.h>

#include "e-date.h"

static size_t e_strftime(char *s, size_t max, const char *fmt, const struct tm *tm)
{
#ifdef HAVE_LKSTRFTIME
	return strftime(s, max, fmt, tm);
#else
	char *c, *ffmt, *ff;
	size_t ret;

	ffmt = g_strdup(fmt);
	ff = ffmt;
	while ((c = strstr(ff, "%l")) != NULL) {
		c[1] = 'I';
		ff = c;
	}

	ff = ffmt;
	while ((c = strstr(ff, "%k")) != NULL) {
		c[1] = 'H';
		ff = c;
	}

	ret = strftime(s, max, ffmt, tm);
	g_free(ffmt);
	return ret;
#endif
}


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

size_t e_strftime_fix_am_pm(char *s, size_t max, const char *fmt, const struct tm *tm)
{
	char buf[10];
	char *sp;
	char *ffmt;
	size_t ret;

	if (strstr(fmt, "%p")==NULL && strstr(fmt, "%P")==NULL) {
		/* No AM/PM involved - can use the fmt string directly */
		ret=e_strftime(s, max, fmt, tm);
	} else {
		/* Get the AM/PM symbol from the locale */
		e_strftime (buf, 10, "%p", tm);

		if (buf[0]) {
			/**
			 * AM/PM have been defined in the locale
			 * so we can use the fmt string directly
			 **/
			ret=e_strftime(s, max, fmt, tm);
		} else {
			/**
			 * No AM/PM defined by locale
			 * must change to 24 hour clock
			 **/
			ffmt=g_strdup(fmt);
			for (sp=ffmt; (sp=strstr(sp, "%l")); sp++) {
				/**
				 * Maybe this should be 'k', but I have never
				 * seen a 24 clock actually use that format
				 **/
				sp[1]='H';
			}
			for (sp=ffmt; (sp=strstr(sp, "%I")); sp++) {
				sp[1]='H';
			}
			ret=e_strftime(s, max, ffmt, tm);
			g_free(ffmt);
		}
	}
	return(ret);
}

size_t
e_utf8_strftime_fix_am_pm(char *s, size_t max, const char *fmt, const struct tm *tm)
{
	size_t sz, ret;
	char *locale_fmt, *buf;

	locale_fmt = g_locale_from_utf8(fmt, -1, NULL, &sz, NULL);
	if (!locale_fmt)
		return 0;

	ret = e_strftime_fix_am_pm(s, max, locale_fmt, tm);
	if (!ret) {
		g_free (locale_fmt);
		return 0;
	}

	buf = g_locale_to_utf8(s, ret, NULL, &sz, NULL);
	if (!buf) {
		g_free (locale_fmt);
		return 0;
	}

	if (sz >= max) {
		char *tmp = buf + max - 1;
		tmp = g_utf8_find_prev_char(buf, tmp);
		if (tmp)
			sz = tmp - buf;
		else
			sz = 0;
	}
	memcpy(s, buf, sz);
	s[sz] = '\0';
	g_free(locale_fmt);
	g_free(buf);
	return sz;
}

