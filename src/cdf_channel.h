/*
   CDF channel parsing
      
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

#ifndef _CDF_CHANNEL_H
#define _CDF_CHANNEL_H

#include "backend.h"

#define CDF_CHANNEL_TITLE		0
#define CDF_CHANNEL_DESCRIPTION		1
#define CDF_CHANNEL_IMAGE		2
#define CDF_CHANNEL_COPYRIGHT		3
#define CDF_CHANNEL_PUBDATE		4
#define CDF_CHANNEL_WEBMASTER		5
#define CDF_CHANNEL_CATEGORY		6

#define CDF_CHANNEL_MAX_TAG		7

typedef struct CDFChannel {
	/* type, key and keyprefix HAVE TO BE THE FIRST elements of 
	   this structure, order is important! */
	gint		type;		/* FST_CDF for CDF channels */
	gchar		*key;		/* configuration storage key */	
	gchar		*keyprefix;	/* folder key the feed is stored in */

	/* standard namespace infos */
	gchar		*tags[CDF_CHANNEL_MAX_TAG];

	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */
					   
	GSList		*items;		/* the item list */
	
	/* other information */
	gchar		*usertitle;	/* feed title may be modified by user */	
	gchar 		*source;	/* source url */	
	gboolean	available;	/* flag to signalize load/update errors */
	time_t		time;		/* last feed build/creation time */	
	gchar 		*encoding;	/* xml encoding */
	gint		updateInterval;	/* feed refresh interval */
	gint		updateCounter;	/* counter of minutes till next feed refresh */	
	gint		unreadCounter;	/* counter of unread items */
} *CDFChannelPtr;

feedHandlerPtr initCDFFeedHandler(void);

#endif
