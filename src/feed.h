/**
 * @file feed.h common feed handling
 * 
 * Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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
};

/** macro to test whether a type is a resource which is regularly updated */
#define IS_FEED(type)		(FST_FEED == type)

/** macro to test whether a type is a folder entry */
#define IS_FOLDER(type)		(FST_FOLDER == type)


/** Flags used in the request structure */
enum feed_request_flags {
	FEED_REQ_SHOW_PROPDIALOG = 1,
	FEED_REQ_RESET_TITLE = 2,	/**< Feed's title should be reset to default upon update */
	FEED_REQ_RESET_UPDATE_INT = 4,	/**< Feed's update interval should be reset to default upon update */
	FEED_REQ_AUTO_DISCOVER = 8,	/**< Feed auto-discovery attempts should be made */
	
	FEED_REQ_PRIORITY_HIGH = 16,	/**< set to signal that this is an important user triggered request */
	FEED_REQ_DOWNLOAD_FAVICON = 32, /**< set to make the favicon be updated after the feed is downloaded */
	FEED_REQ_AUTH_DIALOG = 64     /**< set to make an auth request dialog to be created after 401 errors */
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
	gint		loaded;			/**< counter which is non-zero if items are to be kept in memory */
	gboolean	needsCacheSave;		/**< flag set when the feed's cache needs to be resaved */
	gchar		*parseErrors;		/**< textual/HTML description of parsing errors */
	gchar		*errorDescription;	/**< textual/HTML description of download/parsing errors */
	
	gpointer	icon;			/**< pointer to pixmap, if there is a favicon */

	time_t		time;			/**< Feeds modified date */
	GHashTable	*tmpdata;		/**< tmp data hash used during stateful parsing */
			
	/* feed properties needed to be saved */
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
	
	GSList		*items;			/**< list of pointers to the item structures of this channel */
	
	GSList		*rules;			/**< list of rules if this is a vfolder */
	
	/* feed properties used for updating */
	GTimeVal	lastModified;		/**< Date at which the feed last changed */
	GTimeVal	lastPoll;		/**< time at which the feed was last updated */
	GTimeVal	lastFaviconPoll;	/**< time at which the feed was last updated */
	struct request	*request;		/**< update request structure used when downloading xml content */
	GSList		*otherRequests;		/**< list of other update request structures used for downloading anything (favicon, blogChannel stuff, ...) */
	gint		cacheLimit;		/**< Amount of cache to save: See the cache_limit enum */
	gboolean	noIncremental;		/**< Do merging for this feed but drop old items */
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
 * General feed source parsing function. Parses the passed feed source
 * and tries to determine the source type. If the type is HTML and 
 * autodiscover is TRUE the function tries to find a feed, tries to
 * download it and parse the feed's source instead of the passed source.
 *
 * @param fp		the feed structure to be filled
 * @param data		the feed source
 * @param dataLength the length of the 'data' string
 * @param autodiscover	TRUE if auto discovery should be possible
 */
feedHandlerPtr feed_parse(feedPtr fp, gchar *data, size_t dataLength, gboolean autodiscover);

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
 * was opened or was already loaded.
 */
gboolean feed_load(feedPtr fp);

/* Only some feed informations are kept in memory to lower memory
   usage. This method unloads everything besides necessary infos. */
void feed_unload(feedPtr fp);

void feed_merge(feedPtr old_fp, feedPtr new_fp);
void feed_remove(feedPtr fp);
void feed_schedule_update(feedPtr fp, gint flags);
void feed_save(feedPtr fp);

/**
 * Can be used to add a single item to a feed. But it's better to
 * use feed_add_items() to keep the item order of parsed feeds.
 * Should be used for vfolders only.
 */
void feed_add_item(feedPtr fp, itemPtr ip);

/**
 * To be used by parser implementation to merge a new orderd list of
 * items to a feed. Ensures properly ordered joint item list. The
 * passed GList is free'd afterwards!
 */
void feed_add_items(feedPtr fp, GList *items);

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

gboolean feed_get_discontinued(feedPtr fp);
void feed_set_discontinued(feedPtr fp, gboolean discontinued);

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

const time_t feed_get_time(feedPtr fp);
void feed_set_time(feedPtr fp, time_t time);

const gchar * feed_get_title(feedPtr fp);
void feed_set_title(feedPtr fp, const gchar * title);

const gchar * feed_get_description(feedPtr fp);
void feed_set_description(feedPtr fp, const gchar *description);

const gchar * feed_get_source(feedPtr fp);
void feed_set_source(feedPtr fp, const gchar * source);

const gchar * feed_get_filter(feedPtr fp);
void feed_set_filter(feedPtr fp, const gchar * filter);

const gchar * feed_get_html_url(feedPtr fp);
void feed_set_html_url(feedPtr fp, const gchar *url);

const gchar * feed_get_image_url(feedPtr fp);
void feed_set_image_url(feedPtr fp, const gchar *url);

const feedHandlerPtr feed_get_fhp(feedPtr fp);

GSList * feed_get_item_list(feedPtr fp);
void feed_clear_item_list(feedPtr fp);
void feed_remove_items(feedPtr fp);

void feed_mark_all_items_read(feedPtr fp);

/** Returns a HTML rendering of a feed. The returned string must be
 *  freed
 */
gchar *feed_render(feedPtr fp);

#endif
