/**
 * @file feed_parser.h  parsing of different feed formats
 *
 * Copyright (C) 2008-2023 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _FEED_PARSER_H
#define _FEED_PARSER_H

#include <libxml/parser.h>

#include "feed.h"

/** Holds all information used on feed parsing time */
typedef struct feedParserCtxt {
	subscriptionPtr	subscription;		/**< the subscription the feed belongs to (optional) */
	feedPtr		feed;			/**< the feed structure to fill */
	GList		*items;			/**< the list of new items */
	itemPtr		item;			/**< the item currently parsed (or NULL) */

	GHashTable	*tmpdata;		/**< tmp data hash used during stateful parsing */

	gchar		*title;			/**< resulting feed/channel title */

	const gchar	*data;			/**< data buffer to parse */
	gsize		dataLength;		/**< length of the data buffer */
} *feedParserCtxtPtr;

/**
 * Function type which parses the given feed data.
 *
 * @param ctxt	feed parsing context
 * @param cur	the XML node to parse
 */
typedef void 	(*feedParserFunc)	(feedParserCtxtPtr ctxt, xmlNodePtr cur);

/**
 * Function type which checks a given XML document if it has the expected format.
 *
 * @param doc	the XML document
 * @param cur	the XML node to parse
 *
 * @return TRUE if the XML document has the correct format
 */
typedef gboolean (*checkFormatFunc)	(xmlDocPtr doc, xmlNodePtr cur);

/** feed handler interface */
typedef struct feedHandler {
	const gchar	*typeStr;		/**< string representation of the feed type */
	feedParserFunc	feedParser;		/**< feed type parse function */
	checkFormatFunc	checkFormat;		/**< Parser for the feed type*/
	gboolean	html;			/**< TRUE if this is a HTML parser (as opposed tp XML feeds) */
} *feedHandlerPtr;

/**
 * Creates a new feed parsing context.
 *
 * @subscription the feed's subscription
 * @data         the feed data to parse
 * @size         size of feed data
 *
 * @returns a new feed parsing context
 */
feedParserCtxtPtr feed_parser_ctxt_new (subscriptionPtr subscription, const gchar *data, gsize size);

/**
 * Frees the given parser context. Note: it does
 * not free the list of new items!
 *
 * @param ctxt		the feed parsing context
 */
void feed_parser_ctxt_free (feedParserCtxtPtr ctxt);

/**
 * Lookup a feed type string from the feed type id.
 *
 * @param id	feed type id
 *
 * @returns feed parser implementation
 */
feedHandlerPtr feed_type_str_to_fhp (const gchar *id);

/**
 * Get feed type id for a given parser implementation.
 *
 * @param fhp	feed type handler
 *
 * @returns feed type id
 */
const gchar *feed_type_fhp_to_str (feedHandlerPtr fhp);

/**
 * General feed source parsing function. Parses the passed feed source
 * and tries to determine the source type.
 *
 * @param ctxt		feed parsing context
 *
 * @returns FALSE if auto discovery is indicated,
 *          TRUE if feed type was recognized
 */
gboolean feed_parse (feedParserCtxtPtr ctxt);

#endif
