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
#include <gtk/gtk.h>
#include "item.h"

/* ------------------------------------------------------------ */
/* feed list view entry types (FS_TYPE) 			*/
/* ------------------------------------------------------------ */
#define FST_INVALID	0	/**< invalid type */
#define FST_FOLDER	1	/**< the folder type */
#define FST_RSS		3	/**< generic RSS */
#define FST_OCS		4	/**< OCS directories */
#define FST_CDF		5	/**< Microsoft CDF */
#define FST_PIE		6	/**< Atom/Echo/PIE */
#define FST_OPML	7	/**< generic OPML */

#define FST_VFOLDER	9	/**<sepcial type for VFolders */

#define FST_HELPFOLDER	50	/**< special tree list types to store help feeds */	
#define FST_HELPFEED	51	/**< special type to allow updating of help feed url */

#define FST_EMPTY	100	/**< special type for "(empty)" entry */

#define FST_AUTODETECT	200	/**< special type to enforce type auto detection */

/** macro to test wether a type is a ressource which is regularily updated */
#define IS_FEED(type)		((FST_RSS == type) || \
				 (FST_CDF == type) || \
				 (FST_PIE == type) || \
				 (FST_OPML == type) || \
				 (FST_HELPFEED == type))

/** macro to test wether a type is a ressource which not regularily updated */				 
#define IS_DIRECTORY(type)	(FST_OCS == type)
				 
/** macro to test wether a type is only a tree list structure entry */
#define IS_FOLDER(type)		((FST_FOLDER == type) || (FST_HELPFOLDER == type))

/** macro to test if feed menu action can be applied to this entry */
#define FEED_MENU(type)		(IS_FEED(type) || IS_DIRECTORY(type))

/** common structure to access feed info structures */
typedef struct feed {
	gint		type;			/**< feed type */
	gchar		*key;			/**< unique feed identifier */
	gchar		*keyprefix;		/**< FIXME: remove this, bad design! */
	gint		unreadCount;		/**< number of unread items */
	gint		defaultInterval;	/**< update interval as specified by the feed */
	gboolean	available;		/**< flag to signalize loading errors */
	
	gchar		*parseErrors;		/**< textual/HTML description of parsing errors */
	
	gpointer	icon;			/**< pointer to pixmap, if theres a favicon */
		
	/* feed properties needed to be saved */
	gchar		*title;			/**< feed/channel title */
	gchar		*description;		/**< HTML string describing the feed */
	gchar		*source;		/**< feed source, FIXME: needed??? */
	gint		updateInterval;		/**< user defined update interval in minutes */

	GSList		*items;			/**< list of pointers to the item structures of this channel */
	
	GSList		*filter;		/**< list of filters applied to this feed */
	
	/* feed properties used for updating */
	GTimeVal	scheduledUpdate;	/**< time at which the feed needs to be updated */
	gboolean	updateRequested;	/**< flag set when update in progress */
	gpointer	*request;		/**< update request structure */
} *feedPtr;

/* ------------------------------------------------------------ */
/* feed handler interface					*/
/* ------------------------------------------------------------ */

/** a function which parses the feed data given with the feed ptr fp */
typedef void 	(*readFeedFunc)		(feedPtr fp, gchar *data);

// FIXME: remove this structure...
typedef struct feedHandler {
	readFeedFunc		readFeed;	/**< feed type parse function */
	gboolean		merge;		/**< flag if feed type supports merging */
} *feedHandlerPtr;

/* ------------------------------------------------------------ */
/* feed creation/modification interface				*/
/* ------------------------------------------------------------ */

void feed_init(void);

feedPtr feed_new(void);
feedPtr newFeed(gint type, gchar *url, gchar *keyprefix);
feedPtr addFeed(gint type, gchar *url, gchar *key, gchar *keyprefix, gchar *feedname, gint interval);
void feed_merge(feedPtr old_fp, feedPtr new_fp);
void feed_remove(feedPtr fp);
void feed_update(feedPtr fp);
void feed_save(feedPtr fp);

void feed_add_item(feedPtr fp, itemPtr ip);

void feed_copy(feedPtr fp, feedPtr new_fp);
void feed_free(feedPtr fp);

/* ------------------------------------------------------------ */
/* feed property get/set 					*/
/* ------------------------------------------------------------ */

feedPtr feed_get_from_key(gchar *key);

gpointer feed_get_favicon(feedPtr fp);
gint feed_get_type(feedPtr fp);
gchar * feed_get_key(feedPtr fp);
gchar * feed_get_keyprefix(feedPtr fp);

void feed_increase_unread_counter(feedPtr fp);
void feed_decrease_unread_counter(feedPtr fp);
gint feed_get_unread_counter(feedPtr fp);

gint feed_get_default_update_interval(feedPtr fp);
gint feed_get_update_interval(feedPtr fp);
void feed_set_update_interval(feedPtr fp, gint interval);
void feed_reset_update_counter(feedPtr fp);
gboolean feed_get_available(feedPtr fp);

/**
 * Returns a HTML string describing the last retrieval error 
 * of this feed. Should only be called when getFeedAvailable
 * returns FALSE. Caller must free returned string! 
 *
 * @return HTML error description
 */
gchar * feed_get_error_description(feedPtr fp);

gchar * feed_get_title(feedPtr fp);
void feed_set_title(feedPtr fp, gchar * title);

gchar * feed_get_description(feedPtr fp);

gchar * feed_get_source(feedPtr fp);
void feed_set_source(feedPtr fp, gchar * source);

GSList * feed_get_item_list(feedPtr fp);
void feed_clear_item_list(feedPtr fp);

void feed_mark_all_items_read(feedPtr fp);

#endif
