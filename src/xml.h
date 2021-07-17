/**
 * @file xml.h  XML helper methods for Liferea
 *
 * Copyright (C) 2003-2020  Lars Windolf <lars.windolf@gmx.de>
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

#include "feed_parser.h"

/**
 * Initialize XML parsing.
 */
void xml_init (void);

/**
 * Deinit XML parsing
 */
void xml_deinit (void);

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
 * string will be freed.
 *
 * @param string	the chunk to strip
 *
 * @returns stripped UTF-8 XHTML string
 */
gchar * unxmlize (gchar *string);

/**
 * xhtml_parse:
 *
 * DOM parse an XHTML string.
 *
 * @html:	The HTML
 * @nodeBase:	An URI to set as xml:base, or #NULL
 *
 * Returns: XHTML version of the HTML
 */

xmlDocPtr xhtml_parse (const gchar *html, gint len);

/**
 * Extract XHTML from a string of HTML and place it in a div tag.
 *
 * @param html		The HTML
 * @param nodeBase	An URI to set as xml:base, or #NULL
 *
 * @returns XHTML version of the HTML
 */
gchar * xhtml_extract_from_string (const gchar *html, const gchar *nodeBase);

/**
* Extract XHTML document from the children of the passed node.
*
* @param cur         parent of the nodes that will be returned
* @param xhtmlMode   If 0, reads escaped HTML.
*                    If 1, reads XHTML nodes as children, and wrap in div tag
*                    If 2, Find a div tag, and return it as a string
* @param defaultBase
* @returns XHTML document containing children of passed node
*/
xmlDocPtr xhtml_extract_doc (xmlNodePtr cur, gint xhtmlMode, const gchar *defaultBase);

/**
 * Extract XHTML string from the children of the passed node.
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
 * Removes self closing tags (on one line) from HTML so that it renders correctly in the browser.
 *
 * @param html	some HTML content
 *
 * @return newly allocated stripped HTML string
 */
gchar * xhtml_expand_self_closing_tag (const gchar *html);

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
xmlNodePtr xpath_find (xmlNodePtr node, const gchar *expr);

/** Function type used by xpath_foreach_match() */
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
gboolean xpath_foreach_match (xmlNodePtr node, const gchar *expr, xpathMatchFunc func, gpointer user_data);

/**
 * Return the value of a attribute.
 *
 * @param node		XML node
 * @param name		attribute name
 *
 * @returns the attribute value (or NULL) to be free'd with g_free
 */
gchar * xml_get_attribute (xmlNodePtr node, const gchar *name);

/**
 * Return the value of a attribute.
 * This is the namespace sensitive version of xml_get_attribute().
 *
 * @param node		XML node
 * @param name		attribute name
 * @param namespace	attribute namespace
 *
 * @returns the attribute value (or NULL) to be free'd with g_free
 */
gchar * xml_get_ns_attribute (xmlNodePtr node, const gchar *name, const gchar *namespace);

/** used to keep track of error messages during parsing */
typedef struct errorCtxt {
	GString		*msg;		/**< message buffer */
	gint		errorCount;	/**< error counter */
} *errorCtxtPtr;

/**
 * Common function to create a XML DOM object from a given string
 *
 * @param data		XML document buffer
 * @param length	length of buffer
 * @param errors	parser error context (can be NULL)
 *
 * @return XML document
 */
xmlDocPtr xml_parse (const gchar *data, size_t length, errorCtxtPtr errors);

/**
 * Common function to create a XML DOM object from a given
 * XML buffer. This function sets up a parser context
 * and sets up the error handler.
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

#endif
