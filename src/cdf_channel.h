/*
   CDF channel parsing
      
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

#ifndef _CDF_CHANNEL_H
#define _CDF_CHANNEL_H

#include "cdf_item.h"

/* all versions */
#define CDF_CHANNEL_TITLE		0
#define CDF_CHANNEL_DESCRIPTION		1
#define CDF_CHANNEL_IMAGE		2
#define CDF_CHANNEL_COPYRIGHT		3
#define CDF_CHANNEL_PUBDATE		4
#define CDF_CHANNEL_WEBMASTER		5
#define CDF_CHANNEL_CATEGORY		6

#define CDF_CHANNEL_MAX_TAG		7

typedef struct CDFChannel {
	/* common attributes, order and position important ! */
	gint		type;		/* FST_FEED for RSS channels */
	gchar		*key;		/* configuration storage key */	
	gchar		*keyprefix;	
	gchar		*usertitle;	/* feed title may be modified by user */	
	gchar 		*source;	/* source url */
	gboolean	available;	/* flag to signalize load/update errors */
	gint		updateCounter;	/* counter of minutes till next feed refresh */
				
	/* standard namespace infos */
	gchar		*tags[CDF_CHANNEL_MAX_TAG];

	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */
					   
	CDFItemPtr 	items;		/* the item list */
	
	/* other information */
	gchar		*time;		/* last feed build/creation time */	
	gchar 		*encoding;	/* xml encoding */
	gint		updateInterval;	/* feed refresh interval */
	gint		unreadCounter;	/* counter of unread items */
} *CDFChannelPtr;

/* -------------------------------------------------------- */
/* CDF only methods used during the HTML processing	    */
/* -------------------------------------------------------- */

gchar * 	getCDFFeedTag(gpointer cp, int tag);
gint		getCDFFeedUnreadCount(gchar *feedkey);

/* -------------------------------------------------------- */
/* CDF read/update methods				    */
/* -------------------------------------------------------- */

CDFChannelPtr readCDFFeed(gchar *url);
CDFChannelPtr mergeCDFFeed(CDFChannelPtr old_cp, CDFChannelPtr new_cp);

#endif
