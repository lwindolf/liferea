/**
 * @file common.h common routines
 *
 * Copyright (C) 2003-2012 Lars Windolf <lars.windolf@gmx.de>
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
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif
#include <string.h>
#include <pango/pango-bidi-type.h>

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
 * Method to build user data file names.
 *
 * @param filename	the user data filename (with extension)
 *
 * @returns a newly allocated filename string (to be free'd using g_free)
 */
gchar * common_create_data_filename (const gchar *filename);

/**
 * Method to build config file names.
 *
 * @param filename	the cache filename (with extension)
 *
 * @returns a newly allocated filename string (to be free'd using g_free)
 */
gchar * common_create_config_filename (const gchar *filename);

/**
 * Method to build cache file names.
 *
 * @param folder	a subfolder in the cache dir (optional)
 * @param filename	the cache filename (without extension)
 * @param extension	the cache filename extension
 *
 * @returns a newly allocated filename string (to be free'd using g_free)
 */
gchar * common_create_cache_filename(const gchar *folder, const gchar *filename, const gchar *extension);

/**
 * Takes an URI and returns a new string containing the escaped URI.
 *
 * @param uri		the URI to escape
 *
 * @returns new escaped URI string
 */
xmlChar * common_uri_escape(const xmlChar *uri);

/**
 * Takes an URI and returns a new string containing the unescaped URI.
 *
 * @param uri		the URI to unescape
 *
 * @returns new unescaped URI string
 */
xmlChar * common_uri_unescape(const xmlChar *uri);

/**
 * Takes an URI of uncertain safety (e.g. partially escaped) and
 * returns if fully escaped safe for passing to a shell or browser.
 * This means the resulting URL is ensured to have no quotes, spaces
 * or &. Note: commata are not escaped!
 *
 * @param uri		the URI to escape
 *
 * @returns new escaped URI string
 */
xmlChar * common_uri_sanitize(const xmlChar *uri);

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
 * Replacement for deprecated pango_find_base_dir
 * Searches a string the first character that has a strong direction,
 * according to the Unicode bidirectional algorithm.
 *
 * @param text		The text to process. Must be valid UTF-8
 *
 * @param length	Length of text in bytes 
 * 					(may be -1 if text is nul-terminated)
 *
 * @returns 		The direction corresponding to the first strong character.
 * 					If no such character is found, then 
 * 					PANGO_DIRECTION_NEUTRAL is returned.
 */
PangoDirection common_find_base_dir (const gchar *text, gint length);

/**
 * Analyzes the string, returns a direction setting immediately
 * useful for insertion into HTML
 *
 * @param text		string to analyze
 *
 * @returns a constant "ltr" (default) or "rtl"
 */
const gchar * common_get_text_direction(const gchar *text);

/**
 * Returns the overall directionality of the application
 *
 * @returns a constant "ltr" (default) or "rtl"
 */
const gchar * common_get_app_direction(void);

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
 * Checks if a string is empty, when leading and trailing whitespace is ignored
 *
 * @param string	a string to check
 *
 * @returns TRUE if string only contains whitespace or is NULL, FALSE otherwise
 */
gboolean common_str_is_empty (const gchar *string);

/**
 * Get file modification timestamp
 *
 * @param *file		the file name
 *
 * @returns modification timestamp (or 0 if file doesn't exist)
 */
time_t common_get_mod_time(const gchar *file);

/**
 * Create a localized filename.
 *
 * This function tries all applicable locale names and replaces
 * the %s in str with the first one that points to an existing file.
 * "en" is always among the searched locale names.
 *
 * @param str		full path containing one %s
 *
 * @returns		a string with %s replaced (to be freed by the caller), or NULL
 */
gchar *common_get_localized_filename (const gchar *str);

/**
 * Copy a source file to a target filename.
 *
 * @param src           absolute source file path
 * @param dest          absolute destination file path
 */
void common_copy_file (const gchar *src, const gchar *dest);

#endif
