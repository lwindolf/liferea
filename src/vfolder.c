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

/* wrapper structure to store item information */
typedef struct VFolderItem {
	gint		type;	/* feed type this item belongs to */
	gpointer	ip;	/* the item pointer itself */
	gpointer	ep;	/* the feed this item belongs to */
	gpointer	vp;	/* the vfolder this vfolder entry belongs to */
} *VFolderItemPtr;

/* we reuse the backend item handlers to redirect item handler calls
   to the real item handlers */
extern GHashTable	*itemHandler;

/* FIXME: stat() ! */
#define MAXBUFSIZE	1024

/* loads a saved VFolder feed from disk */
static gpointer loadVFolder(gchar *keyprefix, gchar *key) {
	VFolderPtr	new_vp = NULL;
	rulePtr		rule = NULL;
	char		*buf, *filename;
	FILE		*f;

	if(NULL == (new_vp = (VFolderPtr) malloc(sizeof(struct VFolder)))) {
		g_error(_("could not allocate memory!\n"));
		return NULL;
	}

	memset(new_vp, 0, sizeof(struct VFolder));
	new_vp->type = FST_VFOLDER;
	new_vp->itemtypes = g_hash_table_new(g_int_hash, g_int_equal);

	filename = getCacheFileName(keyprefix, key, "vfolder");	
	if(NULL != (f = fopen(filename, "r"))) {
		buf = (gchar *)g_malloc(MAXBUFSIZE + 1);
		rule = (rulePtr)g_malloc(sizeof(struct rule));
		
		if((NULL != buf) && (NULL != rule)) {
			memset(buf, 0, MAXBUFSIZE + 1);
			fread(buf, MAXBUFSIZE, 1, f);
			rule->value = buf;
			new_vp->rules = rule;
		} else {
			g_error(_("could not allocate memory!\n"));
		}
		fclose(f);	
	} else {
		g_warning(_("could not read VFolder save file!!!\n"));
	}
	
	g_free(filename);
	return (gpointer)new_vp;
}

/* though VFolders are treated like feeds, there 'll be a read() call
   when creating a new VFolder, we just do nothing but initializing
   the vfolder structure */
static gpointer readVFolder(gchar *url) {
	VFolderPtr 	vp;
	
	/* initialize channel structure */
	if(NULL == (vp = (VFolderPtr) malloc(sizeof(struct VFolder)))) {
		g_error("not enough memory!\n");
		return NULL;
	}
	memset(vp, 0, sizeof(struct VFolder));
	vp->type = FST_VFOLDER;
	vp->usertitle = url;
	vp->itemtypes = g_hash_table_new(g_int_hash, g_int_equal);
	
	return vp;
}

/* merging does nothing for VFolders, should never be called */
static gpointer mergeVFolder(gpointer old_fp, gpointer new_fp) {
	VFolderPtr	old_vp = (VFolderPtr)old_fp;
	
	g_free(old_vp->key);
	g_free(old_vp->keyprefix);
	g_free(old_vp->usertitle);
	// FIXME: free item list memory
	g_slist_free(old_vp->items);
	g_free(old_vp);
	g_print(_("hmmm... maybe this should not happen!"));
	return new_fp;
}

static void removeVFolder(gchar *keyprefix, gchar *key, gpointer fp) {
	VFolderPtr	vp = (VFolderPtr)fp;
	gchar		*filename;

	/* never free key and keyprefix, this is done by backend! */	
	g_free(vp->usertitle);
	// FIXME: free item list memory
	g_slist_free(vp->items);
	g_free(vp);

	filename = getCacheFileName(keyprefix, key, "vfolder");
	g_print("deleting cache file %s\n", filename);
	if(0 != unlink(filename)) {
		showErrorBox(_("could not remove cache file of this entry!"));
	}
	g_free(filename);
}

/* ---------------------------------------------------------------------------- */
/* VFolder item handler								*/
/* ---------------------------------------------------------------------------- */

static gpointer getVFolderItemProp(gpointer ip, gint proptype) {
	VFolderItemPtr	vip = (VFolderItemPtr)ip;
	itemHandlerPtr	ihp;
	
	g_assert(NULL != vip);
	if(NULL == (ihp = g_hash_table_lookup(itemHandler, (gpointer)&vip->type)))
		g_error(g_strdup_printf(_("internal error! no item handler for this type %d!"), vip->type));	

	g_assert(NULL != ihp->getItemProp);
	return (*(ihp->getItemProp))(vip->ip, proptype);
}

static void setVFolderItemProp(gpointer ip, gint proptype, gpointer data) {
	VFolderItemPtr	vip = (VFolderItemPtr)ip;
	itemHandlerPtr	ihp;
	
	g_assert(NULL != vip);
	if(NULL == (ihp = g_hash_table_lookup(itemHandler, (gpointer)&vip->type)))
		g_error(g_strdup_printf(_("internal error! no item handler for this type %d!"), vip->type));	
		
	g_assert(NULL != ihp->setItemProp);
	g_assert(NULL != ihp->getItemProp);
	switch(proptype) {
		case ITEM_PROP_READSTATUS:
			if(FALSE == (gboolean)(*(ihp->getItemProp))(vip->ip, proptype))
				((VFolderPtr)(vip->vp))->unreadCounter--;
			break;
	}
	(*(ihp->setItemProp))(vip->ip, proptype, data);
}

static void showVFolderItem(gpointer ip) {
	VFolderItemPtr	vip = (VFolderItemPtr)ip;
	itemHandlerPtr	ihp;

	g_assert(NULL != vip);
	if(NULL == (ihp = g_hash_table_lookup(itemHandler, (gpointer)&vip->type)))
		g_error(g_strdup_printf(_("internal error! no item handler for this type %d!"), vip->type));	

	(*(ihp->showItem))(vip->ip);
}

/* ---------------------------------------------------------------------------- */
/* backend interface to search other feeds					*/
/* ---------------------------------------------------------------------------- */
void removeOldItemsFromVFolder(VFolderPtr vp, gpointer ep) {
	GSList		*list, *newlist = NULL;
	VFolderItemPtr	vip;
	
	g_assert(NULL != vp);
	list = vp->items;
	
	while(NULL != list) {
		vip = list->data;
		if(vip->ep != ep)
			newlist = g_slist_append(newlist, vip);
		else
			g_free(vip);
			
		list = g_slist_next(list);
	}
	g_slist_free(vp->items);
	vp->items = newlist;
}

/* adds an item to this VFolder, this method is called
   when a VFolder scan method of a feed found a matching item */
void addItemToVFolder(VFolderPtr vp, gpointer ep, gpointer ip, gint type) {
	VFolderItemPtr	vip = NULL;
	itemHandlerPtr	ihp;

	g_assert(NULL != vp);
	g_assert(NULL != ip);

	if(NULL == (ihp = g_hash_table_lookup(itemHandler, (gpointer)&type)))
		g_error(g_strdup_printf(_("internal error! no item handler for this type %d!"), type));	

	if(NULL != (vip = g_malloc(sizeof(struct VFolderItem)))) {
		vip->type = type;
		vip->ip = ip;
		vip->ep = ep;
		vip->vp = vp;
		g_assert(NULL != (*(ihp->getItemProp)));
		if(FALSE == (*(ihp->getItemProp))(ip, ITEM_PROP_READSTATUS))
			vp->unreadCounter++;
		vp->items = g_slist_append(vp->items, vip);
	} else {
		g_error(_("could not allocate memory!"));
	}
}

void setVFolderRules(VFolderPtr vp, rulePtr rp) {
	FILE	*f;
	gchar	*filename;
	
	vp->rules = rp; 
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

rulePtr getVFolderRules(VFolderPtr vp) { return vp->rules; }

/* applies the rules of the VFolder vp to the parameter string,
   the function returns TRUE if the rules were matched, otherwise
   FALSE */
gboolean matchVFolderRules(VFolderPtr vp, gchar *string) {
	
	/* do a simple strcmp() */
	if(NULL == string) {
		g_print("matchVFolderRules() with NULL string\n");
		return FALSE;
	}
	
	g_assert(NULL != vp);
	
	if(NULL == vp->rules) {
		g_warning(_("internal error! VFolder has no rules!"));
		return FALSE;
	}
	
	g_assert(NULL != vp->rules->value);

	if(NULL != strstr(string, vp->rules->value)) {
		return TRUE;
	}
	return FALSE;
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* writes VFolder HTML description */
static void showVFolderInfo(gpointer fp) {
	VFolderPtr	vp = (VFolderPtr)fp;

	g_assert(vp != NULL);	
	
	startHTMLOutput();
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	writeHTML(FEED_HEAD_START);
	writeHTML(FEED_HEAD_VFOLDER);
	writeHTML(vp->usertitle);
	writeHTML(FEED_HEAD_END);	

	finishHTMLOutput();
}

/* ---------------------------------------------------------------------------- */
/* just some encapsulation 							*/
/* ---------------------------------------------------------------------------- */

static void setVFolderProp(gpointer fp, gint proptype, gpointer data) {
	VFolderPtr	c = (VFolderPtr)fp;
	
	if(NULL != c) {
		g_assert(FST_VFOLDER == c->type);
		switch(proptype) {
			case FEED_PROP_TITLE:
			case FEED_PROP_USERTITLE:
				g_free(c->usertitle);
				c->usertitle = (gchar *)data;
				break;
			case FEED_PROP_SOURCE:
				/* we don't need this, but cannot suppress the setting
				   by conf.c */
				g_free(data);
				break;
			case FEED_PROP_DFLTUPDINTERVAL:
			case FEED_PROP_UPDATEINTERVAL:
			case FEED_PROP_UPDATECOUNTER:
				break;
			case FEED_PROP_UNREADCOUNT:
			case FEED_PROP_AVAILABLE:
			case FEED_PROP_ITEMLIST:		
				g_error("internal error! please don't do this!");
				break;
			default:
				g_error(g_strdup_printf(_("internal error! unknow feed property type %d!\n"), proptype));
				break;
		}
	}
}

static gpointer getVFolderProp(gpointer fp, gint proptype) {
	VFolderPtr	c = (VFolderPtr)fp;

	if(NULL != c) {
		g_assert(FST_VFOLDER == c->type);
		switch(proptype) {
			case FEED_PROP_TITLE:
			case FEED_PROP_USERTITLE:
				return (gpointer)c->usertitle;
				break;
			case FEED_PROP_SOURCE:
				return NULL;
				break;
			case FEED_PROP_DFLTUPDINTERVAL:
			case FEED_PROP_UPDATEINTERVAL:
			case FEED_PROP_UPDATECOUNTER:
				return (gpointer)-1;
				break;
			case FEED_PROP_UNREADCOUNT:
				return (gpointer)c->unreadCounter;
				break;
			case FEED_PROP_AVAILABLE:
				return (gpointer)TRUE;
				break;
			case FEED_PROP_ITEMLIST:
				return (gpointer)c->items;
				break;
			default:
				g_error(g_strdup_printf(_("internal error! unknow feed property type %d!\n"), proptype));
				break;
		}
	} else {
		return NULL;
	}
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
	fhp->loadFeed		= loadVFolder;
	fhp->readFeed		= readVFolder;
	fhp->mergeFeed		= mergeVFolder;
	fhp->removeFeed		= removeVFolder;
	fhp->getFeedProp	= getVFolderProp;	
	fhp->setFeedProp	= setVFolderProp;
	fhp->showFeedInfo	= showVFolderInfo;
	fhp->doVFolderScan	= NULL;	/* we are a VFolder, *WE* scan! */
	
	return fhp;
}

itemHandlerPtr initVFolderItemHandler(void) {
	itemHandlerPtr	ihp;
	
	if(NULL == (ihp = (itemHandlerPtr)g_malloc(sizeof(struct itemHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(ihp, 0, sizeof(struct itemHandler));

	/* prepare item handler structure */
	ihp->getItemProp	= getVFolderItemProp;	
	ihp->setItemProp	= setVFolderItemProp;
	ihp->showItem		= showVFolderItem;
	
	return ihp;
}
