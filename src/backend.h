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
#define FST_NODE	1	/* the folder type */
#define FST_RSS		3	/* thats the standard RDF Site Summary type */
#define FST_OCS		4	/* OCS directories */
#define FST_CDF		5	/* Microsoft CDF */
#define FST_PIE		6	/* Echo/Atom/PIE */

#define FST_VFOLDER	9	/* sepcial type for VFolders */

#define FST_HELPNODE	50	/* special tree list types to store help feeds */	
#define FST_HELPFEED	51	/* special type to allow updating of help feed url */

#define FST_EMPTY	100	/* special type for "(empty)" entry */

#define IS_FEED(type)		((FST_RSS == type) || (FST_CDF == type) || (FST_PIE == type))
#define IS_NODE(type)		((FST_NODE == type) || (FST_HELPNODE == type))

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

/* common structure to access feed info structures */
typedef struct entry {
	/* type, key and keyprefix HAVE TO BE THE FIRST elements of 
	   this structure, order is important! */
	gint		type;		
	gchar		*key;	
	gchar		*keyprefix;
} *entryPtr;

/* ------------------------------------------------------------ */
/* feed handler interface					*/
/* ------------------------------------------------------------ */

#define FEED_PROP_TITLE			0
#define FEED_PROP_USERTITLE		1
#define FEED_PROP_SOURCE		2
#define FEED_PROP_UPDATEINTERVAL	3
#define FEED_PROP_DFLTUPDINTERVAL	4
#define FEED_PROP_UPDATECOUNTER		5
#define FEED_PROP_UNREADCOUNT		6
#define FEED_PROP_ITEMLIST		7
#define FEED_PROP_AVAILABLE		8

typedef gpointer	(*mergeFeedFunc)	(gpointer old_fp, gpointer new_fp);
typedef gpointer 	(*readFeedFunc)		(gchar *url);
typedef gpointer 	(*loadFeedFunc)		(gchar *keyprefix, gchar *key);
typedef void	 	(*removeFeedFunc)	(gchar *keyprefix, gchar *key, gpointer fp);
typedef void		(*showFeedInfoFunc)	(gpointer fp);
typedef void		(*doVFolderScanFunc)	(gpointer vp);
/* methods to set/get the FEED_PROP_* properties */
typedef void 		(*setFeedPropFunc)	(gpointer fp, gint proptype, gpointer data);
typedef gpointer	(*getFeedPropFunc)	(gpointer fp, gint proptype);

typedef struct feedHandler {
	getFeedPropFunc		getFeedProp;
	setFeedPropFunc		setFeedProp;
	loadFeedFunc		loadFeed;
	readFeedFunc		readFeed;
	mergeFeedFunc		mergeFeed;
	removeFeedFunc		removeFeed;
	showFeedInfoFunc	showFeedInfo;
	doVFolderScanFunc	doVFolderScan;
} *feedHandlerPtr;

/* ------------------------------------------------------------ */
/* item handler interface					*/
/* ------------------------------------------------------------ */

#define ITEM_PROP_TITLE			0
#define ITEM_PROP_READSTATUS		1
#define ITEM_PROP_DESCRIPTION		2
#define ITEM_PROP_TIME			3
#define ITEM_PROP_TYPE			4
#define ITEM_PROP_SOURCE		5

typedef void		(*showItemFunc)		(gpointer fp);
/* methods to set/get the ITEM_PROP_* properties */
typedef void 		(*setItemPropFunc)	(gpointer ip, gint proptype, gpointer data);
typedef gpointer	(*getItemPropFunc)	(gpointer ip, gint proptype);

typedef struct itemHandler {
	getItemPropFunc		getItemProp;
	setItemPropFunc		setItemProp;
	showItemFunc		showItem;
} *itemHandlerPtr;

/* ------------------------------------------------------------ */
/* functions to create/change/remove feed entries		*/
/* ------------------------------------------------------------ */

gchar * newEntry(gint type, gchar *url, gchar *keyprefix);
gchar * addEntry(gint type, gchar *url, gchar *key, gchar *keyprefix, gchar *feedname, gint interval);
void	addFolder(gchar *keyprefix, gchar *title, gint type);
void 	removeEntry(gchar *keyprefix, gchar *key);

gpointer	getFeedProp(gchar *key, gint proptype);
void		setFeedProp(gchar *key, gint proptype, gpointer data);
gchar *		getDefaultEntryTitle(gchar *key);	/* returns the title defined by the feed */
gint		getEntryType(gchar *key);

gchar *		getFolderTitle(gchar *keyprefix);
void		setFolderTitle(gchar *keyprefix, gchar *title);

/* -------------------------------------------------------- */
/* callback interface to access items and the item list	    */
/* -------------------------------------------------------- */

void	loadItem(gint type, gpointer ip);
void	loadItemList(gchar *feedkey, gchar *searchstring);
gchar	*getItemURL(gint type, gpointer ip);
gboolean getItemReadStatus(gint type, gpointer ip);
void	markItemAsRead(gint type, gpointer ip);
void	searchItems(gchar *string);
void	clearItemList();

/* -------------------------------------------------------- */
/* callback interface to access feeds and the feed list	    */
/* -------------------------------------------------------- */

void	moveInEntryList(gchar *oldkeyprefix, gchar *oldkey);
void	resetAllUpdateCounters(void);

/* vfolder interface for GUI */
void	loadNewVFolder(gchar *key, gpointer rp);

/* vfolder interface for updating */
void	scanFeed(gpointer key, gpointer value, gpointer userdata);
void	removeOldItemsFromVFolders(gpointer key, gpointer value, gpointer userdata);

#endif
