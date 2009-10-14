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

#include <libxml/uri.h>
#include <gtk/gtk.h>
#include <glib.h>
#include <pango/pango-types.h>

#include <sys/stat.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>
#include <ctype.h>

#include "common.h"
#include "feed.h"
#include "debug.h"

static gchar *lifereaUserPath = NULL;

long
common_parse_long (const gchar *str, long def)
{
	long num;

	if (str == NULL)
		return def;
	if (0 == (sscanf (str,"%ld", &num)))
		num = def;
	
	return num;
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

	lifereaUserPath = g_build_filename (g_get_home_dir(), ".liferea_1.7", NULL);
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

const gchar *
common_get_cache_path (void)
{	
	if (!lifereaUserPath)
		common_init_cache_path ();
		
	return lifereaUserPath;
}

gchar *
common_create_cache_filename (const gchar *folder, const gchar *filename, const gchar *extension)
{
	gchar *result;

	result = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%s%s%s%s%s", common_get_cache_path (),
	                          folder ? folder : "",
	                          folder ? G_DIR_SEPARATOR_S : "",
	                          filename,
	                          extension ? "." : "",
	                          extension ? extension : "");

	return result;
}

/* to correctly escape and expand URLs */
xmlChar *
common_build_url (const gchar *url, const gchar *baseURL)
{
	xmlChar	*escapedURL, *absURL, *escapedBaseURL;

	escapedURL = xmlURIEscape (url);

	if (baseURL) {
		escapedBaseURL = xmlURIEscape (baseURL);	
		absURL = xmlBuildURI (escapedURL, escapedBaseURL);
		xmlFree (escapedURL);
		xmlFree (escapedBaseURL);
	} else {
		absURL = escapedURL;
	}

	return absURL;
}

const gchar *
common_get_direction_mark (gchar *text)
{
	PangoDirection		pango_direction = PANGO_DIRECTION_NEUTRAL;
	GtkTextDirection	gtk_direction;
	
	if (text)
		pango_direction = pango_find_base_dir (text, -1);
		
	switch (pango_direction) {
		case PANGO_DIRECTION_LTR:
			gtk_direction = GTK_TEXT_DIR_LTR;
			break;
		case PANGO_DIRECTION_RTL:
			gtk_direction = GTK_TEXT_DIR_RTL;
			break;
		default:
			gtk_direction = gtk_widget_get_default_direction ();
			break;
	}

	switch (gtk_direction) {
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
char *
common_strsep (char **stringp, const char *delim)
{
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
gchar *
common_strreplace (gchar *string, const gchar *delimiter, const gchar *replacement)
{
	gchar **split;
	gchar *ret;

	g_return_val_if_fail (string      != NULL, NULL);
	g_return_val_if_fail (delimiter   != NULL, NULL);
	g_return_val_if_fail (replacement != NULL, NULL);

	split = g_strsplit (string, delimiter, 0);
	ret = g_strjoinv (replacement, split);
	g_strfreev (split);
	g_free (string);

	return ret;
}

typedef unsigned chartype;

/* strcasestr is Copyright (C) 1994, 1996-2000, 2004 Free Software
   Foundation, Inc.  It was taken from the GNU C Library, which is
   licenced under the GPL v2.1 or (at your option) newer version. */
char *
common_strcasestr (const char *phaystack, const char *pneedle)
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
