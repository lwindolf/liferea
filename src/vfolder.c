/*
   VFolder functionality
   
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

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

#include <sys/time.h>
#include <string.h> /* For memset() */
#include "conf.h"
#include "support.h"
#include "common.h"
#include "vfolder.h"

#include "htmlview.h"

/* HTML constants */
#define FEED_HEAD_VFOLDER	"VFolder: "

/* FIXME: stat() ! */
#define MAXBUFSIZE	1024

extern GMutex 		*feeds_lock;
extern GHashTable	*feeds;

/* though VFolders are treated like feeds, there 'll be a read() call
   when creating a new VFolder, we just do nothing but initializing
   the vfolder structure */
static feedPtr readVFolder(gchar *url) {
	feedPtr	vp;
	
	/* initialize channel structure */
	vp = g_new0(struct feed, 1);
	vp->type = FST_VFOLDER;
	vp->title = url;
	
	return vp;
}

/* ---------------------------------------------------------------------------- */
/* backend interface to search other feeds					*/
/* ---------------------------------------------------------------------------- */
void removeOldItemsFromVFolder(feedPtr vp, feedPtr ep) {
	GSList		*list, *newlist = NULL;
	itemPtr		ip;
	
	g_assert(NULL != vp);
	list = vp->items;
	
	while(NULL != list) {
		ip = list->data;
		if(ip->fp != ep)
			newlist = g_slist_append(newlist, ip);
		else
			g_free(ip);
			
		list = g_slist_next(list);
	}
	g_slist_free(vp->items);
	vp->items = newlist;
}

/* adds an item to this VFolder, this method is called
   when a VFolder scan method of a feed found a matching item */
void addItemToVFolder(feedPtr vp, itemPtr ip) {
	g_assert(NULL != vp);
	g_assert(NULL != ip);

	if(FALSE == getItemReadStatus(ip))
		feed_increase_unread_counter(vp);
	vp->items = g_slist_append(vp->items, ip);
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* writes VFolder HTML description */
static void showVFolderInfo(feedPtr vp) {
/*	g_assert(vp != NULL);	
	
	startHTMLOutput(TRUE);
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	writeHTML(FEED_HEAD_START);
	writeHTML(FEED_HEAD_VFOLDER);
	writeHTML(getFeedTitle(vp));
	writeHTML(FEED_HEAD_END);	

	finishHTMLOutput();*/
}

/* ---------------------------------------------------------------------------- */
/* handler structures								*/
/* ---------------------------------------------------------------------------- */

feedHandlerPtr initVFolderFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);

	/* prepare feed handler structure */
	//fhp->readFeed		= readVFolder;
	fhp->merge		= FALSE;	/* if that happens its much to late... */
	
	return fhp;
}

/* ---------------------------------------------------------------------------- */
/* vfolder handling functions							*/
/* ---------------------------------------------------------------------------- */

/* called when a feed is deleted, whose items are in a the vfolder fp */
void removeItemFromVFolder(feedPtr fp, itemPtr ip) {

	g_slist_remove(fp->items, (gpointer)ip);
}
