/**
 * @file common.c common routines for Liferea
 * 
 * Copyright (C) 2003-2009  Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006  Nathan J. Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2004       Karl Soderstrom <ks@xanadunet.net>
 *
 * parts of the RFC822 timezone decoding were reused from the gmime source
 *
 *    Authors: Michael Zucchi <notzed@helixcode.com>
 *             Jeffrey Stedfast <fejj@helixcode.com>
 *
 *    Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 * 
 * The date formatting was reused from the Evolution code base
 *
 *    Author: Chris Lahey <clahey@ximian.com
 *
 *    Copyright 2001, Ximian, Inc.
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

#define _XOPEN_SOURCE	600 /* glibc2 needs this (man strptime) */

#include <libxml/uri.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <pango/pango-types.h>

#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <locale.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#include "common.h"
#include "feed.h"
#include "debug.h"

static gchar *lifereaUserPath = NULL;

long common_parse_long(gchar *str, long def) {
	long num;

	if(str == NULL)
		return def;
	if(0 == (sscanf(str,"%ld",&num)))
		num = def;
	
	return num;
}

/* converts a ISO 8601 time string to a time_t value */
time_t parseISO8601Date(gchar *date) {
	struct tm	tm;
	time_t		t, t2, offset = 0;
	gboolean	success = FALSE;
	gchar *pos;
	
	g_assert(date != NULL);
	
	memset(&tm, 0, sizeof(struct tm));
	
	/* we expect at least something like "2003-08-07T15:28:19" and
	   don't require the second fractions and the timezone info

	   the most specific format:   YYYY-MM-DDThh:mm:ss.sTZD
	 */
	 
	/* full specified variant */
	if(NULL != (pos = strptime((const char *)date, "%t%Y-%m-%dT%H:%M%t", &tm))) {
		/* Parse seconds */
		if (*pos == ':')
			pos++;
		if (isdigit(pos[0]) && !isdigit(pos[1])) {
			tm.tm_sec = pos[0] - '0';
			pos++;
		} else if (isdigit(pos[0]) && isdigit(pos[1])) {
			tm.tm_sec = 10*(pos[0]-'0') + pos[1] - '0';
			pos +=2;
		}
		/* Parse second fractions */
		if (*pos == '.') {
			while (*pos == '.' || isdigit(pos[0]))
				pos++;
		}
		/* Parse timezone */
		if (*pos == 'Z')
			offset = 0;
		else if ((*pos == '+' || *pos == '-') && isdigit(pos[1]) && isdigit(pos[2]) && strlen(pos) >= 3) {
			offset = (10*(pos[1] - '0') + (pos[2] - '0')) * 60 * 60;
			
			if (pos[3] == ':' && isdigit(pos[4]) && isdigit(pos[5]))
				offset +=  (10*(pos[4] - '0') + (pos[5] - '0')) * 60;
			else if (isdigit(pos[3]) && isdigit(pos[4]))
				offset +=  (10*(pos[3] - '0') + (pos[4] - '0')) * 60;
			
			offset *= (pos[0] == '+') ? 1 : -1;

		}
		success = TRUE;
	/* only date */
	} else if(NULL != strptime((const char *)date, "%t%Y-%m-%d", &tm))
		success = TRUE;
	/* there were others combinations too... */

	if(TRUE == success) {
		if((time_t)(-1) != (t = mktime(&tm))) {
			/* Correct for the local timezone*/
			struct tm tmp_tm;
			
			t = t - offset;
			gmtime_r(&t, &tmp_tm);
			t2 = mktime(&tmp_tm);
			t = t - (t2 - t);
			
			return t;
		} else {
			debug0(DEBUG_PARSING, "internal error! time conversion error! mktime failed!");
		}
	} else {
		debug0(DEBUG_PARSING, "Invalid ISO8601 date format! Ignoring <dc:date> information!");
	}
	
	return 0;
}

/* in theory, we'd need only the RFC822 timezones here
   in practice, feeds also use other timezones...        */
static struct {
	const char *name;
	int offset;
} tz_offsets [] = {
	{ "IDLW", -1200 },
	{ "HAST", -1000 },
	{ "AKST", -900 },
	{ "AKDT", -800 },
	{ "WESZ", 100 },
	{ "WEST", 100 },
	{ "WEDT", 100 },
	{ "MEST", 200 },
	{ "MESZ", 200 },
	{ "CEST", 200 },
	{ "CEDT", 200 },
	{ "EEST", 300 },
	{ "EEDT", 300 },
	{ "IRST", 430 },
	{ "CNST", 800 },
	{ "ACST", 930 },
	{ "ACDT", 1030 },
	{ "AEST", 1000 },
	{ "AEDT", 1100 },
	{ "IDLE", 1200 },
	{ "NZST", 1200 },
	{ "NZDT", 1300 },
	{ "GMT", 0 },
	{ "EST", -500 },
	{ "EDT", -400 },
	{ "CST", -600 },
	{ "CDT", -500 },
	{ "MST", -700 },
	{ "MDT", -600 },
	{ "PST", -800 },
	{ "PDT", -700 },
	{ "HDT", -900 },
	{ "YST", -900 },
	{ "YDT", -800 },
	{ "AST", -400 },
	{ "ADT", -300 },
	{ "VST", -430 },
	{ "NST", -330 },
	{ "NDT", -230 },
	{ "WET", 0 },
	{ "WEZ", 0 },
	{ "IST", 100 },
	{ "CET", 100 },
	{ "MEZ", 100 },
	{ "EET", 200 },
	{ "MSK", 300 },
	{ "MSD", 400 },
	{ "IRT", 330 },
	{ "IST", 530 },
	{ "ICT", 700 },
	{ "JST", 900 },
	{ "NFT", 1130 },
	{ "UT", 0 },
	{ "PT", -800 },
	{ "BT", 300 },
	{ "Z", 0 },
	{ "A", -100 },
	{ "M", -1200 },
	{ "N", 100 },
	{ "Y", 1200 }
};
/** @returns timezone offset in seconds */
static time_t common_parse_rfc822_tz(char *token) {
	int offset = 0;
	const char *inptr = token;
	int num_timezones = sizeof(tz_offsets) / sizeof((tz_offsets)[0]);

	if (*inptr == '+' || *inptr == '-') {
		offset = atoi (inptr);
	} else {
		int t;

		if (*inptr == '(')
			inptr++;

		for (t = 0; t < num_timezones; t++)
			if (!strncmp (inptr, tz_offsets[t].name, strlen (tz_offsets[t].name))) {
				offset = tz_offsets[t].offset;
				break;
			}
	}
	
	return 60 * ((offset / 100) * 60 + (offset % 100));
}


/* converts a RFC822 time string to a time_t value */
time_t parseRFC822Date(gchar *date) {
	struct tm	tm;
	time_t		t, t2;
	char 		*oldlocale;
	char		*pos;
	gboolean	success = FALSE;

	memset(&tm, 0, sizeof(struct tm));

	/* we expect at least something like "03 Dec 12 01:38:34" 
	   and don't require a day of week or the timezone

	   the most specific format we expect:  "Fri, 03 Dec 12 01:38:34 CET"
	 */
	/* skip day of week */
	if(NULL != (pos = g_utf8_strchr(date, -1, ',')))
		date = ++pos;

	/* we expect English month names, so we set the locale */
	oldlocale = g_strdup(setlocale(LC_TIME, NULL));
	setlocale(LC_TIME, "C");
	
	/* standard format with seconds and 4 digit year */
	if(NULL != (pos = strptime((const char *)date, " %d %b %Y %T", &tm)))
		success = TRUE;
	/* non-standard format without seconds and 4 digit year */
	else if(NULL != (pos = strptime((const char *)date, " %d %b %Y %H:%M", &tm)))
		success = TRUE;
	/* non-standard format with seconds and 2 digit year */
	else if(NULL != (pos = strptime((const char *)date, " %d %b %y %T", &tm)))
		success = TRUE;
	/* non-standard format without seconds 2 digit year */
	else if(NULL != (pos = strptime((const char *)date, " %d %b %y %H:%M", &tm)))
		success = TRUE;
	
	while(pos && *pos != '\0' && isspace((int)*pos))       /* skip whitespaces before timezone */
		pos++;
	
	if(oldlocale) {
		setlocale(LC_TIME, oldlocale);	/* and reset it again */
		g_free(oldlocale);
	}
	
	if(success) {
		if((time_t)(-1) != (t = mktime(&tm))) {
			/* GMT time, with no daylight savings time
			   correction. (Usually, there is no daylight savings
			   time since the input is GMT.) */
			t = t - common_parse_rfc822_tz(pos);
			t2 = mktime(gmtime(&t));
			t = t - (t2 - t);
			return t;
		} else {
			debug0(DEBUG_PARSING, "internal error! time conversion error! mktime failed!");
		}
	}
	
	return 0;
}

static void
common_check_dir (gchar *path)
{
	if (!g_file_test (path, G_FILE_TEST_IS_DIR)) {
		if (0 != g_mkdir_with_parents (path, S_IRUSR | S_IWUSR | S_IXUSR)) {
			g_error (_("Cannot create cache directory \"%s\"!"), path);
		}
	}
	g_free (path);
}

static void
common_init_cache_path (void)
{
	gchar *cachePath;

	lifereaUserPath = g_build_filename (g_get_home_dir(), ".liferea_1.6", NULL);
	cachePath = g_build_filename (lifereaUserPath, "cache", NULL);

	common_check_dir (g_strdup (lifereaUserPath));
	common_check_dir (g_strdup (cachePath));
	common_check_dir (g_build_filename (cachePath, "feeds", NULL));
	common_check_dir (g_build_filename (cachePath, "favicons", NULL));
	common_check_dir (g_build_filename (cachePath, "plugins", NULL));
	common_check_dir (g_build_filename (cachePath, "scripts", NULL));

	g_free (cachePath);
	/* lifereaUserPath is reused globally */
	
	/* ensure reasonable default umask */
	umask (077);
}

const gchar * common_get_cache_path(void) {
	
	if(!lifereaUserPath)
		common_init_cache_path();
		
	return lifereaUserPath;
}

gchar * common_create_cache_filename(const gchar *folder, const gchar *filename, const gchar *extension) {
	gchar *result;

	result = g_strdup_printf("%s" G_DIR_SEPARATOR_S "%s%s%s%s%s", common_get_cache_path(),
	                         (folder != NULL) ? folder : "",
	                         (folder != NULL) ? G_DIR_SEPARATOR_S : "",
	                         filename,
	                         (extension != NULL)? "." : "",
	                         (extension != NULL)? extension : "");

	return result;
}

/* to correctly escape and expand URLs */
xmlChar * common_build_url(const gchar *url, const gchar *baseURL) {
	xmlChar	*escapedURL, *absURL, *escapedBaseURL;

	escapedURL = xmlURIEscape(url);

	if(NULL != baseURL) {
		escapedBaseURL = xmlURIEscape(baseURL);	
		absURL = xmlBuildURI(escapedURL, escapedBaseURL);
		xmlFree(escapedURL);
		xmlFree(escapedBaseURL);
	} else {
		absURL = escapedURL;
	}

	return absURL;
}

const gchar * common_get_direction_mark(gchar *text) {
	PangoDirection		pango_direction = PANGO_DIRECTION_NEUTRAL;
	GtkTextDirection	gtk_direction;
	
	if(text)
		pango_direction = pango_find_base_dir(text, -1);
		
	switch(pango_direction) {
		case PANGO_DIRECTION_LTR:
			gtk_direction = GTK_TEXT_DIR_LTR;
			break;
		case PANGO_DIRECTION_RTL:
			gtk_direction = GTK_TEXT_DIR_RTL;
			break;
		default:
			gtk_direction = gtk_widget_get_default_direction();
			break;
	}

	switch(gtk_direction) {
		case GTK_TEXT_DIR_RTL: 
			return "\342\200\217"; /* U+200F RIGHT-TO-LEFT MARK */
		case GTK_TEXT_DIR_LTR: 
			return "\342\200\216"; /* U+200E LEFT-TO-RIGHT MARK */
		default:
			return "";
	}
}

#ifndef HAVE_STRSEP
/* code taken from glibc-2.2.1/sysdeps/generic/strsep.c */
char * common_strsep (char **stringp, const char *delim) {
	char *begin, *end;

	begin = *stringp;
	if (begin == NULL)
		return NULL;

	/* A frequent case is when the delimiter string contains only one
	   character.  Here we don't need to call the expensive `strpbrk'
	   function and instead work using `strchr'.  */
	if (delim[0] == '\0' || delim[1] == '\0')
		{
			char ch = delim[0];

			if (ch == '\0')
				end = NULL;
			else
				{
					if (*begin == ch)
						end = begin;
					else if (*begin == '\0')
						end = NULL;
					else
						end = strchr (begin + 1, ch);
				}
		}
	else
		/* Find the end of the token.  */
		end = strpbrk (begin, delim);

	if (end)
		{
			/* Terminate the token and set *STRINGP past NUL character.  */
			*end++ = '\0';
			*stringp = end;
		}
	else
		/* No more delimiters; this is the last token.  */
		*stringp = NULL;
	return begin;
}
#endif  /*  HAVE_STRSEP  */

/* Taken from gaim 24 June 2004, copyrighted by the gaim developers
   under the GPL, etc.... It was slightly modified to free the passed string */
gchar * common_strreplace(gchar *string, const gchar *delimiter, const gchar *replacement) {
	gchar **split;
	gchar *ret;

	g_return_val_if_fail(string      != NULL, NULL);
	g_return_val_if_fail(delimiter   != NULL, NULL);
	g_return_val_if_fail(replacement != NULL, NULL);

	split = g_strsplit(string, delimiter, 0);
	ret = g_strjoinv(replacement, split);
	g_strfreev(split);
	g_free(string);

	return ret;
}

typedef unsigned chartype;

/* strcasestr is Copyright (C) 1994, 1996-2000, 2004 Free Software
   Foundation, Inc.  It was taken from the GNU C Library, which is
   licenced under the GPL v2.1 or (at your option) newer version. */
char *common_strcasestr (const char *phaystack, const char *pneedle)
{
	register const unsigned char *haystack, *needle;
	register chartype b, c;

	haystack = (const unsigned char *) phaystack;
	needle = (const unsigned char *) pneedle;

	b = tolower(*needle);
	if (b != '\0') {
		haystack--;             /* possible ANSI violation */
		do {
			c = *++haystack;
			if (c == '\0')
				goto ret0;
		} while (tolower(c) != (int) b);
		
		c = tolower(*++needle);
		if (c == '\0')
			goto foundneedle;
		++needle;
		goto jin;
		
		for (;;) {
			register chartype a;
			register const unsigned char *rhaystack, *rneedle;
			
			do {
				a = *++haystack;
				if (a == '\0')
					goto ret0;
				if (tolower(a) == (int) b)
					break;
				a = *++haystack;
				if (a == '\0')
					goto ret0;
			shloop:
				;
			}
			while (tolower(a) != (int) b);
			
		jin:      a = *++haystack;
			if (a == '\0')
				goto ret0;
			
			if (tolower(a) != (int) c)
				goto shloop;
			
			rhaystack = haystack-- + 1;
			rneedle = needle;
			a = tolower(*rneedle);
			
			if (tolower(*rhaystack) == (int) a)
				do {
					if (a == '\0')
						goto foundneedle;
					++rhaystack;
					a = tolower(*++needle);
					if (tolower(*rhaystack) != (int) a)
						break;
					if (a == '\0')
						goto foundneedle;
					++rhaystack;
					a = tolower(*++needle);
				} while (tolower (*rhaystack) == (int) a);
			
			needle = rneedle;             /* took the register-poor approach */
			
			if (a == '\0')
				break;
		}
	}
 foundneedle:
	return (char*) haystack;
 ret0:
	return 0;
}

time_t
common_get_mod_time (const gchar *file)
{
	struct stat	attribute;
	struct tm	tm;

	if (stat (file, &attribute) == 0) {
		gmtime_r (&(attribute.st_mtime), &tm);
		return mktime (&tm);
	} else {
		/* this is no error as this method is used to check for files */
		return 0;
	}
}
