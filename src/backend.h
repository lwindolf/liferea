/*
   backend interface

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

#ifndef _BACKEND_H
#define _BACKEND_H

#include <gtk/gtk.h>

/* feed list view entry types (FS_TYPE) */
#define FST_INVALID	0
#define FST_NODE	1
#define FST_HELPFEED	2
/* thats the standard RDF Site Summary type */
#define FST_FEED	3	
#define FST_OCS		4
#define FST_CDF		5

#define IS_FEED(type)		((FST_FEED == type) || (FST_CDF == type))

/* constants for attributes in feedstore */
#define FS_TITLE	0
#define FS_URL		1
#define FS_STATE	2
#define FS_KEY		3
#define FS_TYPE		4

/* constants for attributes in itemstore */
#define IS_TITLE	0
#define IS_STATE	1
#define IS_PTR		2
#define IS_TIME		3
#define IS_TYPE		4

/* to connect the data to any views... */
GtkTreeStore * getFeedStore(void);
GtkTreeStore * getItemStore(void);

/* common structure to access channelPtr and directoryPtr */
typedef struct entry {
	/* common attributes, order and position important ! */
	gint		type;		
	gchar		*key;	
	gchar		*keyprefix;
	gchar		*usertitle;	
	gchar 		*source;	
	gboolean	available;	
	gint		updateCounter;
} *entryPtr;

/* ------------------------------------------------------------ */
/* functions to create/change/remove entries			*/
/* ------------------------------------------------------------ */

gchar * newEntry(gint type, gchar *url, gchar *keyprefix);
gchar * addEntry(gint type, gchar *url, gchar *key, gchar *keyprefix, gchar *feedname, gint interval);
void	addFolder(gchar *keyprefix, gchar *title);
void 	removeEntry(gchar *keyprefix, gchar *key);

void	moveUpEntryPosition(gchar *keyprefix, gchar *key);
void	moveDownEntryPosition(gchar *keyprefix, gchar *key);

void 	setEntryTitle(gchar *key, gchar *title);
void 	setEntrySource(gchar *key, gchar *source);

gboolean	getEntryStatus(gchar *key);
gchar *		getEntrySource(gchar *key);
gchar *		getDefaultEntryTitle(gchar *key);	/* returns the title defined by the feed */
gint		getEntryType(gchar *key);

GtkTreeStore * 	getEntryStore(void);

/* -------------------------------------------------------- */
/* RSS only methods used by callbacks           	    */
/* -------------------------------------------------------- */

gint	getFeedUpdateInterval(gchar *feedkey);
gint	getFeedUnreadCount(gchar *feedkey);
void	setFeedUpdateInterval(gchar *feedkey, gint interval);
void	resetAllUpdateCounters(void);
gchar * getHelpFeedKey(void);

/* -------------------------------------------------------- */
/* functions to change items and the item list		    */
/* -------------------------------------------------------- */

void	loadItem(gint type, gpointer ip);
void	loadItemList(gchar *feedkey, gchar *searchstring);
void	markItemAsRead(gint type, gpointer ip);
void	searchItems(gchar *string);
void	clearItemList();

#endif
