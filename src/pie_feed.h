/*
   PIE 0.2 feed parsing
      
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

#ifndef _PIE_FEED_H
#define _PIE_FEED_H

#include <time.h>
#include "feed.h"
#include "pie_entry.h"

#define PIE_FEED_TITLE			0
#define PIE_FEED_DESCRIPTION		1
#define PIE_FEED_LINK			2
#define PIE_FEED_COPYRIGHT		4
#define PIE_FEED_GENERATOR		5
#define PIE_FEED_PUBDATE		6

#define PIE_FEED_MAX_TAG		7

typedef struct PIEFeed {
	/* standard namespace infos */
	gchar		*author;	/* author of the feed */
	gchar		*contributors;	/* list of contributors */
	gchar		*tags[PIE_FEED_MAX_TAG];

	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */
					   
	/* other information */
	time_t		time;		/* last feed build/creation time */	
	gint		updateInterval;	/* feed refresh interval */
} *PIEFeedPtr;

feedHandlerPtr	initPIEFeedHandler(void);

#endif
