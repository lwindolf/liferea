/**
 * @file xml.h XML helper methods for Liferea
 * 
 * Copyright (C) 2003-2007  Lars Lindner <lars.lindner@gmail.com>
 * Copyright (C) 2004-2006  Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _XML_H
#define _XML_H

#include <glib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "feed.h"

/**
 * Initialize XML parsing.
 */
void xml_init (void);

/**
 * Retrieves the text content of an HTML chunk. All entities
 * will be replaced. All HTML tags are stripped. The passed
 * string will be freed.
 *
 * @param string	the string to strip
 *
 * @returns stripped UTF-8 plain text string
 */
gchar * unhtmlize (gchar *string);

/**
 * Retrieves the text content of an XML chunk. All entities
 * will be replaced. All XML tags are stripped. The passed
 * string will be freed.s
 *
 * @param string	the chunk to strip
 *
 * @returns stripped UTF-8 XHTML string
 */
gchar * unxmlize (gchar *string);

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
gchar * xhtml_extract (xmlNodePtr cur, gint xhtmlMode, const gchar *defaultBase);

/**
 * Strips some DHTML constructs from the given HTML string.
 *
 * @param html	some HTML content
 *
 * @return newly allocated stripped HTML string
 */
gchar * xhtml_strip_dhtml (const gchar *html);

/**
 * Convert the given string to proper XHTML content.
 * Note: this function does not respect relative URLs
 * and is to be used for cache migration 1.0 -> 1.1 only!
 *
 * @param text		usually an entity escaped HTML string
 *
 * @returns a new valid XHTML string
 */
gchar * xhtml_from_text (const gchar *text);

/**
 * Checks the given string for XHTML well formedness.
 *
 * @returns TRUE if the string is well formed XHTML
 */
gboolean xhtml_is_well_formed (const gchar *text);

/**
 * Find the first XML node matching an XPath expression.
 *
 * @param node		node to apply the XPath expression to
 * @param expr		an XPath expression string
 *
 * @return first node found that matches expr (or NULL)
 */
xmlNodePtr xpath_find (xmlNodePtr node, gchar *expr);

/** Function type used by common_xpath_foreach_match() */
typedef void (*xpathMatchFunc)(xmlNodePtr match, gpointer user_data);

/**
 * Executes an XPath expression and calls the given function for each matching node.
 *
 * @param node		node to apply the XPath expression to
 * @param expr		an XPath expression string
 * @param func		the function to call for each result
 *
 * @return TRUE if result set was not empty
 */
gboolean xpath_foreach_match (xmlNodePtr node, gchar *expr, xpathMatchFunc func, gpointer user_data);

/** used to keep track of error messages during parsing */
typedef struct errorCtxt {
	GString		*msg;		/**< message buffer */
	gint		errorCount;	/**< error counter */
} *errorCtxtPtr;

/**
 * Common function to create a XML DOM object from a given XML buffer.
 * 
 * The function returns a XML document pointer or NULL
 * if the document could not be read.
 *
 * @param data		XML document buffer
 * @param length	length of buffer
 * @param recovery	enable tolerant XML parsing (use only for RSS 0.9x!)
 * @param errors	parser error context (can be NULL)
 *
 * @return XML document
 */
xmlDocPtr xml_parse (gchar *data, size_t length, gboolean revocery, errorCtxtPtr errors);

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
xmlDocPtr xml_parse_feed (feedParserCtxtPtr fpc);

/**
 * Common function for reliable writing of XML documents
 * to disk. In difference to xmlSaveFile*() this function
 * syncs the file and gives better error handling.
 *
 * @param doc		the XML document to save
 * @param filename	the filename to write to
 *
 * @returns 0 on success
 */
gint xml_save_to_file (xmlDocPtr doc, gchar *filename);

#endif
