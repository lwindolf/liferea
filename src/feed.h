/**
 * @file feed.h common feed handling interface
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

#ifndef _FEED_H
#define _FEED_H

#include <libxml/parser.h>
#include <glib.h>
#include "node.h"
#include "item.h"

/* The feed interface can be used by all feed list provider plugins
   serving real feeds that are downloaded from the web, are provided
   by local files or executed commands. The feed list provider plugins
   can be are not forced to use this interface. */

/** Flags used in the request structure */
enum feed_request_flags {
	FEED_REQ_RESET_TITLE		= (1<<0),	/**< Feed's title should be reset to default upon update */
	FEED_REQ_RESET_UPDATE_INT	= (1<<1),	/**< Feed's update interval should be reset to default upon update */
	FEED_REQ_AUTO_DISCOVER		= (1<<2),	/**< Feed auto-discovery attempts should be made */
	
	FEED_REQ_PRIORITY_HIGH		= (1<<3),	/**< set to signal that this is an important user triggered request */
	FEED_REQ_DOWNLOAD_FAVICON	= (1<<4),	/**< set to make the favicon be updated after the feed is downloaded */
	FEED_REQ_AUTH_DIALOG		= (1<<5),	/**< set to make an auth request dialog to be created after 401 errors */
	FEED_REQ_ALLOW_RETRIES		= (1<<6),	/**< set to allow fetch retries on network errors */
	FEED_REQ_NO_PROXY		= (1<<7)	/**< sets no proxy flag */
};

struct feedHandler;
struct request;

/** Caching property constants */
enum cache_limit {
	/* Values > 0 are used to specify certain limits */
	CACHE_DISABLE = 0,
	CACHE_DEFAULT = -1,
	CACHE_UNLIMITED = -2,
};

/** Holds all information used on feed parsing time */
typedef struct feedParserCtxt {
	struct feed	*feed;		/**< the feed to be parsed */
	struct node	*node;		/**< the node the feed belongs to */
	struct itemSet	*itemSet;	/**< the item set to fill */
	struct item	*item;		/**< the item currently parsed (or NULL) */
	gboolean	recovery;	/**< TRUE if tolerant parsing needed (use only for RSS 0.9x!) */

	GHashTable	*tmpdata;	/**< tmp data hash used during stateful parsing */

	gchar		*title;		/**< resulting feed/channel title */

	gchar		*data;		/**< data buffer to parse */
	gsize		dataLength;	/**< length of the data buffer */

	xmlDocPtr	doc;		/**< the parsed data buffer */
	gboolean	failed;		/**< TRUE if parsing failed because feed type could not be detected */
} *feedParserCtxtPtr;

/** Common structure to hold all information about a single feed. */
typedef struct feed {
	struct feedHandler *fhp;     		/**< Feed handler saved by the ->typeStr attribute. */
	
	gint		defaultInterval;	/**< update interval as specified by the feed */
	GString		*parseErrors;		/**< textual description of parsing errors */
	gchar		*updateError;		/**< textual description of processing errors */
	gchar		*filterError;		/**< textual description of filter errors */
	gchar		*httpError;		/**< textual description of HTTP protocol errors */	
	gint		httpErrorCode;		/**< last HTTP error code */

	time_t		time;			/**< Feeds modified date */
			
	/* feed properties that need to be saved */
	gboolean	discontinued;		/**< flag to avoid updating after HTTP 410 */

	gchar		*htmlUrl;		/**< URL of HTML version of the feed */
	gchar		*imageUrl;		/**< URL of the optional feed image */
	gchar		*description;		/**< HTML string describing the feed */
	gchar		*filtercmd;		/**< feed filter command */
	gint		updateInterval;		/**< user defined update interval in minutes */
	GSList		*metadata;		/**< metadata of this feed */
	gboolean	encAutoDownload;	/**< enclosure auto download flag */
	gboolean	loadItemLink;		/**< automatic item link load flag */

	/** feed source definition */	
	gchar		*source;		/**< current feed source, can be changed by redirects */
	gchar		*origSource;		/**< the feed source given when creating the subscription */
	updateOptionsPtr updateOptions;		/**< update options for the feed source */
	
	/* feed cache state properties */
	gint		cacheLimit;		/**< Amount of cache to save: See the cache_limit enum */
	gboolean	noIncremental;		/**< Do merging for this feed but drop old items */
	updateStatePtr	updateState;		/**< update states (etag, last modified, cookies, last polling times...) */	
} *feedPtr;

/* ------------------------------------------------------------ */
/* feed handler interface					*/
/* ------------------------------------------------------------ */

/** a function which parses the feed data given with the feed ptr feed */
typedef void 	(*feedParserFunc)	(feedParserCtxtPtr ctxt, xmlNodePtr cur);
typedef gboolean (*checkFormatFunc)	(xmlDocPtr doc, xmlNodePtr cur); /**< Returns true if correct format */

typedef struct feedHandler {
	const gchar	*typeStr;	/**< string representation of the feed type */
	int		icon;		/**< Icon number used for available feeds/directories */
	gboolean	directory;	/**< Determines if a feed should be autoupdated and updated when "update all" is selected */
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
 * @param source	the feed source URL (or NULL)
 * @param filter	a feed filter (or NULL)
 * @param options	update options (or NULL)
 *
 * @returns the new, empty feed
 */
feedPtr feed_new(const gchar *source, const gchar *filter, updateOptionsPtr options);

/**
 * Serialization helper function for rendering and caching purposes.
 *
 * @param node		the feed node to serialize
 * @param feedNode	XML node to add feed attributes to,
 *                      or NULL if a new XML document is to
 *                      be created
 * @param rendering	TRUE if XML output is to be used
 *                  	for rendering (adds some more tags)
 * 
 * @returns a new XML document (if feedNode was NULL)
 */
xmlDocPtr feed_to_xml(nodePtr node, xmlNodePtr feedNode, gboolean rendering);

/* feed parsing */

/**
 * Creates a new feed parsing context.
 *
 * @returns a new feed parsing context
 */
feedParserCtxtPtr feed_create_parser_ctxt(void);

/**
 * Frees the given parser context. Note: it does
 * not free the constructed itemset!
 *
 * @param ctxt		the feed parsing context
 */
void feed_free_parser_ctxt(feedParserCtxtPtr ctxt);

/**
 * Cancel feed request waiting to be retried, if any.
 *
 * @param node	the feed node
 */
void feed_cancel_retry(nodePtr node);

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

/**
 * Merging implementation for the feed itemset type.
 *
 * @param sp	the itemset to merge against
 * @param ip	the item to merge
 *
 * @returns TRUE if the item can be merged
 */
gboolean feed_merge_check(itemSetPtr sp, itemPtr ip);

/* ------------------------------------------------------------ */
/* feed property get/set 					*/
/* ------------------------------------------------------------ */

/**
 * Lookup a feed type string from the feed type number
 */
feedHandlerPtr feed_type_str_to_fhp(const gchar *str);
const gchar *feed_type_fhp_to_str(feedHandlerPtr fhp);

gint feed_get_default_update_interval(feedPtr feed);
void feed_set_default_update_interval(feedPtr feed, gint interval);

gint feed_get_update_interval(feedPtr feed);
void feed_set_update_interval(feedPtr feed, gint interval);

const gchar * feed_get_title(feedPtr feed);
void feed_set_title(feedPtr feed, const gchar * title);

const gchar * feed_get_description(feedPtr feed);
void feed_set_description(feedPtr feed, const gchar *description);

const gchar * feed_get_source(feedPtr feed);
void feed_set_source(feedPtr feed, const gchar *source);

const gchar * feed_get_orig_source(feedPtr feed);
void feed_set_orig_source(feedPtr feed, const gchar *source);

const gchar * feed_get_filter(feedPtr feed);
void feed_set_filter(feedPtr feed, const gchar * filter);

const gchar * feed_get_html_url(feedPtr feed);
void feed_set_html_url(feedPtr feed, const gchar *url);

const gchar * feed_get_image_url(feedPtr feed);
void feed_set_image_url(feedPtr feed, const gchar *url);

const gchar * feed_get_lastmodified(feedPtr feed);
void feed_set_lastmodified(feedPtr feed, const gchar *lastmodified);

const gchar * feed_get_etag(feedPtr feed);
void feed_set_etag(feedPtr feed, const gchar *etag);

feedHandlerPtr feed_get_fhp(feedPtr feed);

/* implementation of feed node update request processing callback */

/**
 * Gets called by the download handling to start the
 * result processing for feed nodes.
 *
 * @param request	the request to process
 */
void feed_process_update_result(struct request *request);

/* implementation of the node type interface */

/**
 * Returns the node type implementation for feed nodes.
 */
nodeTypePtr feed_get_node_type(void);

/**
 * Request the favicon of the given feed node to be updated.
 *
 * @param node		the feed node
 * @param now		current time
 */
void feed_update_favicon(nodePtr node, GTimeVal *now);

#endif
