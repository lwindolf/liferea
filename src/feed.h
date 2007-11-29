/**
 * @file feed.h common feed handling interface
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

#ifndef _FEED_H
#define _FEED_H

#include <libxml/parser.h>
#include <glib.h>
#include "node.h"
#include "node_type.h"
#include "item.h"
#include "subscription.h"

/* The feed node type can be used by all feed list sources
   serving real feeds that are downloaded from the web, are provided
   by local files or executed commands. Feed list sources are not
   forced to use this node type implementation. */

struct feedHandler;

/** Caching property constants */
enum cache_limit {
	/* Values > 0 are used to specify certain limits */
	CACHE_DISABLE = 0,
	CACHE_DEFAULT = -1,
	CACHE_UNLIMITED = -2,
};

/** Common structure to hold all information about a single feed. */
typedef struct feed {
	struct feedHandler *fhp;     		/**< Feed handler saved by the ->typeStr attribute. */
	
	/* feed properties that need to be saved */	// FIXME: move to metadata
	gchar		*htmlUrl;		/**< URL of HTML version of the feed */
	gchar		*imageUrl;		/**< URL of the optional feed image */

	/* feed cache state properties */
	gint		cacheLimit;		/**< Amount of cache to save: See the cache_limit enum */
	gboolean	noIncremental;		/**< Do merging for this feed but drop old items */
	
	/* feed parsing state */
	gboolean	valid;			/**< FALSE if libxml2 recovery mode was used on last feed parsing */
	GString		*parseErrors;		/**< textual description of parsing errors */
	time_t		time;			/**< Feeds modified date */

	/* feed specific behaviour settings */
	gboolean	encAutoDownload;	/**< enclosure auto download flag */
	gboolean	loadItemLink;		/**< automatic item link load flag */
} *feedPtr;

/** Holds all information used on feed parsing time */
typedef struct feedParserCtxt {
	subscriptionPtr	subscription;	/**< the subscription the feed belongs to (optional) */
	feedPtr		feed;		/**< the feed structure to fill */
	GList		*items;		/**< the list of new items */
	struct item	*item;		/**< the item currently parsed (or NULL) */
	gboolean	recovery;	/**< TRUE if tolerant parsing needed (use only for RSS 0.9x!) */

	GHashTable	*tmpdata;	/**< tmp data hash used during stateful parsing */

	gchar		*title;		/**< resulting feed/channel title */

	gchar		*data;		/**< data buffer to parse */
	gsize		dataLength;	/**< length of the data buffer */

	xmlDocPtr	doc;		/**< the parsed data buffer */
	gboolean	failed;		/**< TRUE if parsing failed because feed type could not be detected */
} *feedParserCtxtPtr;

/* ------------------------------------------------------------ */
/* feed handler interface					*/
/* ------------------------------------------------------------ */

/** a function which parses the feed data given with the feed ptr feed */
typedef void 	(*feedParserFunc)	(feedParserCtxtPtr ctxt, xmlNodePtr cur);
typedef gboolean (*checkFormatFunc)	(xmlDocPtr doc, xmlNodePtr cur); /**< Returns true if correct format */

typedef struct feedHandler {
	const gchar	*typeStr;	/**< string representation of the feed type */
	int		icon;		/**< Icon number used for available nodes without an own favicon */
	feedParserFunc	feedParser;	/**< feed type parse function */
	checkFormatFunc	checkFormat;	/**< Parser for the feed type*/
	gboolean	merge;		/**< TRUE if feed type supports merging */
	gboolean	noXML;		/**< TRUE if feed type isn't guaranteed to be XML */
} *feedHandlerPtr;

/* ------------------------------------------------------------ */
/* feed creation/modification interface				*/
/* ------------------------------------------------------------ */

/** 
 * Initializes feed parsing handlers. Should be called 
 * only once on program startup.
 */
void feed_init(void);

/**
 * Create a new feed structure.
 *
 * @returns a new feed structure
 */
feedPtr feed_new(void);

/**
 * Serialization helper function for rendering and caching purposes.
 *
 * @param node		the feed node to serialize
 * @param feedNode	XML node to add feed attributes to,
 *                      or NULL if a new XML document is to
 *                      be created
 * 
 * @returns a new XML document (if feedNode was NULL)
 */
xmlDocPtr feed_to_xml(nodePtr node, xmlNodePtr feedNode);

/* feed parsing */

/**
 * Creates a new feed parsing context.
 *
 * @returns a new feed parsing context
 */
feedParserCtxtPtr feed_create_parser_ctxt(void);

/**
 * Frees the given parser context. Note: it does
 * not free the list of new items!
 *
 * @param ctxt		the feed parsing context
 */
void feed_free_parser_ctxt(feedParserCtxtPtr ctxt);

/**
 * Returns the feed-specific maximum cache size.
 * If none is set it returns the global default 
 * setting.
 *
 * @param node	the feed node
 *
 * @returns max item count
 */
guint feed_get_max_item_count(nodePtr node);

/* ------------------------------------------------------------ */
/* feed property get/set 					*/
/* ------------------------------------------------------------ */

/**
 * Lookup a feed type string from the feed type number
 */
feedHandlerPtr feed_type_str_to_fhp(const gchar *str);
const gchar *feed_type_fhp_to_str(feedHandlerPtr fhp);

const gchar * feed_get_html_url(feedPtr feed);

/**
 * Set the HTML URL of the given feed. If the passed
 * URL is a relative one it will be expanded using the
 * given base URL.
 *
 * @param feed		the feed
 * @param base		base URL for expansion
 * @param url		the new HTML URL
 */
void feed_set_html_url(feedPtr feed, const gchar *base, const gchar *url);

const gchar * feed_get_image_url(feedPtr feed);
void feed_set_image_url(feedPtr feed, const gchar *url);

const gchar * feed_get_lastmodified(feedPtr feed);
void feed_set_lastmodified(feedPtr feed, const gchar *lastmodified);

const gchar * feed_get_etag(feedPtr feed);
void feed_set_etag(feedPtr feed, const gchar *etag);

/**
 * General feed source parsing function. Parses the passed feed source
 * and tries to determine the source type. 
 *
 * @param ctxt		feed parsing context
 *
 * @returns FALSE if auto discovery is indicated, 
 *          TRUE if feed type was recognized
 */
gboolean feed_parse(feedParserCtxtPtr ctxt);

/* implementation of the node type interface */

#define IS_FEED(node) (node->type == feed_get_node_type ())

/**
 * Returns the node type implementation for feed nodes.
 */
nodeTypePtr feed_get_node_type (void);

#endif
