/*
   OCS 0.4 support directory tag parsing
   
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

#ifndef _OCS_DIR_H
#define _OCS_DIR_H

#include <glib.h>

/* this is a generic subtag list for directory, channel and format description tags */
#define OCS_TITLE		0
#define OCS_CREATOR		1
#define OCS_DESCRIPTION		2
#define OCS_SUBJECT		3
#define OCS_FORMAT		4
#define OCS_UPDATEPERIOD	5
#define OCS_UPDATEFREQUENCY	6
#define OCS_UPDATEBASE		7
#define OCS_LANGUAGE		8
#define OCS_CONTENTTYPE		9
#define OCS_IMAGE		10
#define OCS_MAX_TAG		11

/* FIXME: maybe using an array for all possible tags in each
   structure is memory wasting :) */

typedef struct format {
	gchar 		*source;		/* source url */
	gchar		*tags[OCS_MAX_TAG];	/* standard namespace infos */	
} *formatPtr;

typedef struct dirEntry {
	gpointer	dp;			/* has to be the first element!!!! */
	gchar 		*source;		/* source url */
	GSList		*formats;		/* list of format structures */
	gchar		*tags[OCS_MAX_TAG];	/* standard namespace infos */	
} *dirEntryPtr;

/* a structure to store OCS feed list information */
typedef struct directory {
	/* common attributes, order and position important ! */
	gint		type;			/* FST_OCS for OCS feed lists */
	gchar		*key;			/* configuration storage key */	
	gchar		*keyprefix;	
	gchar		*usertitle;		/* feed title may be modified by user */	
	gchar 		*source;		/* source url */
	gboolean	available;		/* flag to signalize load/update errors */
	gint		updateCounter;		/* only used as a flag to signalize updating to the update thread */	

	gchar		*tags[OCS_MAX_TAG];	/* standard namespace infos */

	GSList		*items;			/* list of dirEntry structures */
} *directoryPtr;

directoryPtr readOCS(gchar *url);
directoryPtr loadOCS(gchar *filename);

gchar * getOCSDirectorySource(gpointer dp);
gchar * getOCSDirectoryTag(gpointer dp, gint tag);
void setOCSDirectoryTag(gpointer dp, gint tag, gchar *value);

gchar * getOCSDirEntrySource(gpointer dep);
gchar * getOCSDirEntryTag(gpointer dep, gint tag);
void setOCSDirEntryTag(gpointer dep, gint tag, gchar *value);

gchar * getOCSFormatSource(gpointer fp);
gchar * getOCSFormatTag(gpointer fp, gint tag);
void setOCSFormatTag(gpointer fp, gint tag, gchar *value);

#endif
