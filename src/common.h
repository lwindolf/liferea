/**
 * @file common.h common routines
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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
#include <gtk/gtk.h>

extern gboolean lifereaStarted;

/*
 * Standard gettext macros (as provided by glade-2).
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

/** Conversion function which should be applied to all read XML strings, 
   to ensure proper UTF8. doc points to the xml document and its encoding and
   string is a xmlChar pointer to the read string. The result gchar
   string is returned, the original XML string is freed. */
gchar * common_utf8_fix(xmlChar * string);

/**
 * Parses the given string as a number.
 *
 * @param str	the string to parse
 * @param def	default value to return on parsing error
 *
 * @returns result value
 */
long common_parse_long(gchar *str, long def);

/**
 * Returns a formatted date string for the given timestamp.
 *
 * @param t		the timestamp
 * @param date_format	a strptime format string (encoded in user locale!)
 *
 * @returns a new formatted date string (encoded in user locale!)
 */
gchar * common_format_date(time_t date, const gchar *date_format);

/**
 * Returns a formatted date string for the given timestamp.
 * In difference to common_format_date() it uses different
 * format string according to the time difference to today.
 *
 * @param t		the timestamp
 *
 * @returns a new formatted date string (encoded in user locale!)
 */
gchar * common_format_nice_date(time_t date);

/**
 * Parses a ISO8601 date.
 *
 * @returns timestamp
 */
time_t 	parseISO8601Date(gchar *date);

/**
 * Parses a RFC822 format date. This FAILS if a timezone string is
 * specified such as EDT or EST and that timezone is in daylight
 * savings time.
 *
 * @returns timestamp (GMT, no daylight savings time)
 */
time_t 	parseRFC822Date(gchar *date);

/**
 * Creates a date string from the given timestamp.
 *
 * @param time	pointer to timestamp
 *
 * @returns time as string
 */
gchar *createRFC822Date(const time_t *time);

/**
 * FIXME: formatDate used by several functions not only
 * to format date column, don't use always date column format!!!
 * maybe gchar * formatDate(time_t, gchar *format) 
 */
gchar * formatDate(time_t t);

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
 * Returns explanation string for the given HTTP 4xx error code.
 *
 * @param httpstatus	HTTP error code between 401 and 410
 *
 * @returns explanation string
 */
const gchar * common_http_error_to_str(gint httpstatus);

/**
 * Returns explanation string for the given network error code.
 *
 * @param netstatus	network error status
 *
 * @returns explanation string
 */
const gchar * common_netio_error_to_str(gint netstatus);

/**
 * Encodes all non URI conformant characters in the passed
 * string to be included in a HTTP URI.
 *
 * @param string	string to be URI-escaped (will be freed)
 *
 * @returns new string that can be inserted into a HTTP URI
 */
gchar * common_encode_uri_string(gchar *string);

/**
 * Takes an URL and returns a new string containing the escaped URL.
 *
 * @param url		the URL to escape
 *
 * @returns new escaped URL string
 */
xmlChar * common_uri_escape(const xmlChar *url);

/**
 * Takes an URL and returns a new string containing the unescaped URL.
 *
 * @param url		the URL to unescape
 *
 * @returns new unescaped URL string
 */
xmlChar * common_uri_unescape(const xmlChar *url);

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
 * Adds a directory to the list of pixmap directories
 * to be searched when using create_pixbuf()
 *
 * @param directory	directory path name
 */
void	add_pixmap_directory (const gchar *directory);

/**
 * Takes a filename and tries to load the image into a GdkPixbuf. 
 *
 * @param filename	the filename
 *
 * @returns a new pixbuf or NULL
 */
GdkPixbuf*  create_pixbuf (const gchar *filename);

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

#endif
