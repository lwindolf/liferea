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

	FST_VFOLDER 	= 9,		/**<special type for VFolders */
	FST_FEED		= 10,     /**< Any type of feed */
	FST_HELPFOLDER	= 50,	/**< special tree list types to store help feeds */	
	FST_HELPFEED	= 51,		/**< special type to allow updating of help feed url */

	FST_AUTODETECT	= 200,	/**< special type to enforce type auto detection */
};

/** macro to test whether a type is a resource which is regularly updated */
#define IS_FEED(type)		((FST_HELPFEED == type) || \
				 (FST_FEED == type))

/** macro to test whether a type is a folder entry */
#define IS_FOLDER(type)		((FST_FOLDER == type) || (FST_HELPFOLDER == type))

struct feedhandler;

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
	gint		type;			/**< feed type (position is important!!!)*/
	
	/* per-feed UI data */
	gpointer	*ui_data;
	
	gchar		*id;			/**< unique feed identifier string */
	gint		unreadCount;		/**< number of unread items */
	gint		defaultInterval;	/**< update interval as specified by the feed */
	gboolean	available;		/**< flag to signalize loading errors */
	gchar		*parseErrors;		/**< textual/HTML description of parsing errors */
	
	gpointer	icon;			/**< pointer to pixmap, if there is a favicon */
		
	/* feed properties needed to be saved */
	gchar		*title;			/**< feed/channel title */
	gchar		*description;		/**< HTML string describing the feed */
	gchar		*source;		/**< feed source, FIXME: needed??? */
	gint		updateInterval;		/**< user defined update interval in minutes */

	GSList		*items;			/**< list of pointers to the item structures of this channel */
	
	GSList		*filter;		/**< list of filters applied to this feed */
	struct feedHandler *fhp;      /**< Feed handler saved by the ->typeStr attribute. */
	gint			cacheLimit;  /**< Amount of cache to save: See the cache_limit enum */
	
	/* feed properties used for updating */
	GTimeVal	scheduledUpdate;	/**< time at which the feed needs to be updated */
	gboolean	updateRequested;	/**< flag set when update in progress */
	gpointer	*request;		/**< update request structure */

	gboolean needsCacheSave; /**< flag set when the feed's cache needs to be resaved */
} *feedPtr;

/* ------------------------------------------------------------ */
/* feed handler interface					*/
/* ------------------------------------------------------------ */

/** a function which parses the feed data given with the feed ptr fp */
typedef void 	(*feedParserFunc)		(feedPtr fp, xmlDocPtr doc, xmlNodePtr cur);
typedef gboolean (*checkFormatFunc) (xmlDocPtr doc, xmlNodePtr cur); /**< Returns true if correct format */

typedef struct feedHandler {
	const gchar *typeStr; /**< string representation of the feed type */
	int icon; /**< Icon number used for available feeds/directories */
     gboolean directory; /**< Determines if a feed should be autoupdated and updated when "update all" is selected */
	feedParserFunc	    feedParser;	/**< feed type parse function */
	checkFormatFunc checkFormat;  /**< Parser for the feed type*/
	gboolean		merge;		/**< flag if feed type supports merging */
	
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
void feed_schedule_update(feedPtr fp);
void feed_save(feedPtr fp);

void feed_add_item(feedPtr fp, itemPtr ip);

void feed_copy(feedPtr fp, feedPtr new_fp);
void feed_free(feedPtr fp);
feedHandlerPtr feed_parse(feedPtr fp, gchar *data);
/**
 * This is a timeout callback to check for feed update results.
 * If there is a result pending its data is parsed and merged
 * against the feed it belongs to.
 */
gint feed_process_update_results(gpointer data);

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
feedHandlerPtr feed_type_str_to_fhp(gchar *str);
const gchar *feed_type_fhp_to_str(feedHandlerPtr fhp);

void feed_increase_unread_counter(feedPtr fp);
void feed_decrease_unread_counter(feedPtr fp);
gint feed_get_unread_counter(feedPtr fp);

gint feed_get_default_update_interval(feedPtr fp);
gint feed_get_update_interval(feedPtr fp);
void feed_set_update_interval(feedPtr fp, gint interval);

void feed_reset_update_counter(feedPtr fp);

gboolean feed_get_available(feedPtr fp);
void feed_set_available(feedPtr fp, gboolean available);

/**
 * Returns a HTML string describing the last retrieval error 
 * of this feed. Should only be called when getFeedAvailable
 * returns FALSE. Caller must free returned string! 
 *
 * @return HTML error description
 */
gchar * feed_get_error_description(feedPtr fp);

gchar *feed_get_id(feedPtr fp);
void feed_set_id(feedPtr fp, gchar *id);

gchar * feed_get_title(feedPtr fp);
void feed_set_title(feedPtr fp, gchar * title);

gchar * feed_get_description(feedPtr fp);

gchar * feed_get_source(feedPtr fp);
void feed_set_source(feedPtr fp, gchar * source);

GSList * feed_get_item_list(feedPtr fp);
void feed_clear_item_list(feedPtr fp);

void feed_mark_all_items_read(feedPtr fp);

#endif
