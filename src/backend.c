/*
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <time.h>
#include <gconf/gconf.h>	// FIXME: remove
#include "support.h"
#include "common.h"
#include "conf.h"

#include "cdf_channel.h"
#include "cdf_item.h"
#include "rss_channel.h"
#include "rss_item.h"
#include "pie_feed.h"
#include "pie_entry.h"
#include "ocs_dir.h"
#include "vfolder.h"

#include "backend.h"
#include "callbacks.h"	

GtkTreeStore	*itemstore = NULL;
GtkTreeStore	*feedstore = NULL;

extern GMutex * feeds_lock;

/* hash table to lookup the tree iterator for each key prefix */
GHashTable	*folders = NULL;

/* hash table to look up feed type handlers */
GHashTable	*feedHandler = NULL;
GHashTable	*itemHandler = NULL;

/* used to lookup a feed/folders pointer specified by a key */
GHashTable	*feeds = NULL;

/* prototypes */
void setInEntryList(entryPtr ep, gchar *feedname, gchar *feedurl, gint type);
void addToEntryList(entryPtr cp);
void saveToFeedList(entryPtr ep);

/* ------------------------------------------------------------------------- */

guint hashFunction(gconstpointer key) {	return (guint)atoi((char *)key); }
gint feedsHashCompare(gconstpointer a, gconstpointer b) { return a-b; }

static registerFeedType(gint type, feedHandlerPtr fhp) {
	gint	*typeptr;
	
	if(NULL != (typeptr = (gint *)g_malloc(sizeof(gint)))) {
		*typeptr = type;
		g_hash_table_insert(feedHandler, (gpointer)typeptr, (gpointer)fhp);
	}
}

static registerItemType(gint type, itemHandlerPtr ihp) {
	gint	*typeptr;
	
	if(NULL != (typeptr = (gint *)g_malloc(sizeof(gint)))) {
		*typeptr = type;
		g_hash_table_insert(itemHandler, (gpointer)typeptr, (gpointer)ihp);
	}
}

/* initializing function, called upon initialization and each
   preference change */
void initBackend() {

	g_mutex_lock(feeds_lock);
	if(NULL == feeds)
		feeds = g_hash_table_new(g_str_hash, g_str_equal);
	g_mutex_unlock(feeds_lock);
		
	if(NULL == folders)
		folders =  g_hash_table_new(g_str_hash, g_str_equal);

	feedHandler = g_hash_table_new(g_int_hash, g_int_equal);
	
	registerFeedType(FST_RSS,	initRSSFeedHandler());
	registerFeedType(FST_HELPFEED,	initRSSFeedHandler());	
	registerFeedType(FST_OCS,	initOCSFeedHandler());
	registerFeedType(FST_CDF,	initCDFFeedHandler());
	registerFeedType(FST_PIE,	initPIEFeedHandler());
	registerFeedType(FST_VFOLDER,	initVFolderFeedHandler());
	
	itemHandler = g_hash_table_new(g_int_hash, g_int_equal);
		
	registerItemType(FST_RSS,	initRSSItemHandler());
	registerItemType(FST_HELPFEED,	initRSSItemHandler());	
	registerItemType(FST_OCS,	initOCSItemHandler());	
	registerItemType(FST_CDF, 	initCDFItemHandler());
	registerItemType(FST_PIE, 	initPIEItemHandler());
	registerItemType(FST_VFOLDER,	initVFolderItemHandler());
}


gpointer getFeedProp(gchar *key, gint proptype) { 
	entryPtr	ep;
	feedHandlerPtr	fhp;
		
	g_mutex_lock(feeds_lock);
	ep = (entryPtr)g_hash_table_lookup(feeds, (gpointer)key);
	g_mutex_unlock(feeds_lock);
	if(NULL != ep) {
		if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&(ep->type))))
			g_error(g_strdup_printf(_("internal error! no feed handler for this type %d!"), ep->type));

		return (*(fhp->getFeedProp))(ep, proptype);
	} else {
		g_warning(g_strdup_printf(_("internal error! there is no feed for feed key %s!"), key));
	}

	return NULL;
}

void setFeedProp(gchar *key, gint proptype, gpointer data) {
	gboolean	visibleInGUI = FALSE;
	entryPtr	ep;
	feedHandlerPtr	fhp;

	g_mutex_lock(feeds_lock);
	ep = (entryPtr)g_hash_table_lookup(feeds, (gpointer)key);
	g_mutex_unlock(feeds_lock);

	if(NULL != ep) {	
		if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&(ep->type))))
			g_error(_("internal error! no feed handler for this type!"));	

		g_assert(NULL != fhp->setFeedProp);
		(*(fhp->setFeedProp))(ep, proptype, data);
		
		/* handling for persistent and visible feed properties */
		switch(proptype) {
			case FEED_PROP_USERTITLE:
				visibleInGUI = TRUE;
				setEntryTitleInConfig(key, (gchar *)data);
				break;
			case FEED_PROP_AVAILABLE:
				visibleInGUI = TRUE;
				break;
			case FEED_PROP_SOURCE:
				setEntryURLInConfig(key, (gchar *)data);
				break;
			case FEED_PROP_UPDATEINTERVAL:
				setFeedUpdateIntervalInConfig(key, (gint)data);
				break;
		}

		/* if properties which are displayed in the feed list (like
		   title, availability, unreadCounter) are changed
		   the feed list model has to be updated! */		
		if(visibleInGUI)
			saveToFeedList(ep);
	} else {
		g_warning(g_strdup_printf(_("internal error! there is no feed for this feed key!"), key));
	}

}

/* "foreground" update executed in the main thread to update
   the selected and displayed feed */
void updateEntry(gchar *key) {

	print_status(g_strdup_printf("updating \"%s\"", getDefaultEntryTitle(key)));
	setFeedProp(key, FEED_PROP_UPDATECOUNTER, (gpointer)0);
	updateNow();
}

/* method to add feeds from new dialog */
gchar * newEntry(gint type, gchar *url, gchar *keyprefix) {
	feedHandlerPtr	fhp;
	entryPtr	new_ep = NULL;
	gchar		*key;
	gchar		*oldfilename, *newfilename;
	
	if(NULL != (fhp = g_hash_table_lookup(feedHandler, (gpointer)&type))) {
		g_assert(NULL != fhp->readFeed);
		new_ep = (entryPtr)(*(fhp->readFeed))(url);
	} else {
		g_error(_("internal error! no feed handler for this type!"));
	}
		
	if(NULL != new_ep) {	
		new_ep->type = type;
		new_ep->keyprefix = keyprefix;
		if(NULL != (key = addEntryToConfig(keyprefix, url, type))) {
			new_ep->key = key;
			
			if(type == FST_OCS) {
				/* rename the temporalily saved ocs file new.ocs to
				   <keyprefix>_<key>.ocs  */
				oldfilename = g_strdup_printf("%s/none_new.ocs", getCachePath());
				newfilename = getCacheFileName(keyprefix, key, "ocs");
				if(0 != rename(oldfilename, newfilename)) {
					g_print("error! could not move file %s to file %s\n", oldfilename, newfilename);
				}
				g_free(oldfilename);
				g_free(newfilename);
			}
			
			g_mutex_lock(feeds_lock);
			g_hash_table_insert(feeds, (gpointer)new_ep->key, (gpointer)new_ep);
			g_mutex_unlock(feeds_lock);
			
			addToEntryList((entryPtr)new_ep);
		} else {
			g_print(_("error! could not add entry!\n"));
		}
	} else {
		g_print("internal error while adding entry!\n");
	}

	return (NULL == new_ep)?NULL:new_ep->key;

}

/* ---------------------------------------------------------------------------- */
/* folder handling stuff (thats not the VFolder handling!)			*/
/* ---------------------------------------------------------------------------- */

gchar * getFolderTitle(gchar *keyprefix) {
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter;
	gchar			*tmp_title;

	feedstore = getFeedStore();
	g_assert(feedstore != NULL);
	
	/* topiter must not be NULL! because we cannot rename the root folder ! */
	if(NULL != (iter = g_hash_table_lookup(folders, (gpointer)keyprefix))) {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), iter, FS_TITLE, &tmp_title, -1);	
		return tmp_title;
	} else {
		g_print(_("internal error! could not determine folder key!"));
	}
}

void setFolderTitle(gchar *keyprefix, gchar *title) {
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter;

	feedstore = getFeedStore();
	g_assert(feedstore != NULL);
	
	/* topiter must not be NULL! because we cannot rename the root folder ! */
	if(NULL != (iter = g_hash_table_lookup(folders, (gpointer)keyprefix))) {
		gtk_tree_store_set(feedstore, iter, FS_TITLE, title, -1);	
		setFolderTitleInConfig(keyprefix, title);
	} else {
		g_print(_("internal error! could not determine folder key!"));
	}
}

void addFolder(gchar *keyprefix, gchar *title, gint type) {
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter;

	if(NULL == (iter = (GtkTreeIter *)g_malloc(sizeof(GtkTreeIter)))) 
		g_error("could not allocate memory!\n");

	feedstore = getFeedStore();
	g_assert(feedstore != NULL);

	/* if keyprefix is "" we have the root folder and don't create
	   a new iter! */
	if(0 == strlen(keyprefix)) {
		iter = NULL;
	} else {
		gtk_tree_store_append(feedstore, iter, NULL);
		gtk_tree_store_set(feedstore, iter, FS_TITLE, title,
						    FS_KEY, keyprefix,	
						    FS_TYPE, type,
						    -1);
	}
					    
	g_hash_table_insert(folders, (gpointer)keyprefix, (gpointer)iter);
}

void removeFolder(gchar *keyprefix) {
	GtkTreeStore		*feedstore;
	GtkTreeIter		*iter;

	feedstore = getFeedStore();
	g_assert(feedstore != NULL);
	
	/* topiter must not be NULL! because we cannot delete the root folder ! */
	if(NULL != (iter = g_hash_table_lookup(folders, (gpointer)keyprefix))) {
		removeFolderFromConfig(keyprefix);
	} else {
		g_print(_("internal error! could not determine folder key!"));
	}
}

/* ---------------------------------------------------------------------------- */
/* feed handling functions							*/
/* ---------------------------------------------------------------------------- */

/* method to add feeds from config */
gchar * addEntry(gint type, gchar *url, gchar *key, gchar *keyprefix, gchar *feedname, gint interval) {
	entryPtr	new_ep = NULL;
	feedHandlerPtr	fhp;

	if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&type))) {
		g_warning(g_strdup_printf(_("cannot load feed: no feed handler for type %d!"),type));
		return;
	}

	g_assert(NULL != fhp->loadFeed);
	g_assert(NULL != fhp->setFeedProp);
	new_ep = (entryPtr)(*(fhp->loadFeed))(keyprefix, key);

	if(NULL != new_ep) {
		new_ep->type = type;
		new_ep->key = key;	
		new_ep->keyprefix = keyprefix;

		(*(fhp->setFeedProp))(new_ep, FEED_PROP_USERTITLE, (gpointer)feedname);
		(*(fhp->setFeedProp))(new_ep, FEED_PROP_SOURCE, (gpointer)url);
		
		if(IS_FEED(type)) {
			(*(fhp->setFeedProp))(new_ep, FEED_PROP_UPDATEINTERVAL, (gpointer)interval);
			(*(fhp->setFeedProp))(new_ep, FEED_PROP_AVAILABLE, (gpointer)FALSE);
		}

		g_mutex_lock(feeds_lock);
		g_hash_table_insert(feeds, (gpointer)key, (gpointer)new_ep);
		g_mutex_unlock(feeds_lock);
		
		addToEntryList(new_ep);
	} else {
		g_print("internal error while adding entry!\n");
	}

	return (NULL == new_ep)?NULL:new_ep->key;
}

void removeEntry(gchar *keyprefix, gchar *key) {
	GtkTreeIter	iter;
	entryPtr	ep;
	feedHandlerPtr	fhp;

	g_mutex_lock(feeds_lock);	
	ep = (entryPtr)g_hash_table_lookup(feeds, (gpointer)key);
	g_mutex_unlock(feeds_lock);	

	if(NULL == ep) {
		print_status(_("internal error! could not find key in entry list! Cannot delete!\n"));
		return;
	}

	fhp = g_hash_table_lookup(feedHandler, (gpointer)&(ep->type));
	g_assert(NULL != fhp);
	
	if(NULL != fhp->removeFeed) {
		(*(fhp->removeFeed))(keyprefix, key, ep);
	} else {
		//g_warning(_("FIXME: no handler to remove this feed type (delete cached contents manually)!"));
	}

	removeEntryFromConfig(keyprefix, key);
}

/* shows entry after loading on startup or creations of a new entry */
void saveToFeedList(entryPtr ep) {
	gchar		*title, *source;
	feedHandlerPtr	fhp;

	if(NULL != (fhp = g_hash_table_lookup(feedHandler, (gpointer)&(ep->type)))) {
		g_assert(NULL != fhp->getFeedProp);
		title = (gchar *)(*(fhp->getFeedProp))(ep, FEED_PROP_TITLE);
		source = (gchar *)(*(fhp->getFeedProp))(ep, FEED_PROP_SOURCE);

		setInEntryList(ep, title, source, ep->type);
	} else {
		g_error(_("internal error! no feed handler for this type!"));
	}
}

static void resetUpdateCounter(gpointer key, gpointer value, gpointer userdata) {
	entryPtr	ep;
	feedHandlerPtr	fhp;
	gint		interval;
	
	/* we can't use getFeedProp and setFeedProp because feeds_lock
	   is lock and using these functions would deadlock us */
	   
	ep = (entryPtr)g_hash_table_lookup(feeds, (gpointer)key);
	g_assert(NULL != ep);

	if(NULL != (fhp = g_hash_table_lookup(feedHandler, (gpointer)&(ep->type)))) {
		g_assert(NULL != fhp->getFeedProp);
		g_assert(NULL != fhp->setFeedProp);
		
		interval = (gint)(*(fhp->getFeedProp))(ep, FEED_PROP_UPDATEINTERVAL);
g_print("update interval for %s ---- %d\n",(gchar *)(*(fhp->getFeedProp))(ep, FEED_PROP_USERTITLE) ,interval);
		if(0 < (interval)) {
g_print("resetting update counter\n");		
			(*(fhp->setFeedProp))(ep, FEED_PROP_UPDATECOUNTER, (gpointer)0);
		}
	}
}

void resetAllUpdateCounters(void) {

	g_mutex_lock(feeds_lock);
	g_hash_table_foreach(feeds, resetUpdateCounter, NULL);
	g_mutex_unlock(feeds_lock);
	
	updateNow();
}

gint  getEntryType(gchar *key) { 
	entryPtr	ep;
	
	g_mutex_lock(feeds_lock);
	ep = (entryPtr)g_hash_table_lookup(feeds, (gpointer)key);
	g_mutex_unlock(feeds_lock);
	
	if(NULL != ep) {
		return ep->type; 
	} else {
		return FST_INVALID;
	}
}

gchar * getDefaultEntryTitle(gchar *key) { 
	gchar		*title, *usertitle;

	title = (gchar *)getFeedProp(key, FEED_PROP_TITLE);
	usertitle = (gchar *)getFeedProp(key, FEED_PROP_USERTITLE);
	return (NULL == usertitle)?title:usertitle;		
}

void clearItemList() {
	gtk_tree_store_clear(GTK_TREE_STORE(itemstore));
}

gboolean getItemReadStatus(gint type, gpointer ip) {
	itemHandlerPtr	ihp;
	
	if(NULL != ip) {
		if(NULL != (ihp = g_hash_table_lookup(itemHandler, (gpointer)&type))) {
			g_assert(NULL != ihp->setItemProp);
			return (gboolean)(*(ihp->getItemProp))(ip, ITEM_PROP_READSTATUS);
		} else {
			g_error(_("internal error! no item handler for this feed type!"));
		}
	}
	
	return FALSE;
}

void markItemAsRead(gint type, gpointer ip) {
	itemHandlerPtr	ihp;
	
	if(NULL != ip) {
		if(NULL != (ihp = g_hash_table_lookup(itemHandler, (gpointer)&type))) {
			g_assert(NULL != ihp->setItemProp);
			(*(ihp->setItemProp))(ip, ITEM_PROP_READSTATUS, NULL);
		} else {
			g_error(_("internal error! no item handler for this feed type!"));
		}
	}
}

void loadItem(gint type, gpointer ip) {
	itemHandlerPtr	ihp;
	
	if(NULL != ip) {
		if(NULL != (ihp = g_hash_table_lookup(itemHandler, (gpointer)&type))) {
			g_assert(NULL != ihp->showItem);
			(*(ihp->showItem))(ip);
		} else {
			g_error(_("internal error! no item handler for this feed type!"));
		}
	}

	markItemAsRead(type, ip);
}

void loadItemList(gchar *key, gchar *searchstring) {
	feedHandlerPtr	fhp;
	itemHandlerPtr	ihp;
	GtkTreeIter	iter;
	GSList		*itemlist = NULL;
	gpointer	ip;
	entryPtr	ep;
	gint		count = 0;
	gchar		*title, *description, *time;
	gboolean	add;
	
	/* hmm... maybe we should store the parsed data as GtkTreeStores 
	   and exchange them ? */
	if(NULL == searchstring) g_mutex_lock(feeds_lock);
	ep = (entryPtr)g_hash_table_lookup(feeds, (gpointer)key);
	if(NULL == searchstring) g_mutex_unlock(feeds_lock);

	if(NULL != ep) {
		itemlist = (GSList *)getFeedProp(key, FEED_PROP_ITEMLIST);

		if(NULL == (ihp = g_hash_table_lookup(itemHandler, (gpointer)&(ep->type)))) {
			g_error(_("internal error! no item handler for this type!"));
			return;			
		}		
	} else {
		print_status(_("internal error! item display for NULL pointer requested!"));
		return;
	}

	while(NULL != itemlist) {
		ip = itemlist->data;
		title = (gchar *)(*(ihp->getItemProp))(ip, ITEM_PROP_TITLE);
		description = (gchar *)(*(ihp->getItemProp))(ip, ITEM_PROP_DESCRIPTION);
		time = (gchar *)(*(ihp->getItemProp))(ip, ITEM_PROP_TIME);
		
		if(0 == ((++count)%100)) 
			print_status(g_strdup_printf(_("loading feed... (%d)"), count));

		add = TRUE;
		if(NULL != searchstring) {
			add = FALSE;
				
			if((NULL != title) && (NULL != strstr(title, searchstring)))
				add = TRUE;

			if((NULL != description) && (NULL != strstr(description, searchstring)))
				add = TRUE;
		}

		if(add) {
			gtk_tree_store_append(itemstore, &iter, NULL);
			gtk_tree_store_set(itemstore, &iter,
	     		   		IS_TITLE, title,
					IS_PTR, ip,
					IS_TIME, time,
					IS_TYPE, ep->type,
					-1);
		}

		itemlist = g_slist_next(itemlist);
	}

	if(gnome_vfs_is_primary_thread()) {	/* must not be called from updateFeeds()! */
		if(NULL != (fhp = g_hash_table_lookup(feedHandler, (gpointer)&(ep->type)))) {
			g_assert(NULL != fhp->showFeedInfo);
			(*(fhp->showFeedInfo))(ep);
		} else {
			g_error(_("internal error! no feed handler for this type!"));
		}
	}
}

static void searchInFeed(gpointer key, gpointer value, gpointer userdata) {

	loadItemList((gchar *)key, (gchar *)userdata);
}

void searchItems(gchar *string) {

	clearItemList();
	//g_mutex_lock(feeds_lock);
	g_hash_table_foreach(feeds, searchInFeed, string);
	//g_mutex_unlock(feeds_lock);	
}

/* ---------------------------------------------------------------------------- */
/* vfolder handling functions							*/
/* ---------------------------------------------------------------------------- */

/* does the scanning of a feed for loadVFolder(), method is also called 
   by the merge() functions of the feed modules */
void scanFeed(gpointer key, gpointer value, gpointer userdata) {
	entryPtr	ep = (entryPtr)userdata;
	VFolderPtr	vp = (VFolderPtr)value;
	itemHandlerPtr	ihp;
	feedHandlerPtr	fhp;
	GSList		*itemlist = NULL;
	gpointer	ip;
	gchar		*title, *description;
	gboolean	add;

	/* check the type because we are called with a g_hash_table_foreach()
	   but only want to process vfolders ...*/
	if(vp->type != FST_VFOLDER) 
		return;
	
	if(ep->type == FST_VFOLDER)
		return;	/* don't scan vfolders! */

	if(NULL != ep) {
		/* cannot use getFeedProp(), that would cause a deadlock! */
		if(NULL != (fhp = g_hash_table_lookup(feedHandler, (gpointer)&(ep->type)))) {
			g_assert(NULL != fhp->getFeedProp);
			itemlist = (GSList *)(*(fhp->getFeedProp))(ep, FEED_PROP_ITEMLIST);
		} else {
			g_error(_("internal error! no feed handler for this type!"));
			return;			
		}	

		if(NULL == (ihp = g_hash_table_lookup(itemHandler, (gpointer)&(ep->type)))) {			
			g_error(_("internal error! no item handler for this type!"));
			return;			
		}		
	} else {
		print_status(_("internal error! item scan for NULL pointer requested!"));
		return;
	}
	
	while(NULL != itemlist) {
		ip = itemlist->data;
		title = (gchar *)(*(ihp->getItemProp))(ip, ITEM_PROP_TITLE);
		description = (gchar *)(*(ihp->getItemProp))(ip, ITEM_PROP_DESCRIPTION);
		
		add = FALSE;
		if((NULL != title) && matchVFolderRules(vp, title))
			add = TRUE;

		if((NULL != description) && matchVFolderRules(vp, description))
			add = TRUE;

		if(add) {
			addItemToVFolder(vp, ep, ip, ep->type);
		}

		itemlist = g_slist_next(itemlist);
	}
}

/* g_hash_table_foreach-function to be called from update.c to 
   remove old items of a feed from all vfolders */
void removeOldItemsFromVFolders(gpointer key, gpointer value, gpointer userdata) {
	VFolderPtr	vp = (VFolderPtr)value;
	
	if(FST_VFOLDER == vp->type)
		removeOldItemsFromVFolder(vp, userdata);
}

/* scan all feeds for matching any vfolder rules */
void loadVFolder(gpointer key, gpointer value, gpointer userdata) {
	entryPtr	ep = (entryPtr)value;

	/* match the feed ep against all vfolders... */
	g_hash_table_foreach(feeds, scanFeed, ep);
}

/* called upon initialization */
void loadVFolders(void) {

	g_mutex_lock(feeds_lock);
	/* iterate all feeds ... */
	g_hash_table_foreach(feeds, loadVFolder, NULL);
	g_mutex_unlock(feeds_lock);

}

/* called when creating a new VFolder */
void loadNewVFolder(gchar *key, gpointer rp) {
	gpointer	vp;

	g_mutex_lock(feeds_lock);
	vp = g_hash_table_lookup(feeds, (gpointer)key);
	g_mutex_unlock(feeds_lock);

	setVFolderRules((VFolderPtr)vp, (rulePtr)rp);

	/* FIXME: brute force: update of all vfolders redundant */
	loadVFolders();
}

/* ---------------------------------------------------------------------------- */
/* tree store handling								*/
/* ---------------------------------------------------------------------------- */

GtkTreeStore * getItemStore(void) {

	if(NULL == itemstore) {
		/* set up a store of these attributes: 
			- item title
			- item state (read/unread)		
			- pointer to item data
			- date time_t value
			- the type of the feed the item belongs to

		 */
		itemstore = gtk_tree_store_new(5, G_TYPE_STRING, 
						  GDK_TYPE_PIXBUF, 
						  G_TYPE_POINTER, 
						  G_TYPE_INT,
						  G_TYPE_INT);
	}
	
	return itemstore;
}

GtkTreeStore * getFeedStore(void) {

	if(NULL == feedstore) {
		/* set up a store of these attributes: 
			- feed title
			- feed url
			- feed state icon (not available/available)
			- feed key in gconf
			- feed list view type (node/rss/cdf/pie/ocs...)
		 */
		feedstore = gtk_tree_store_new(5, G_TYPE_STRING, 
						  G_TYPE_STRING, 
						  GDK_TYPE_PIXBUF, 
						  G_TYPE_STRING,
						  G_TYPE_INT);
	}
	
	return feedstore;
}

static void moveIfInFolder(gpointer keyprefix, gpointer value, gpointer key) {
	GtkTreeIter	iter;
	GtkTreeIter	*topiter = (GtkTreeIter *)value;
	gint		tmp_type;
	entryPtr	ep;
	gchar		*newkey, *tmp_key, *tmp_url;
	gboolean	valid, found = FALSE;

	g_assert(NULL != keyprefix);
	g_assert(NULL != key);
	g_assert(NULL != feedstore);

	found = FALSE;
	topiter = (GtkTreeIter *)g_hash_table_lookup(folders, keyprefix);
	gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &iter, topiter);
	do {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter,
				FS_URL, &tmp_url,
				FS_KEY, &tmp_key,
				FS_TYPE, &tmp_type,
		  	      -1);

		g_assert(NULL != tmp_key);
		if(0 == strcmp(tmp_key, (gchar *)key))
			found = TRUE;

		if(found) {
			newkey = addEntryToConfig((gchar *)keyprefix, tmp_url, tmp_type);

			/* update the feed hashtable */
			ep = (entryPtr)g_hash_table_lookup(feeds, (gpointer)key);
			ep->key = newkey;
			ep->keyprefix = keyprefix;
			g_hash_table_insert(feeds, (gpointer)newkey, (gpointer)ep);
			g_hash_table_remove(feeds, (gpointer)key);

			/* update changed row contents */
			gtk_tree_store_set(feedstore, &iter,
					   FS_KEY, (gpointer)newkey,
					  -1);
		}
		g_free(tmp_url);
		g_free(tmp_key);
		
		if(found)
			return;
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	} while(valid);
}

/* function to reflect DND of feed entries in the configuration */
void moveInEntryList(gchar *oldkeyprefix, gchar *oldkey) {

	removeEntryFromConfig(oldkeyprefix, oldkey);

	/* find new treestore entry and keyprefix */
	g_hash_table_foreach(folders, moveIfInFolder, (gpointer)oldkey);
}

/* this function can be used to update any of the values by specifying
   the feedkey as first parameter */
void setInEntryList(entryPtr ep, gchar * title, gchar *source, gint type) {
	GtkTreeIter	iter;
	GtkTreeIter	*topiter;
	gboolean	valid, found = 0;	
	gchar		*tmp_title = NULL;
	gchar		*tmp_url = NULL;
	gchar		*tmp_key;
	gchar		*keyprefix;
	gint		tmp_type;
	
	g_assert(NULL != ep);
	g_assert(NULL != feedstore);

	g_assert(NULL != ep->keyprefix);
	topiter = (GtkTreeIter *)g_hash_table_lookup(folders, (gpointer)(ep->keyprefix));
	gtk_tree_model_iter_children(GTK_TREE_MODEL(feedstore), &iter, topiter);
	do {
		gtk_tree_model_get(GTK_TREE_MODEL(feedstore), &iter,
				FS_TITLE, &tmp_title,
				FS_URL, &tmp_url,
				FS_KEY, &tmp_key,
				FS_TYPE, &tmp_type,
		  	      -1);

		if(tmp_type == type) {
			g_assert(NULL != tmp_key);
			if(0 == strcmp(tmp_key, ep->key))
				found = 1;
		}

		if(found) {
			/* insert changed row contents */
			gtk_tree_store_set(feedstore, &iter,
					   FS_TITLE, title,
					   FS_URL, source,
					   FS_KEY, ep->key,
					   FS_TYPE, type,
					  -1);
		}
		
		g_free(tmp_title);
		g_free(tmp_url);
		g_free(tmp_key);
		
		if(found)
			return;
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(feedstore), &iter);
	} while(valid);

	/* if we come here, this is a not yet added feed */
	addToEntryList(ep);
}

void addToEntryList(entryPtr ep) {
	GtkTreeIter	iter;
	GtkTreeIter	*topiter;
	
	g_assert(NULL != ep->key);
	g_assert(NULL != ep->keyprefix);

	topiter = (GtkTreeIter *)g_hash_table_lookup(folders, (gpointer)(ep->keyprefix));

	gtk_tree_store_append(feedstore, &iter, topiter);
	gtk_tree_store_set(feedstore, &iter,
			   FS_TITLE, getDefaultEntryTitle(ep->key),
			   FS_URL, (gchar *)getFeedProp(ep->key, FEED_PROP_SOURCE),
			   FS_KEY, ep->key,
			   FS_TYPE, ep->type,
			   -1);		   
}
