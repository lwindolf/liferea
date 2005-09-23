/*
   RSS item tag parsing 
      
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

#ifndef _RSS_ITEM_H
#define _RSS_ITEM_H

#include <time.h>
#include <glib.h>

#include "item.h"
#include "feed.h"
#include "rss_channel.h"

/* all RSS versions */
#define RSS_ITEM_TITLE			0
#define RSS_ITEM_DESCRIPTION		1
#define RSS_ITEM_LINK			2
/* 2.0 and <dc:creator> */
#define RSS_ITEM_AUTHOR			3
/* 2.0 and Annotate-Module */
#define RSS_ITEM_COMMENTS		4
/* 0.92, 2.0 and <dc:subject> */
#define RSS_ITEM_CATEGORY		5
/* 0.92+ */
#define RSS_ITEM_GUID			6
	
#define RSS_ITEM_MAX_TAG		7

typedef struct RSSItem {
	gchar		*tags[RSS_ITEM_MAX_TAG];	/** standard namespace infos */
	gchar		*enclosure;			/** for collecting enclosure informations */
	gchar		*real_source_url;		/** source URL if the item doesn't comes from it's parent feed */
	gchar		*real_source_title;		/** source title if the item doesn't comes from it's parent feed */
	
	time_t		time;				/** timestamp of the item */
} *RSSItemPtr;

itemPtr parseRSSItem(feedPtr fp, xmlNodePtr cur);

#endif
