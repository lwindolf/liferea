/*
   common feed (channel) handling
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _FEED_H
#define _FEED_H

#include <glib.h>
#include <gtk/gtk.h>
#include "item.h"

/* ------------------------------------------------------------ */
/* feed list view entry types (FS_TYPE) 			*/
/* ------------------------------------------------------------ */
#define FST_INVALID	0
#define FST_NODE	1	/* the folder type */
#define FST_RSS		3	/* generic RSS */
#define FST_OCS		4	/* OCS directories */
#define FST_CDF		5	/* Microsoft CDF */
#define FST_PIE		6	/* Atom/Echo/PIE */
#define FST_OPML	7	/* generic OPML */

#define FST_VFOLDER	9	/* sepcial type for VFolders */

#define FST_HELPNODE	50	/* special tree list types to store help feeds */	
#define FST_HELPFEED	51	/* special type to allow updating of help feed url */

#define FST_EMPTY	100	/* special type for "(empty)" entry */

#define FST_AUTODETECT	200	/* special type to enforce type auto detection */

/* macro to test wether a type is a ressource which is regularily updated */
#define IS_FEED(type)		((FST_RSS == type) || \
				 (FST_CDF == type) || \
				 (FST_PIE == type) || \
				 (FST_OPML == type) || \
				 (FST_HELPFEED == type))

/* macro to test wether a type is a ressource which not regularily updated */				 
#define IS_DIRECTORY(type)	(FST_OCS == type)
				 
/* macro to test wether a type is only a tree list structure entry */
#define IS_NODE(type)		((FST_NODE == type) || (FST_HELPNODE == type))

/* macro to test if feed menu action can be applied to this entry */
#define FEED_MENU(type)		(IS_FEED(type) || IS_DIRECTORY(type))

/* ------------------------------------------------------------ */
/* feed handler interface					*/
/* ------------------------------------------------------------ */

/* common structure to access feed info structures */
typedef struct feed {
	gint		type;			/* feed type */
	gchar		*key;
	gchar		*keyprefix;
	gint		unreadCount;		/* number of unread items */
	gint		defaultInterval;	/* update interval as specified by the feed */
	gint		updateCounter;		/* minutes till next auto-update */
	gboolean	available;		/* flag to signalize loading errors */
	
	gchar		*data;		/* raw XML data, used while downloading/parsing the feed */
	gchar		*parseErrors;	/* textual/HTML description of parsing errors */
	
	gpointer	icon;		/* pointer to pixmap, if theres a favicon */
		
	/* feed properties needed to be saved */
	gchar		*title;		/* feed/channel title */
	gchar		*description;	/* HTML string describing the feed */
	gchar		*source;	/* feed source, FIXME: needed??? */
	gint		updateInterval;	/* user defined update interval */

	GSList		*items;		/* list of pointers to the item structures of this channel */
	
	GSList		*filter;	/* list of filters applied to this feed */
	
	/* feed properties used for updating */
	gpointer	*request;
} *feedPtr;

/* ------------------------------------------------------------ */
/* feed handler interface					*/
/* ------------------------------------------------------------ */

/* a function which parses the feed data given with the feed ptr fp */
typedef void 	(*readFeedFunc)		(feedPtr fp);

typedef struct feedHandler {
	readFeedFunc		readFeed;
	gboolean		merge;		/* flag if feed type supports merging */
} *feedHandlerPtr;

/* ------------------------------------------------------------ */
/* feed creation/modification interface				*/
/* ------------------------------------------------------------ */

void initBackend(void);

void initFeedTypes(void);
void registerFeedType(gint type, feedHandlerPtr fhp);

feedPtr getNewFeedStruct(void);
feedPtr newFeed(gint type, gchar *url, gchar *keyprefix);
feedPtr addFeed(gint type, gchar *url, gchar *key, gchar *keyprefix, gchar *feedname, gint interval);
void mergeFeed(feedPtr old_fp, feedPtr new_fp);
void removeFeed(feedPtr fp);
void updateFeed(feedPtr fp);
gint saveFeed(feedPtr fp);
void saveAllFeeds(void);

void addItem(feedPtr fp, itemPtr ip);

void copyFeed(feedPtr fp, feedPtr new_fp);
void freeFeed(feedPtr fp);

/* ------------------------------------------------------------ */
/* feed property get/set 					*/
/* ------------------------------------------------------------ */

feedPtr getFeed(gchar *key);

gpointer getFeedIcon(feedPtr fp);
gint getFeedType(feedPtr fp);
gchar * getFeedKey(feedPtr fp);
gchar * getFeedKeyPrefix(feedPtr fp);

void increaseUnreadCount(feedPtr fp);
void decreaseUnreadCount(feedPtr fp);
gint getFeedUnreadCount(feedPtr fp);

gint getFeedDefaultInterval(feedPtr fp);
gint getFeedUpdateInterval(feedPtr fp);
void setFeedUpdateInterval(feedPtr fp, gint interval);
gint getFeedUpdateCounter(feedPtr fp);
void setFeedUpdateCounter(feedPtr fp, gint count);
gboolean getFeedAvailable(feedPtr fp);

/* Returns a HTML string describing the last retrieval error 
   of this feed. Should only be called when getFeedAvailable
   returns FALSE. Caller must free returned string! */
gchar * getFeedErrorDescription(feedPtr fp);

/* Returns a HTML string describing the last retrieval error 
   of this feed. Should only be called when getFeedAvailable
   returns FALSE. Caller must free returned string! */
gchar * getFeedErrorDescription(feedPtr fp);

gchar * getFeedTitle(feedPtr fp);
void setFeedTitle(feedPtr fp, gchar * title);

gchar * getFeedDescription(feedPtr fp);

gchar * getFeedSource(feedPtr fp);
void setFeedSource(feedPtr fp, gchar * source);

GSList * getFeedItemList(feedPtr fp);
void clearFeedItemList(feedPtr fp);

#endif
