/*
   RSS channel parsing
      
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

#ifndef _RSS_CHANNEL_H
#define _RSS_CHANNEL_H

#include <time.h>
#include "feed.h"

/* all versions */
#define RSS_CHANNEL_TITLE		0
#define RSS_CHANNEL_DESCRIPTION		1
#define RSS_CHANNEL_LINK		2
#define RSS_CHANNEL_IMAGE		3
/* since 0.91 and in <dc:rights> */
#define RSS_CHANNEL_COPYRIGHT		4
/* in 0.91, 0.92, 2.0 and <dc:language> */
#define RSS_CHANNEL_LANGUAGE		5
/* in 0.91, 0.92, 2.0 */
#define RSS_CHANNEL_LASTBUILDDATE	6
#define RSS_CHANNEL_PUBDATE		7
/* in 0.91, 0.92, 2.0 and <dc:publisher> */
#define RSS_CHANNEL_WEBMASTER		8
/* in 0.91, 0.92, 2.0 and <dc:creator> */
#define RSS_CHANNEL_MANAGINGEDITOR	9
/* 0.92, 2.0 and <dc:subject> */
#define RSS_CHANNEL_CATEGORY		10

#define RSS_CHANNEL_MAX_TAG		11

typedef struct RSSChannel {		
	/* standard namespace infos */
	gchar		*tags[RSS_CHANNEL_MAX_TAG];

	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */
	
	gchar		*tiTitle;	/* text input title */
	gchar		*tiDescription;	/* text input description */
	gchar		*tiName;	/* text input object name */
	gchar		*tiLink;	/* text input HTTP GET destination URL */
	
	time_t		time;		/* last feed build/creation time */	
	gint		updateInterval;	/* channel defined feed refresh interval */
} *RSSChannelPtr;

feedHandlerPtr	initRSSFeedHandler(void);

#endif
