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

#ifndef _CDF_CDF_ITEM_H
#define _CDF_CDF_ITEM_H

#include <glib.h>

#define CDF_ITEM_TITLE		0
#define CDF_ITEM_DESCRIPTION	1
#define CDF_ITEM_LINK		2
#define CDF_ITEM_AUTHOR		3
#define CDF_ITEM_IMAGE		4
#define CDF_ITEM_CATEGORY	5
	
#define CDF_ITEM_MAX_TAG	6

typedef struct CDFItem {
	gpointer	cp;		/* the channel structure this CDF_ITEM belongs to,
					   must be the first element of the structure! */
	
	gchar		*tags[CDF_ITEM_MAX_TAG];	/* standard namespace infos */
	
	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */	
	gboolean	read;
	time_t		time;
	struct CDFItem	*next;
} *CDFItemPtr;

itemHandlerPtr initCDFItemHandler(void);

#endif
