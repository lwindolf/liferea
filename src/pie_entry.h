/*
   PIE 0.2 entry tag parsing 
      
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

#ifndef _PIE_ENTRY_H
#define _PIE_ENTRY_H

#include <time.h>
#include <glib.h>
#include "pie_feed.h"

#define PIE_ENTRY_TITLE			0
#define PIE_ENTRY_DESCRIPTION		1
#define PIE_ENTRY_LINK			2
#define PIE_ENTRY_COPYRIGHT		3
#define PIE_ENTRY_PUBDATE		4

#define PIE_ENTRY_MAX_TAG		5

typedef struct PIEEntry {
	gchar		*tags[PIE_ENTRY_MAX_TAG];	/* standard namespace infos */
	gboolean	summary;			/* helper flag, TRUE if PIE_ENTRY_DESCRIPTION contains a <summary> text */
	
	GHashTable	*nsinfos;	/* list to store pointers to namespace
					   specific informations */	
	gchar		*author;
	gchar		*contributors;
	time_t		time;
} *PIEEntryPtr;

itemPtr parseEntry(gpointer cp, xmlDocPtr doc, xmlNodePtr cur);

#endif
