/*
   CDF item tag parsing 
      
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

#ifndef _CDF_ITEM_H
#define _CDF_ITEM_H

#include <time.h>
#include <glib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "cdf_channel.h"

typedef struct CDFItem {
	
	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */	
	time_t		time;
} *CDFItemPtr;

itemPtr parseCDFItem(feedPtr fp, CDFChannelPtr cp, xmlDocPtr doc, xmlNodePtr cur);

#endif
