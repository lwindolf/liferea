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

#include <glib.h>
#include "node.h"
#include "item.h"

/* The feed interface can be used by all feed list provider plugins
   serving real feeds that are downloaded from the web, are provided
   by local files or executed commands. The feed list provider plugins
   can be are not forced to use this interface. */

/** Flags used in the request structure */
enum feed_request_flags {
	FEED_REQ_SHOW_PROPDIALOG = 1,
	FEED_REQ_RESET_TITLE = 2,	/**< Feed's title should be reset to default upon update */
	FEED_REQ_RESET_UPDATE_INT = 4,	/**< Feed's update interval should be reset to default upon update */
	FEED_REQ_AUTO_DISCOVER = 8,	/**< Feed auto-discovery attempts should be made */
	
	FEED_REQ_PRIORITY_HIGH = 16,	/**< set to signal that this is an important user triggered request */
	FEED_REQ_DOWNLOAD_FAVICON = 32, /**< set to make the favicon be updated after the feed is downloaded */
	FEED_REQ_AUTH_DIALOG = 64	/**< set to make an auth request dialog to be created after 401 errors */
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

/** Common structure to hold all information about a single feed. */
typedef struct feed {
	struct feedHandler *fhp;     		/**< Feed handler saved by the ->typeStr attribute. */
	
	gint		defaultInterval;	/**< update interval as specified by the feed */
	gchar		*parseErrors;		/**< textual/HTML description of parsing errors */
	gchar		*errorDescription;	/**< textual/HTML description of download/parsing errors */

	time_t		time;			/**< Feeds modified date */
	GHashTable	*tmpdata;		/**< tmp data hash used during stateful parsing */
			
	/* feed properties that need to be saved */
	gboolean	available;		/**< flag to signalize loading errors */
	gboolean	discontinued;		/**< flag to avoid updating after HTTP 410 */

	gchar		*title;			/**< feed/channel title */
	gchar		*htmlUrl;		/**< URL of HTML version of the feed */
	gchar		*imageUrl;		/**< URL of the optional feed image */
	gchar		*description;		/**< HTML string describing the feed */
	gchar		*source;		/**< feed source */
	gchar		*filtercmd;		/**< feed filter command */
	gint		updateInterval;		/**< user defined update interval in minutes */
	GSList		*metadata;		/**< metadata of this feed */
	gboolean	encAutoDownload;	/**< enclosure auto download flag */

	/* feed updating state properties */
	gchar		*lastModified;		/**< Last modified string as sent by the server */
	gchar		*etag;			/**< E-Tag sent by the server */
	GTimeVal	lastPoll;		/**< time at which the feed was last updated */
	GTimeVal	lastFaviconPoll;	/**< time at which the feed was last updated */
	gchar		*cookies;		/**< cookies to be used */	
	struct request	*request;		/**< update request structure used when downloading content */
	GSList		*otherRequests;		/**< list of other update request structures used for downloading anything (favicon, blogChannel stuff, ...) */
	
	/* feed cache state properties */
	gint		cacheLimit;		/**< Amount of cache to save: See the cache_limit enum */
	gboolean	noIncremental;		/**< Do merging for this feed but drop old items */
	
} *feedPtr;

/* ------------------------------------------------------------ */
/* feed handler interface					*/
/* ------------------------------------------------------------ */

/** a function which parses the feed data given with the feed ptr fp */
typedef void 	(*feedParserFunc)	(feedPtr fp, itemSetPtr sp, xmlDocPtr doc, xmlNodePtr cur);
typedef gboolean (*checkFormatFunc)	(xmlDocPtr doc, xmlNodePtr cur); /**< Returns true if correct format */

typedef struct feedHandler {
	const gchar	*typeStr;	/**< string representation of the feed type */
	int		icon;		/**< Icon number used for available feeds/directories */
	gboolean	directory;	/**< Determines if a feed should be autoupdated and updated when "update all" is selected */
	feedParserFunc	feedParser;	/**< feed type parse function */
	checkFormatFunc	checkFormat;	/**< Parser for the feed type*/
	gboolean	merge;		/**< flag if feed type supports merging */
	
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
 * @param title		the feed title (or NULL)
 * @param filter	a feed filter (or NULL)
 *
 * @returns the new, empty feed
 */
feedPtr feed_new(gchar *source, gchar *title, gchar *filter);

/**
 * Feed specific feed list import parsing.
 *
 * @param typeStr	feed type string
 * @param cur		DOM node to parse
 * @param trusted	allows filter scripts...
 * @returns pointer to resulting feed
 */
gpointer feed_import(const gchar *typeStr, xmlNodePtr cur, gboolean trusted);

/**
 * Feed specific feed list import parsing.
 *
 * @param fp		the feed to export
 * @param cur		DOM node to write to
 * @param internal	feed list saving/export flag
 * @returns pointer to resulting feed
 */
void feed_export(feedPtr fp, xmlNodePtr cur, gboolean internal);

/**
 * General feed source parsing function. Parses the passed feed source
 * and tries to determine the source type. If the type is HTML and 
 * autodiscover is TRUE the function tries to find a feed, tries to
 * download it and parse the feed's source instead of the passed source.
 *
 * @param fp		the feed structure to be filled
 * @param sp		the item set to be filled
 * @param data		the feed source
 * @param dataLength the length of the 'data' string
 * @param autodiscover	TRUE if auto discovery should be possible
 */
feedHandlerPtr feed_parse(feedPtr fp, itemSetPtr sp, gchar *data, size_t dataLength, gboolean autodiscover);

/**
 * Feed loading from cache.
 *
 * @param fp	the feed structure to load
 * @param id	the cache id of the feed
 *
 * @returns the set of items loaded from cache
 */
itemSetPtr feed_load_from_cache(feedPtr fp, const gchar *id);

/**
 * Saving a given feed and its item set to cache.
 *
 * @param fp	the feed
 * @param sp	its set of items
 * @param id	the cache id of this feed
 */
void feed_save_to_cache(feedPtr fp, itemSetPtr sp, const gchar *id);

/**
 * Removes a feed from cache and free's the feed.
 *
 * @param fp	the feed
 * @param id	the cache id of this feed
 */
void feed_remove_from_cache(feedPtr fp, const gchar *id);

/**
 * Checks wether updating a feed makes sense.
 *
 * @param fp	the feed
 *
 * @returns TRUE if the feed can be updated.
 */
gboolean feed_can_be_updated(feedPtr fp);

/**
 * Prepares a request to update a feed.
 *
 * @param fp		the feed
 * @param request	the request to prepare
 * @param flags		update handling flags
 */
void feed_prepare_request(feedPtr fp, struct request *request, guint flags);

/**
 * Merging implementation for the feed itemset type.
 *
 * @param sp	the itemset to merge against
 * @param ip	the item to merge
 *
 * @returns TRUE if the item can be merged
 */
gboolean feed_merge_check(itemSetPtr sp, itemPtr ip);

/** 
 * To lookup an item given by it's unique item nr 
 */
itemPtr feed_lookup_item(feedPtr fp, gulong nr);

void feed_free(feedPtr fp);

/**
 * This is a timeout callback to check for feed update results.
 * If there is a result pending its data is parsed and merged
 * against the feed it belongs to.
 */
void feed_process_update_result(struct request *request);

/* ------------------------------------------------------------ */
/* feed property get/set 					*/
/* ------------------------------------------------------------ */

/**
 * Lookup a feed type string from the feed type number
 */
feedHandlerPtr feed_type_str_to_fhp(const gchar *str);
const gchar *feed_type_fhp_to_str(feedHandlerPtr fhp);

void feed_increase_unread_counter(feedPtr fp);
void feed_decrease_unread_counter(feedPtr fp);
gint feed_get_unread_counter(feedPtr fp);

void feed_increase_popup_counter(feedPtr fp);
void feed_decrease_popup_counter(feedPtr fp);
gint feed_get_popup_counter(feedPtr fp);

void feed_increase_new_counter(feedPtr fp);
void feed_decrease_new_counter(feedPtr fp);
gint feed_get_new_counter(feedPtr fp);


gint feed_get_default_update_interval(feedPtr fp);
void feed_set_default_update_interval(feedPtr fp, gint interval);

gint feed_get_update_interval(feedPtr fp);
void feed_set_update_interval(feedPtr fp, gint interval);

void feed_reset_update_counter(feedPtr fp);

gboolean feed_get_available(feedPtr fp);
void feed_set_available(feedPtr fp, gboolean available);

gboolean feed_get_discontinued(feedPtr fp);
void feed_set_discontinued(feedPtr fp, gboolean discontinued);

/**
 * Creates a new error description according to the passed
 * HTTP status and the feeds parser errors. If the HTTP
 * status is a success status and no parser errors occurred
 * no error messages is created. The created error message 
 * can be queried with feed_get_error_description().
 *
 * @param fp		feed
 * @param httpstatus	HTTP status
 * @param resultcode the update code's return code (see update.h)
 */
void feed_set_error_description(feedPtr fp, gint httpstatus, gint resultcode, gchar *filterErrors);
/**
 * Returns a HTML string describing the last retrieval error 
 * of this feed. Should only be called when getFeedAvailable
 * returns FALSE.
 *
 * @param fp		feed
 * @return HTML error description
 */
gchar * feed_get_error_description(feedPtr fp);

const gchar *feed_get_id(feedPtr fp);
void feed_set_id(feedPtr fp, const gchar *id);

time_t feed_get_time(feedPtr fp);
void feed_set_time(feedPtr fp, time_t time);

const gchar * feed_get_title(feedPtr fp);
void feed_set_title(feedPtr fp, const gchar * title);

const gchar * feed_get_description(feedPtr fp);
void feed_set_description(feedPtr fp, const gchar *description);

const gchar * feed_get_source(feedPtr fp);
void feed_set_source(feedPtr fp, const gchar *source);

const gchar * feed_get_filter(feedPtr fp);
void feed_set_filter(feedPtr fp, const gchar * filter);

const gchar * feed_get_html_url(feedPtr fp);
void feed_set_html_url(feedPtr fp, const gchar *url);

const gchar * feed_get_image_url(feedPtr fp);
void feed_set_image_url(feedPtr fp, const gchar *url);

const gchar * feed_get_lastmodified(feedPtr fp);
void feed_set_lastmodified(feedPtr fp, const gchar *lastmodified);

const gchar * feed_get_etag(feedPtr fp);
void feed_set_etag(feedPtr fp, const gchar *etag);

feedHandlerPtr feed_get_fhp(feedPtr fp);

/** 
 * Returns a HTML rendering of a feed. The returned string 
 * must be freed.
 *
 * @param fp	the feed
 */
gchar *feed_render(feedPtr fp);

#endif
