/*
   RSS channel parsing
      
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef _RSS_CHANNEL_H
#define _RSS_CHANNEL_H

#include "rss_item.h"

/* all versions */
#define CHANNEL_TITLE		0
#define CHANNEL_DESCRIPTION	1
#define CHANNEL_LINK		2
#define CHANNEL_IMAGE		3
/* since 0.91 and in <dc:rights> */
#define CHANNEL_COPYRIGHT	4
/* in 0.91, 0.92, 2.0 and <dc:language> */
#define CHANNEL_LANGUAGE	5
/* in 0.91, 0.92, 2.0 */
#define CHANNEL_LASTBUILDDATE	6
#define CHANNEL_PUBDATE		7
/* in 0.91, 0.92, 2.0 and <dc:publisher> */
#define CHANNEL_WEBMASTER	8
/* in 0.91, 0.92, 2.0 and <dc:creator> */
#define CHANNEL_MANAGINGEDITOR	9
/* 0.92, 2.0 and <dc:subject> */
#define CHANNEL_CATEGORY	10

#define CHANNEL_MAX_TAG		11

typedef struct channel {
	/* common attributes, order and position important ! */
	gint		type;		/* FST_FEED for RSS channels */
	gchar		*key;		/* configuration storage key */	
	gchar		*keyprefix;	
	gchar		*usertitle;	/* feed title may be modified by user */	
	gchar 		*source;	/* source url */
	gboolean	available;	/* flag to signalize load/update errors */
	gint		updateCounter;	/* counter of minutes till next feed refresh */
				
	/* standard namespace infos */
	gchar		*tags[CHANNEL_MAX_TAG];

	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */
					   
	itemPtr 	items;		/* the item list */
	
	/* other information */
	gchar		*time;		/* last feed build/creation time */	
	gchar 		*encoding;	/* xml encoding */
	gint		updateInterval;	/* feed refresh interval */
	gint		unreadCounter;	/* counter of unread items */
} *channelPtr;

/* -------------------------------------------------------- */
/* RSS only methods used during the HTML processing	    */
/* -------------------------------------------------------- */

GHashTable * 	getFeedNsHandler(gpointer cp);	/* returns the Hashtable with the namespace I/O-handlers */
gchar * 	getFeedTag(gpointer cp, int tag);
gint		getRSSFeedUnreadCount(gchar *feedkey);

/* -------------------------------------------------------- */
/* RSS read/update methods				    */
/* -------------------------------------------------------- */

channelPtr readRSSFeed(gchar *url);
channelPtr mergeRSSFeed(channelPtr old_cp, channelPtr new_cp);

#endif
