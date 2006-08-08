/**
 * @file common.h common routines
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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
#include "feed.h"

#define htmlToCDATA(buffer) g_strdup_printf("<![CDATA[%s]]>", buffer)

extern gboolean lifereaStarted;

/** Conversion function which should be applied to all read XML strings, 
   to ensure proper UTF8. doc points to the xml document and its encoding and
   string is a xmlChar pointer to the read string. The result gchar
   string is returned, the original XML string is freed. */
gchar * utf8_fix(xmlChar * string);

/* converts a UTF-8 string to HTML (resolves XML entities) */
gchar * convertToHTML(gchar * string);

/* converts a UTF-8 string containing HTML tags to plain text */
gchar * unhtmlize(gchar *string);

gchar * unxmlize(gchar *string);

/* parses a XML node and returns its contents as a string */
/* gchar * parseHTML(htmlNodePtr cur); */

/** 
 * Extract XHTML from the children of the passed node.
 *
 * @param cur         parent of the nodes that will be returned
 * @param xhtmlMode   If 0, reads escaped HTML.
 *                    If 1, reads XHTML nodes as children, and wrap in div tag
 *                    If 2, Find a div tag, and return it as a string
 * @param defaultBase 
 * @returns XHTML version of children of passed node
 */
gchar * extractHTMLNode(xmlNodePtr cur, gint xhtmlMode, const gchar *defaultBase);

/**
 * Strips some DHTML constructs from the given HTML string.
 *
 * @param html	some HTML content
 *
 * @return newly allocated stripped HTML string
 */
gchar * common_strip_dhtml(const gchar *html);

/**
 * Convert the given string to proper XHTML content.
 * Note: this function does not respect relative URLs
 * and is to be used for cache migration 1.0 -> 1.1 only!
 *
 * @param text	    usually an entity escaped HTML string
 *
 * @returns a new valid XHTML string
 */
gchar * common_text_to_xhtml(const gchar *text);

/** used to keep track of error messages during feed parsing */
typedef struct errorCtxt {
	feedParserCtxtPtr	fpc;
	gint			errorCount;
} *errorCtxtPtr;

/**
 * Common function to create a XML DOM object from a given XML buffer.
 * 
 * The function returns a XML document pointer or NULL
 * if the document could not be read.
 *
 * @param data		XML document buffer
 * @param length	length of buffer
 * @param errors	parser error context (can be NULL)
 *
 * @return XML document
 */
xmlDocPtr common_parse_xml(gchar *data, guint length, errorCtxtPtr errors);

/**
 * Common function to create a XML DOM object from a given
 * XML buffer. This function sets up a parser context,
 * enables recovery mode and sets up the error handler.
 * 
 * The function returns a XML document pointer or NULL
 * if the document could not be read. It also sets 
 * errormsg to the last error messages on parsing
 * errors. 
 *
 * @param fpc	feed parsing context with valid data
 *
 * @return XML document
 */
xmlDocPtr common_parse_xml_feed(feedParserCtxtPtr fpc);

/**
 * Returns a formatted date string for the given timestamp.
 *
 * @param t		the timestamp
 * @param date_format	a strptime format string
 *
 * @returns a new formatted date string
 */
gchar * common_format_date(time_t t, const gchar *date_format);

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
 * Encodes all non URI conformant characters in the passed
 * string to be included in a HTTP URI.
 *
 * @param string	string to be URI-escaped (will be freed)
 *
 * @returns new string that can be inserted into a HTTP URI
 */
gchar * common_encode_uri_string(gchar *string);

/**
 * Takes an URL and returns a new string containing
 * the escaped URL.
 *
 * @param url		the URL to escape (not freed)
 *
 * @returns new escaped URL string
 */
xmlChar * common_uri_escape(const xmlChar *url);

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
 * Case sensitive strstr() like searching.
 *
 * @param pneedle	a string to find
 * @param phaystack	the string to search in
 *
 * @returns first found position or NULL 
 */
char * common_strcasestr(const char *phaystack, const char *pneedle);

#endif
