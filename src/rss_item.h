/*
   RSS item tag parsing 
      
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

#ifndef _RSS_ITEM_H
#define _RSS_ITEM_H

#include <glib.h>

/* all RSS versions */
#define ITEM_TITLE		0
#define ITEM_DESCRIPTION	1
#define ITEM_LINK		2
/* 2.0 and <dc:creator> */
#define ITEM_AUTHOR		3
/* 2.0 and Annotate-Module */
#define ITEM_COMMENTS		4
/* 0.92, 2.0 */
#define ITEM_ENCLOSURE		5
/* 0.92, 2.0 and <dc:subject> */
#define ITEM_CATEGORY		6
	
#define ITEM_MAX_TAG		7

typedef struct item {
	gpointer	cp;		/* the channel structure this item belongs to,
					   must be the first element of the structure! */
	
	gchar		*tags[ITEM_MAX_TAG];	/* standard namespace infos */
	
	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */	
	gboolean	read;
	gchar		*time;
	struct item	*next;
} *itemPtr;

/* -------------------------------------------------------- */
/* functions to access properties of the item structure     */
/* -------------------------------------------------------- */

gboolean getRSSItemReadStatus(gpointer ip);

gchar * getRSSItemTag(gpointer ip, int tag);

void	markRSSItemAsRead(gpointer ip);

#endif
