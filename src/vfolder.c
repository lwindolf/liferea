/*
   VFolder functionality
   
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

#include <sys/time.h>
#include "conf.h"
#include "support.h"
#include "common.h"
#include "vfolder.h"

#include "htmlview.h"

/* HTML constants */
#define FEED_HEAD_VFOLDER	"VFolder: "

/* FIXME: stat() ! */
#define MAXBUFSIZE	1024

extern GMutex * feeds_lock;
extern GHashTable	*feeds;

static GHashTable	*vfolders = NULL;

void initVFolders(void) {
	if(NULL == vfolders)
		vfolders =  g_hash_table_new(g_int_hash, g_int_equal);
}

/* though VFolders are treated like feeds, there 'll be a read() call
   when creating a new VFolder, we just do nothing but initializing
   the vfolder structure */
static feedPtr readVFolder(gchar *url) {
	feedPtr	vp;
	
	/* initialize channel structure */
	if(NULL == (vp = (feedPtr) malloc(sizeof(struct feed)))) {
		g_error("not enough memory!\n");
		return NULL;
	}
	memset(vp, 0, sizeof(struct feed));
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
void addItemToVFolder(feedPtr vp, feedPtr fp, itemPtr ip) {
	gint	type = getFeedType(fp);

	g_assert(NULL != vp);
	g_assert(NULL != ip);

	if(FALSE == getItemReadStatus(ip))
		increaseUnreadCount(vp);
	vp->items = g_slist_append(vp->items, ip);
}

void setVFolderRules(feedPtr vp, rulePtr rp) {
	FILE	*f;
	gchar	*filename;
	
	/* update rule entry in vfolder hash table */
	// FIXME: free older one...
	g_hash_table_insert(vfolders, (gpointer)vp, (gpointer)rp);
	
	/* save rule to VFolder save file */
	filename = getCacheFileName(vp->keyprefix, vp->key, "vfolder");
	if(NULL != (f = fopen(filename, "w"))) {	
		fwrite(rp->value, strlen(rp->value), 1, f);
		fclose(f);	
	} else {
		g_warning(_("could not open VFolder save file for writing!!!\n"));
	}
	
	g_free(filename);
}

rulePtr getVFolderRules(feedPtr vp) { 

	g_assert(NULL != vfolders);
	return (rulePtr)g_hash_table_lookup(vfolders, vp); 
}

/* applies the rules of the VFolder vp to the parameter string,
   the function returns TRUE if the rules were matched, otherwise
   FALSE */
gboolean matchVFolderRules(feedPtr vp, gchar *string) {
	rulePtr		rp;
	
	/* do a simple strcmp() */
	if(NULL == string) {
		g_print("matchVFolderRules() with NULL string\n");
		return FALSE;
	}
	
	g_assert(NULL != vp);
	g_assert(NULL != vfolders);
	rp = (rulePtr)g_hash_table_lookup(vfolders, vp);
	if(NULL == rp) {
		g_warning(_("internal error! VFolder has no rules!"));
		return FALSE;
	}
	
	g_assert(NULL != rp->value);

	if(NULL != strstr(string, rp->value)) {
		return TRUE;
	}
	return FALSE;
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* writes VFolder HTML description */
static void showVFolderInfo(feedPtr vp) {

	g_assert(vp != NULL);	
	
	startHTMLOutput();
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	writeHTML(FEED_HEAD_START);
	writeHTML(FEED_HEAD_VFOLDER);
	writeHTML(getFeedTitle(vp));
	writeHTML(FEED_HEAD_END);	

	finishHTMLOutput();
}

/* ---------------------------------------------------------------------------- */
/* handler structures								*/
/* ---------------------------------------------------------------------------- */

feedHandlerPtr initVFolderFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	if(NULL == (fhp = (feedHandlerPtr)g_malloc(sizeof(struct feedHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(fhp, 0, sizeof(struct feedHandler));
	
	/* prepare feed handler structure */
	fhp->readFeed		= readVFolder;
	fhp->merge		= FALSE;	/* if that happens its much to late... */
	
	return fhp;
}

/* ---------------------------------------------------------------------------- */
/* vfolder handling functions							*/
/* ---------------------------------------------------------------------------- */

/* does the scanning of a feed for loadVFolder(), method is also called 
   by the merge() functions of the feed modules */
void scanFeed(gpointer key, gpointer value, gpointer userdata) {
	feedPtr		fp = (feedPtr)userdata;
	feedPtr		vp = (feedPtr)value;
	GSList		*itemlist = NULL;
	gpointer	ip;
	gchar		*title, *description;
	gboolean	add;

	/* check the type because we are called with a g_hash_table_foreach()
	   but only want to process vfolders ...*/
	if(getFeedType(vp) != FST_VFOLDER) 
		return;
	
	if(getFeedType(fp) == FST_VFOLDER)
		return;	/* don't scan vfolders! */

	if(NULL != fp) {
		itemlist = (GSList *)getFeedItemList(fp);
	} else {
		print_status(_("internal error! item scan for NULL pointer requested!"));
		return;
	}
	
	while(NULL != itemlist) {
		ip = itemlist->data;
		title = getItemTitle(ip);
		description = getItemDescription(ip);
		
		add = FALSE;
		if((NULL != title) && matchVFolderRules(vp, title))
			add = TRUE;

		if((NULL != description) && matchVFolderRules(vp, description))
			add = TRUE;

		if(add) {
			addItemToVFolder(vp, fp, ip);
		}

		itemlist = g_slist_next(itemlist);
	}
}

/* called when a feed is deleted, whose items are in a the vfolder fp */
void removeItemFromVFolder(feedPtr fp, itemPtr ip) {

	g_slist_remove(fp->items, (gpointer)ip);
}

/* scan all feeds for matching any vfolder rules */
void loadVFolder(gpointer key, gpointer value, gpointer userdata) {
	feedPtr	fp = (feedPtr)value;

	/* match the feed ep against all vfolders... */
	if(FST_VFOLDER != getFeedType(fp))
		g_hash_table_foreach(feeds, scanFeed, fp);
}

/* called upon initialization */
void loadVFolders(void) {

	g_mutex_lock(feeds_lock);
	/* iterate all feeds ... */
	g_hash_table_foreach(feeds, loadVFolder, NULL);
	g_mutex_unlock(feeds_lock);

}
