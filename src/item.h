/*
   common item handling
   
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

#ifndef _ITEM_H
#define _ITEM_H

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <time.h>

/* ------------------------------------------------------------ */
/* item interface						*/
/* ------------------------------------------------------------ */

#define ITEM_PROP_TITLE			0
#define ITEM_PROP_READSTATUS		1
#define ITEM_PROP_DESCRIPTION		2
#define ITEM_PROP_SOURCE		3
#define ITEM_PROP_ID			4
#define ITEM_PROP_TIME			5
#define ITEM_PROP_TYPE			6

typedef struct item {
	/* common attributes accessed by the GUI */
	gchar		*title;		
	gboolean 	readStatus;	/* FALSE if item was not yet read */
	gboolean 	marked;		
	gchar		*description;	/* HTML string containing the item infos */	
	gchar		*source;	/* item link */
	gchar		*id;		/* depending on the feed type, a unique item identifier like <guid> for RSS */
	time_t		time;		/* item time value */
	
	gint		type;		/* type of item */
	gpointer	fp;		/* pointer to the feed this item belongs to */
	GSList		*vfolders;	/* list of vfolders this item appears in */
} *itemPtr;

/* ------------------------------------------------------------ */
/* item handler interface					*/
/* ------------------------------------------------------------ */

typedef void		(*showItemFunc)		(gpointer fp);
/* methods to set/get the ITEM_PROP_* properties */
typedef void 		(*setItemPropFunc)	(gpointer ip, gint proptype, gpointer data);
typedef gpointer	(*getItemPropFunc)	(gpointer ip, gint proptype);

typedef struct itemHandler {
	getItemPropFunc		getItemProp;
	setItemPropFunc		setItemProp;
	showItemFunc		showItem;	// FIXME: remove
} *itemHandlerPtr;

void 	initItemTypes(void);
void 	registerItemType(gint type, itemHandlerPtr ihp);

itemPtr 	getNewItemStruct(void);
gchar *		getItemTitle(itemPtr ip);
gchar *		getItemDescription(itemPtr ip);
gchar *		getItemSource(itemPtr ip);
time_t		getItemTime(itemPtr ip);
gboolean	getItemMark(itemPtr ip);
void		setItemMark(itemPtr ip, gboolean flag);
gboolean	getItemReadStatus(itemPtr ip);
void 		markItemAsRead(itemPtr ip);
void 		markItemAsUnread(itemPtr ip);
void 		addVFolderToItem(itemPtr ip, gpointer fp);
void		removeVFolderFromItem(itemPtr ip, gpointer fp);
void 		displayItem(itemPtr ip);
void		freeItem(itemPtr ip);

/* for cache loading */
itemPtr parseCacheItem(xmlDocPtr doc, xmlNodePtr cur);

#endif
