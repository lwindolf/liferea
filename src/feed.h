/**
 * @file feed.h common feed handling
 * 
 * Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>
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
#include "common.h"
#include "item.h"
#include "folder.h"

/* ------------------------------------------------------------ */
/* feed list view entry types (FS_TYPE) 			*/
/* ------------------------------------------------------------ */

enum node_types {
	FST_INVALID 	= 0,		/**< invalid type */
	FST_FOLDER 	= 1,		/**< the folder type */

	FST_VFOLDER 	= 9,		/**< special type for VFolders */
	FST_FEED	= 10,		/**< Any type of feed */
	FST_HELPFOLDER	= 50,		/**< special tree list types to store help feeds */	
	FST_HELPFEED	= 51,		/**< special type to allow updating of help feed url */

	FST_AUTODETECT	= 200,		/**< special type to enforce type auto detection */
};

/** macro to test whether a type is a resource which is regularly updated */
#define IS_FEED(type)		((FST_HELPFEED == type) || \
				 (FST_FEED == type))

/** macro to test whether a type is a folder entry */
#define IS_FOLDER(type)		((FST_FOLDER == type) || (FST_HELPFOLDER == type))


/** Flags used in the request structure */
enum feed_request_flags {
	FEED_REQ_SHOW_PROPDIALOG = 1,
	FEED_REQ_RESET_TITLE = 2,	/**< Feed's title should be reset to default upon update */
	FEED_REQ_RESET_UPDATE_INT = 4,	/**< Feed's title should be reset to default upon update */
	FEED_REQ_AUTO_DISCOVER = 8,	/**< Feed auto-discovery attempts should be made */
	
	FEED_REQ_PRIORITY_HIGH = 16,	/**< set to signalize that this is an important user triggered request */
};

struct feedhandler;
struct request;
/* ------------------------------------------------------------ */
/* Feed structure                                               */
/* ------------------------------------------------------------ */

enum cache_limit {
	/* Values > 0 are used to specify certain limits */
	CACHE_DISABLE = 0,
	CACHE_DEFAULT = -1,
	CACHE_UNLIMITED = -2,
};

/** common structure to access feed info structures */

typedef struct feed {
	gint		type;			/**< feed type (first position is important!!!) */
	gpointer	*ui_data;		/**< per-feed UI data (second position is important!!!) */
	
	struct feedHandler *fhp;     		/**< Feed handler saved by the ->typeStr attribute. */
	
	gchar		*id;			/**< unique feed identifier string */
	gint		unreadCount;		/**< number of unread items */
	gint		newCount;		/**< number of new items */
	gint		defaultInterval;	/**< update interval as specified by the feed */
	gboolean	available;		/**< flag to signalize loading errors */
	gboolean	discontinued;		/**< flag to avoid updating after HTTP 410 */
	gchar		*parseErrors;		/**< textual/HTML description of parsing errors */
	gchar		*errorDescription;	/**< textual/HTML description of download/parsing errors */
	
	gpointer	icon;			/**< pointer to pixmap, if there is a favicon */
		
	/* feed properties needed to be saved */
	gchar		*title;			/**< feed/channel title */
	gchar		*htmlUri;		/**< URI of HTML version of the feed */
	gchar		*description;		/**< HTML string describing the feed */
	gchar		*source;		/**< feed source */
	gchar		*filtercmd;		/**< feed filter command */
	gint		updateInterval;		/**< user defined update interval in minutes */
	GSList		*metadata;		/**< metadata of this feed */
	GHashTable	*tmpdata;		/**< tmp data hash used during stateful parsing */
	
	GSList		*items;			/**< list of pointers to the item structures of this channel */
	
	GSList		*filter;		/**< list of filters applied to this feed */
	
	/* feed properties used for updating */
	GTimeVal	lastModified;		/**< Date at which the feed last changed */
	GTimeVal	lastPoll;		/**< time at which the feed was last updated */
	struct request	*request;		/**< update request structure used when downloading xml content */
	struct request *faviconRequest;		/**< update request structure used for downloading the favicon */
	gint		cacheLimit;		/**< Amount of cache to save: See the cache_limit enum */
	gboolean	needsCacheSave;		/**< flag set when the feed's cache needs to be resaved */
} *feedPtr;

/* ------------------------------------------------------------ */
/* feed handler interface					*/
/* ------------------------------------------------------------ */

/** a function which parses the feed data given with the feed ptr fp */
typedef void 	(*feedParserFunc)	(feedPtr fp, xmlDocPtr doc, xmlNodePtr cur);
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
 * @returns the new, empty feed
 */
feedPtr feed_new(void);

/**
 * Loads a feed from a cache file.
 *
 * @param type the type of feed being loaded. This effects the
 * extension of the cache file.
 *
 * @param id the name of the cache file used. Some types of feed have
 * an extension, such as ocs, that is appended to the id, to generate
 * the cache filename.
 *
 * @returns FALSE if the feed file could not be opened and TRUE if it
 * was opened.
 */
gboolean feed_load_from_cache(feedPtr fp);
void feed_merge(feedPtr old_fp, feedPtr new_fp);
void feed_remove(feedPtr fp);
void feed_schedule_update(feedPtr fp, gint flags);
void feed_save(feedPtr fp);

void feed_add_item(feedPtr fp, itemPtr ip);

void feed_copy(feedPtr fp, feedPtr new_fp);
void feed_free(feedPtr fp);

/**
 * General feed source parsing function. Parses the passed feed source
 * and tries to determine the source type. If the type is HTML and 
 * autodiscover is TRUE the function tries to find a feed, tries to
 * download it and parse the feed's source instead of the passed source.
 *
 * @param fp		the feed structure to be filled
 * @param data		the feed source
 * @param autodiscover	TRUE if auto discovery should be possible
 */
feedHandlerPtr feed_parse(feedPtr fp, gchar *data, gboolean autodiscover);

/**
 * This is a timeout callback to check for feed update results.
 * If there is a result pending its data is parsed and merged
 * against the feed it belongs to.
 */
void feed_process_update_result(struct request *request);

/* ------------------------------------------------------------ */
/* feed property get/set 					*/
/* ------------------------------------------------------------ */

gpointer feed_get_favicon(feedPtr fp);

/**
 * Sets the type of feed
 * @param fp feed to modify
 * @param type type to set
 */
void feed_set_type(feedPtr fp, int type);
gint feed_get_type(feedPtr fp);

/**
 * Lookup a feed type string from the feed type number
 */
feedHandlerPtr feed_type_str_to_fhp(const gchar *str);
const gchar *feed_type_fhp_to_str(feedHandlerPtr fhp);

void feed_increase_unread_counter(feedPtr fp);
void feed_decrease_unread_counter(feedPtr fp);
gint feed_get_unread_counter(feedPtr fp);

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

const gchar * feed_get_title(feedPtr fp);
void feed_set_title(feedPtr fp, const gchar * title);

const gchar * feed_get_description(feedPtr fp);
void feed_set_description(feedPtr fp, const gchar *description);

const gchar * feed_get_source(feedPtr fp);
void feed_set_source(feedPtr fp, const gchar * source);

const gchar * feed_get_filter(feedPtr fp);
void feed_set_filter(feedPtr fp, const gchar * filter);

const gchar * feed_get_html_uri(feedPtr fp);
void feed_set_html_uri(feedPtr fp, const gchar *uri);

const feedHandlerPtr feed_get_fhp(feedPtr fp);

GSList * feed_get_item_list(feedPtr fp);
void feed_clear_item_list(feedPtr fp);

void feed_mark_all_items_read(feedPtr fp);

/** Returns a HTML rendering of a feed. The returned string must be
 *  freed
 */
gchar *feed_render(feedPtr fp);

#endif
