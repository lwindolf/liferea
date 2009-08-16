/**
 * @file common.h common routines
 *
 * Copyright (C) 2003-2009 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _COMMON_H
#define _COMMON_H

#include <config.h>
#include <time.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <glib.h>

/*
 * Standard gettext macros
 */
#ifdef ENABLE_NLS
#  include <libintl.h>
#  undef _
#  define _(String) dgettext (PACKAGE, String)
#  define Q_(String) g_strip_context ((String), gettext (String))
#  ifdef gettext_noop
#    define N_(String) gettext_noop (String)
#  else
#    define N_(String) (String)
#  endif
#else
#  define textdomain(String) (String)
#  define gettext(String) (String)
#  define dgettext(Domain,Message) (Message)
#  define dcgettext(Domain,Message,Type) (Message)
#  define bindtextdomain(Domain,Directory) (Domain)
#  define _(String) (String)
#  define Q_(String) g_strip_context ((String), (String))
#  define N_(String) (String)
#endif

/**
 * Parses the given string as a number.
 *
 * @param str	the string to parse
 * @param def	default value to return on parsing error
 *
 * @returns result value
 */
long common_parse_long (const gchar *str, long def);

/**
 * Returns the cache file storage path.
 * Usually: ~/.liferea/
 *
 * @returns the path
 */
const gchar *	common_get_cache_path(void);

/**
 * Method to build cache file names.
 *
 * @param folder	a subfolder in the cache dir (optional)
 * @param filename	the cache filename
 * @param extension	the cache filename extension
 *
 * @returns a newly allocated filename string
 */
gchar * common_create_cache_filename(const gchar *folder, const gchar *filename, const gchar *extension);

/** 
 * To correctly escape and expand URLs, does not touch the
 * passed strings.
 *
 * @param url		relative URL
 * @param baseURL	base URL
 *
 * @returns new string with resulting absolute URL
 */
xmlChar * common_build_url(const gchar *url, const gchar *baseURL);

/**
 * Analyzes the given string and returns the LTR/RTL
 * setting that should be used when displaying it in
 * a GTK tree store.
 *
 * @param text		string to analyze
 *
 * @returns UTF-8 string with direction mark
 */
const gchar * common_get_direction_mark(gchar *text);

#ifndef HAVE_STRSEP
char * common_strsep(char **stringp, const char *delim);
#define strsep(a,b) common_strsep(a,b)
#endif

/**
 * Replaces delimiter in string with a replacement string.
 *
 * @param string	original string (will be freed)
 * @param delimiter	match string
 * @param replacement	replacement string
 *
 * @returns a new modified string
 */
gchar *common_strreplace(gchar *string, const gchar *delimiter, const gchar *replacement);

/**
 * Case insensitive strstr() like searching.
 *
 * @param pneedle	a string to find
 * @param phaystack	the string to search in
 *
 * @returns first found position or NULL 
 */
char * common_strcasestr(const char *phaystack, const char *pneedle);

/**
 * Get file modification timestamp
 *
 * @param *file		the file name
 *
 * @returns modification timestamp (or 0 if file doesn't exist)
 */
time_t common_get_mod_time(const gchar *file);

#endif
