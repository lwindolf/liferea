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
#include "conf.h"

#include "ns_dc.h"
#include "ns_fm.h"
#include "ns_slash.h"
#include "ns_content.h"
#include "ns_ocs.h"

#include "cdf_channel.h"
#include "cdf_item.h"

#include "rss_channel.h"
#include "rss_item.h"
#include "rss_ns.h"

#include "ocs_dir.h"
#include "ocs_ns.h"

#include "backend.h"
#include "callbacks.h"	

#define HELPURL		"http://liferea.sf.net/help033.rdf"

GtkTreeStore	*itemstore = NULL;
GtkTreeStore	*entrystore = NULL;

extern GHashTable *rss_nslist;
extern GHashTable *ocs_nslist;

extern GMutex * entries_lock;

/* hash table to lookup the tree iterator for each key prefix */
GHashTable	*folders = NULL;

/* used to lookup a feed/folders pointer specified by a key */
GHashTable	*entries = NULL;

/* prototypes */
void setInEntryList(gchar *key, gchar *feedname, gchar *feedurl, gint type);
void addToEntryList(entryPtr cp);
void showEntry(gpointer ep);

/* ------------------------------------------------------------------------- */

guint hashFunction(gconstpointer key) {	return (guint)atoi((char *)key); }
gint feedsHashCompare(gconstpointer a, gconstpointer b) { return a-b; }

/* initializing function, called upon initialization and each
   preference change */
void initBackend() {

	g_mutex_lock(entries_lock);
	if(NULL == entries)
		entries = g_hash_table_new(g_str_hash, g_str_equal);
	g_mutex_unlock(entries_lock);
		
	if(NULL == folders)
		folders =  g_hash_table_new(g_str_hash, g_str_equal);

	g_free(rss_nslist);
	g_free(ocs_nslist);	
	rss_nslist = g_hash_table_new(g_str_hash, g_str_equal);
	ocs_nslist = g_hash_table_new(g_str_hash, g_str_equal);	
	
	/* register RSS name space handlers */
	if(getBooleanConfValue(USE_DC))
		g_hash_table_insert(rss_nslist, (gpointer)ns_dc_getRSSNsPrefix(),
					        (gpointer)ns_dc_getRSSNsHandler());
	if(getBooleanConfValue(USE_FM))
		g_hash_table_insert(rss_nslist, (gpointer)ns_fm_getRSSNsPrefix(),
					        (gpointer)ns_fm_getRSSNsHandler());					    
	if(getBooleanConfValue(USE_SLASH))
		g_hash_table_insert(rss_nslist, (gpointer)ns_slash_getRSSNsPrefix(), 
					        (gpointer)ns_slash_getRSSNsHandler());
	if(getBooleanConfValue(USE_CONTENT))
		g_hash_table_insert(rss_nslist, (gpointer)ns_content_getRSSNsPrefix(),
					        (gpointer)ns_content_getRSSNsHandler());
						
	/* register OCS name space handlers */
	g_hash_table_insert(ocs_nslist, (gpointer)ns_dc_getOCSNsPrefix(),
				        (gpointer)ns_dc_getOCSNsHandler());

	g_hash_table_insert(ocs_nslist, (gpointer)ns_ocs_getOCSNsPrefix(),
				        (gpointer)ns_ocs_getOCSNsHandler());
						
}

/* "foreground" update executed in the main thread to update
   the selected and displayed feed */
void updateEntry(gchar *key) {
	entryPtr	old_ep;
	entryPtr	new_ep = NULL;

	g_mutex_lock(entries_lock);
	old_ep = (entryPtr)g_hash_table_lookup(entries, (gpointer)key);
	g_mutex_unlock(entries_lock);
		
	if(NULL != old_ep) {
		print_status(g_strdup_printf("updating \"%s\"", getDefaultEntryTitle(key)));
		old_ep->updateCounter = 0;
		updateNow();
	} else {
		print_status(_("could not resolve entry key! cannot update entry!\n"));
	}
}

/* loads an RDF feed, which is not displayed in the feed list
   but will be loaded as program help each time the help button is clicked... */
gchar * getHelpFeedKey(void) {
	channelPtr	new_cp = NULL;	
	
	g_mutex_lock(entries_lock);
	new_cp = (channelPtr)g_hash_table_lookup(entries, (gpointer)"helpkey");
	g_mutex_unlock(entries_lock);	
	
	if(NULL == new_cp) {
		if(NULL == (new_cp = (channelPtr) malloc(sizeof(struct channel)))) {
			g_error("not enough memory!\n");
			return NULL;
		}

		new_cp = (channelPtr)readRSSFeed(HELPURL);
		new_cp->key = g_strdup("helpkey");
		new_cp->type = FST_FEED;

		g_mutex_lock(entries_lock);
		g_hash_table_insert(entries, (gpointer)(new_cp->key), (gpointer)new_cp);	
		g_mutex_unlock(entries_lock);
		
		return new_cp->key;
	}
	
	return new_cp->key;
}

/* method to add entries from new dialog */
gchar * newEntry(gint type, gchar *url, gchar *keyprefix) {
	entryPtr	new_ep = NULL;
	channelPtr	new_cp = NULL;
	CDFChannelPtr	new_cdfp = NULL;
	directoryPtr	new_dp = NULL;	
	gchar		*key, *keypos;
	gchar		*oldfilename, *newfilename;
	
	switch(type) {
		case FST_FEED:
			new_ep = (entryPtr)(new_cp = readRSSFeed(url));
			new_cp->updateInterval = -1;
			new_cp->source = url;
			break;
		case FST_CDF:
			new_ep = (entryPtr)(new_cdfp = readCDFFeed(url));
			new_cdfp->updateInterval = -1;
			new_cdfp->source = url;		
			break;
		case FST_OCS:
			new_ep = (entryPtr)(new_dp = readOCS(url));
			break;
		default:
			g_print("FIXME: implement add of entry type %d\n", type);
			return;
			break;
	}
	
	if(NULL != new_ep) {	
		new_ep->usertitle = NULL;
		new_ep->type = type;
		new_ep->keyprefix = keyprefix;
		new_ep->updateCounter = -1;
		if(NULL != (key = addEntryToConfig(keyprefix, url, type))) {
			new_ep->key = key;
			
			if(type == FST_OCS) {
				/* rename the temporalily saved ocs file new.ocs to
				   <keyprefix>_<key>.ocs  */
				keypos = strrchr(key, '/');
				if(NULL == keypos)
					keypos = key;
				else
					keypos++;

				oldfilename = g_strdup_printf("%s/new.ocs", getCachePath());
				newfilename = g_strdup_printf("%s/%s_%s.ocs", getCachePath(), keyprefix, keypos);
				if(0 != rename(oldfilename, newfilename)) {
					g_print("error! could not move file %s to file %s\n", oldfilename, newfilename);
				}
				g_free(oldfilename);
				g_free(newfilename);
			}
			
			g_mutex_lock(entries_lock);
			g_hash_table_insert(entries, (gpointer)new_ep->key, (gpointer)new_ep);
			g_mutex_unlock(entries_lock);
			
			addToEntryList((entryPtr)new_ep);
		} else {
			g_print(_("error! could not add entry!\n"));
		}
	} else {
		g_print("internal error while adding entry!\n");
	}

	return (NULL == new_ep)?NULL:new_ep->key;

}

void addFolder(gchar *keyprefix, gchar *title) {
	GtkTreeStore		*entrystore;
	GtkTreeIter		*iter;

	if(NULL == (iter = (GtkTreeIter *)g_malloc(sizeof(GtkTreeIter)))) 
		g_error("could not allocate memory!\n");

	entrystore = getEntryStore();
	g_assert(entrystore != NULL);
	gtk_tree_store_append(entrystore, iter, NULL);
	gtk_tree_store_set(entrystore, iter, FS_TITLE, title,
					    FS_KEY, keyprefix,	
					    FS_TYPE, FST_NODE,
					    -1);
	g_hash_table_insert(folders, (gpointer)keyprefix, (gpointer)iter);
}

void removeFolder(gchar *keyprefix) {
	GtkTreeStore		*entrystore;
	GtkTreeIter		*iter;

	entrystore = getEntryStore();
	g_assert(entrystore != NULL);
	
	if(NULL != (iter = g_hash_table_lookup(folders, (gpointer)keyprefix))) {
		removeFolderFromConfig(keyprefix);
	} else {
		g_print(_("internal error! could not determine folder key!"));
	}
}

/* method to add entries from config */
gchar * addEntry(gint type, gchar *url, gchar *key, gchar *keyprefix, gchar *feedname, gint interval) {
	entryPtr	new_ep = NULL;
	channelPtr	new_cp = NULL;
	gchar		*filename, *keypos;

	if(IS_FEED(type)) {

		/* if we are called from loadConfig() we load the saved
		   feed from harddisc */
		// FIXME: new_cp = loadFeed(key);

		// workaround as long loading is not implemented
		if(NULL == (new_cp = (channelPtr) malloc(sizeof(struct channel)))) {
			g_error("not enough memory!\n");
			return NULL;
		}

		memset(new_cp, 0, sizeof(struct channel));
		new_cp->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);
		new_cp->updateInterval = -1;
		new_cp->updateCounter = 0;	/* to enforce immediate reload */			
		new_ep = (entryPtr)new_cp;

	} else if(FST_OCS == type) {

		keypos = strrchr(key, '/');
		if(NULL == keypos)
			keypos = key;
		else
			keypos++;

		filename = g_strdup_printf("%s/%s_%s.ocs", getCachePath(), keyprefix, keypos);		
		new_ep = (entryPtr)loadOCS(filename);
		g_free(filename);

	} else {

		g_print(_("unknown entry type! cannot add entry!!!\n"));
		return NULL;;

	}
	
	if(NULL != new_ep) {
		new_ep->key = key;	
		new_ep->keyprefix = keyprefix;	
		new_ep->usertitle = feedname;	
		new_ep->type = type;
/*g_print("key:%s title:%s t:%d uc:%d\n",new_ep->key,new_ep->usertitle,new_ep->type,new_ep->updateCounter);*/
		if(IS_FEED(type)) {
			new_ep->source = url;		
			new_cp->updateInterval = interval;
			new_ep->available = FALSE;
/*g_print("setting updateinterval to: %d\n",new_cp->updateInterval);*/
		}
	
		g_mutex_lock(entries_lock);
		g_hash_table_insert(entries, (gpointer)key, (gpointer)new_ep);
		g_mutex_unlock(entries_lock);
		
		addToEntryList((entryPtr)new_ep);
	} else {
		g_print("internal error while adding entry!\n");
	}

	return (NULL == new_ep)?NULL:new_ep->key;
}

void removeEntry(gchar *keyprefix, gchar *key) {
	GtkTreeIter	iter;
	entryPtr	ep;
	gchar		*filename;
	gchar		*keypos;

	g_mutex_lock(entries_lock);	
	ep = (entryPtr)g_hash_table_lookup(entries, (gpointer)key);
	g_mutex_unlock(entries_lock);	
	
	if(NULL == ep) {
		print_status(_("internal error! could not find key in entry list! Cannot delete!\n"));
		return;
	}
	
	/* removal of used memory and persistant data */	
	switch(ep->type) {
		case FST_FEED:
		case FST_CDF:
			// FIXME: free feed data structures...
			// FIXME: remove channel from savefile
			break;			
		case FST_OCS:
			// FIXME: free ocs data structures...
			keypos = strrchr(key, '/');
			if(NULL == keypos)
				keypos = key;
			else
				keypos++;

			filename = g_strdup_printf("%s/%s_%s.ocs", getCachePath(), keyprefix, keypos);
			g_print("deleting cache file %s\n", filename);
			if(0 != unlink(filename)) {
				showErrorBox(_("could not remove cache file of this entry!"));
			}
			g_free(filename);
			break;
		default:
			g_error(_("unknown type while removing entry!"));
			break;
	}

	removeEntryFromConfig(keyprefix, key);
}

/* shows entry after loading on startup or creations of a new entry */
void showEntry(gpointer e) {
	entryPtr	ep = (entryPtr)e;
	CDFChannelPtr	cdfp;
	channelPtr	cp;

	switch(ep->type) {
		case FST_FEED:
			cp = (channelPtr)ep;
			setInEntryList(cp->key, 
				      (cp->usertitle)?cp->usertitle:cp->tags[CHANNEL_TITLE],
				      cp->source,
				      ep->type);
			break;
		case FST_CDF:
			cdfp = (CDFChannelPtr)ep;
			setInEntryList(cdfp->key, 
				      (cdfp->usertitle)?cdfp->usertitle:cdfp->tags[CDF_CHANNEL_TITLE],
				      cdfp->source,
				      ep->type);			      
			break;
		case FST_OCS:
			setInEntryList(ep->key, 
			      ep->usertitle,
			      ep->source,
			      ep->type);
			break;
		default:
			g_assert(_("invalid entry type! cannot set entry in treeview store!"));
			break;
	}		      
}

void setEntryTitle(gchar *key, gchar *title) { 
	entryPtr	ep;

	g_mutex_lock(entries_lock);	
	ep = (entryPtr)g_hash_table_lookup(entries, (gpointer)key);
	g_mutex_unlock(entries_lock);	
	
	if(NULL != ep) {
		setEntryTitleInConfig(key, title);	/* update in gconf */
		g_free(ep->usertitle);			/* update local copy */
		ep->usertitle = g_strdup(title);
		showEntry(ep); 				/* refresh treeview model */
	}
}

void setEntrySource(gchar *key, gchar *source) {
	entryPtr	ep;

	g_mutex_lock(entries_lock);
	ep = (entryPtr)g_hash_table_lookup(entries, (gpointer)key);
	g_mutex_unlock(entries_lock);
	
	if(NULL != ep) {
		setEntryURLInConfig(key, source); 	/* update in gconf */
		g_free(ep->source);			/* update local copy */
		ep->source = g_strdup(source);
		showEntry(ep); 				/* refresh treeview model */
	}
}

void setFeedUpdateInterval(gchar *feedkey, gint interval) {
	channelPtr	c;
	
	g_mutex_lock(entries_lock);
	c = (channelPtr)g_hash_table_lookup(entries, (gpointer)feedkey);
	g_mutex_unlock(entries_lock);
	
	if(NULL != c) {
		g_assert(IS_FEED(c->type));	
		
		if(0 == interval)
			interval = -1;	/* this is due to ignore this feed while updating */
					
		setFeedUpdateIntervalInConfig(c->key, interval);	/* update in gconf */
			
		c->updateInterval = interval;				/* update local copy */
		c->updateCounter = interval;
	}
}

static void resetUpdateCounter(gpointer key, gpointer value, gpointer userdata) {

	if(IS_FEED(((entryPtr)value)->type))
		((channelPtr)value)->updateCounter = 0;
}

void resetAllUpdateCounters(void) {
	g_mutex_lock(entries_lock);
	g_hash_table_foreach(entries, resetUpdateCounter, NULL);
	g_mutex_unlock(entries_lock);
}

/* just some encapsulation */

gboolean getEntryStatus(gchar *key) {
	entryPtr	ep;

	g_mutex_lock(entries_lock);
	ep = (entryPtr)g_hash_table_lookup(entries, (gpointer)key);
	g_mutex_unlock(entries_lock);
	
	if(NULL != ep) {
		return ep->available; 
	} else {
		return FALSE;
	}

}

gchar * getEntrySource(gchar *key) { 
	entryPtr	ep;
	
	g_mutex_lock(entries_lock);
	ep = (entryPtr)g_hash_table_lookup(entries, (gpointer)key);
	g_mutex_unlock(entries_lock);
	
	if(NULL != ep) {
		return ep->source; 
	} else {
		return NULL;
	}
}

gint  getEntryType(gchar *key) { 
	entryPtr	ep;
	
	g_mutex_lock(entries_lock);
	ep = (entryPtr)g_hash_table_lookup(entries, (gpointer)key);
	g_mutex_unlock(entries_lock);
	
	if(NULL != ep) {
		return ep->type; 
	} else {
		return FST_INVALID;
	}
}

gint	getFeedUpdateInterval(gchar *feedkey) { 
	channelPtr	c;
	
	g_mutex_lock(entries_lock);
	c = (channelPtr)g_hash_table_lookup(entries, (gpointer)feedkey);
	g_mutex_unlock(entries_lock);
	
	if(NULL != c) {
		return c->updateInterval;
	} else {
		return -1;
	}
}

gchar * getDefaultEntryTitle(gchar *key) { 
	entryPtr	ep;
	CDFChannelPtr	cdfp;
	directoryPtr	dp;
	channelPtr	cp;
	
	g_mutex_lock(entries_lock);
	ep = (entryPtr)g_hash_table_lookup(entries, (gpointer)key);
	g_mutex_unlock(entries_lock);
	
	if(NULL != ep) {
		switch(ep->type) {
			case FST_FEED:
				cp = (channelPtr)ep;
				return (NULL == (cp->usertitle))?cp->tags[CHANNEL_TITLE]:cp->usertitle;
				break;
			case FST_CDF:
				cdfp = (CDFChannelPtr)ep;
				return (NULL == (cdfp->usertitle))?cdfp->tags[CDF_CHANNEL_TITLE]:cdfp->usertitle;
				break;
			case FST_OCS:
				dp = (directoryPtr)ep;
				return (NULL == (dp->usertitle))?dp->tags[OCS_TITLE]:dp->usertitle;
				break;
			default:
				g_print(_("internal error: unknown entry type!"));
				break;
		}
	} else {
		return NULL;
	}
}

void clearItemList() {
	gtk_tree_store_clear(GTK_TREE_STORE(itemstore));
}

void markItemAsRead(gint type, gpointer ip) {

	switch(type) {
		case FST_FEED:
			if(NULL != ((itemPtr)ip)->cp) {
				markRSSItemAsRead(ip);
			} else {
				print_status(_("internal error! mark item as unread for NULL feed pointer requested!\n"));
			}
			break;
		case FST_CDF:
			if(NULL != ((CDFItemPtr)ip)->cp) {
				markCDFItemAsRead(ip);
			} else {
				print_status(_("internal error! mark item as unread for NULL feed pointer requested!\n"));
			}
			break;			
		case FST_OCS:
			/* do nothing */
			break;
		default:
			g_print("internal error! unknown item type!\n");
	}
}

void loadItem(gint type, gpointer ip) {

	switch(type) {
		case FST_FEED:
			if(NULL != ((itemPtr)ip)->cp) {
				showItem(ip, ((itemPtr)ip)->cp);
			} else {
				print_status(_("internal error! show item for NULL feed pointer requested!\n"));
			}
			break;
		case FST_CDF:
			if(NULL != ((CDFItemPtr)ip)->cp) {
				showCDFItem(ip, ((CDFItemPtr)ip)->cp);
			} else {
				print_status(_("internal error! show item for NULL feed pointer requested!\n"));
			}
			break;			
		case FST_OCS:
			if(NULL != ((dirEntryPtr)ip)->dp) {
				showDirectoryEntry(ip, ((dirEntryPtr)ip)->dp);
			} else {
				print_status(_("internal error! show item for NULL feed pointer requested!\n"));
			}
		
			break;
		default:
			g_print("internal error! unknown item type!\n");
	}
	
	markItemAsRead(type, ip);
}

void loadItemList(gchar *feedkey, gchar *searchstring) {
	GtkTreeIter	iter;
	GSList		*direntry;
	itemPtr		item;
	CDFItemPtr	cdfitem;	
	entryPtr	ep;
	gint		count;
	gboolean	add;
	
	/* hmm... maybe we should store the parsed data as GtkTreeStores 
	   and exchange them ? */
	if(NULL == searchstring) g_mutex_lock(entries_lock);
	ep = (entryPtr)g_hash_table_lookup(entries, (gpointer)feedkey);
	if(NULL == searchstring) g_mutex_unlock(entries_lock);

	if(NULL == ep) {
		print_status(_("internal error! item display for NULL pointer requested!"));
		return;
	}

	switch(ep->type) {
		case FST_FEED:  
			item = ((channelPtr)ep)->items;
			while(NULL != item) {
				add = TRUE;

				if(NULL != searchstring) {
					add = FALSE;

					// FIXME: use getRSSItemTag				
					if((NULL != item->tags[ITEM_TITLE]) && (NULL != strstr(item->tags[ITEM_TITLE], searchstring)))
						add = TRUE;

					if((NULL != item->tags[ITEM_DESCRIPTION]) && (NULL != strstr(item->tags[ITEM_DESCRIPTION], searchstring)))
						add = TRUE;
				}
									
				if(add) {
					gtk_tree_store_append(itemstore, &iter, NULL);
					gtk_tree_store_set(itemstore, &iter,
	     		   				IS_TITLE, item->tags[ITEM_TITLE],
							IS_PTR, (gpointer)item,
							IS_TIME, item->time,
							IS_TYPE, ep->type,
							-1);
				}

				item = item->next;
			}

			if(gnome_vfs_is_primary_thread())	/* must not be called from updateFeeds()! */
					showFeedInfo(ep);
			break;
		case FST_CDF:  
			cdfitem = ((CDFChannelPtr)ep)->items;
			while(NULL != cdfitem) {
				add = TRUE;
				
				if(NULL != searchstring) {
					add = FALSE;
					
					// FIXME: use getCDFItemTag			
					if((NULL != cdfitem->tags[CDF_ITEM_TITLE]) && (NULL != strstr(cdfitem->tags[CDF_ITEM_TITLE], searchstring)))
						add = TRUE;

					if((NULL != cdfitem->tags[CDF_ITEM_DESCRIPTION]) && (NULL != strstr(cdfitem->tags[CDF_ITEM_DESCRIPTION], searchstring)))
						add = TRUE;
				}
				
				if(add) {
					gtk_tree_store_append(itemstore, &iter, NULL);
					gtk_tree_store_set(itemstore, &iter,
	     		   				IS_TITLE, cdfitem->tags[CDF_ITEM_TITLE],
							IS_PTR, (gpointer)cdfitem,
							IS_TIME, cdfitem->time,
							IS_TYPE, ep->type,
							-1);
				}

				cdfitem = cdfitem->next;
			}

			if(gnome_vfs_is_primary_thread())	/* must not be called from updateFeeds()! */
					showCDFFeedInfo(ep);
			break;			
		case FST_OCS:
			count = 0;
			direntry = ((directoryPtr)ep)->items;
			while(NULL != direntry) {
				if(0 == ((++count)%100)) 
					print_status(g_strdup_printf(_("loading directory entries... (%d)"), count));

				add = TRUE;
				
				if(NULL != searchstring) {
					add = FALSE;

					if((NULL != ((directoryPtr)(direntry->data))->tags[OCS_TITLE]) && (NULL != strstr(((directoryPtr)(direntry->data))->tags[OCS_TITLE], searchstring)))
						add = TRUE;

					if((NULL != ((directoryPtr)(direntry->data))->tags[OCS_DESCRIPTION]) && (NULL != strstr(((directoryPtr)(direntry->data))->tags[OCS_DESCRIPTION], searchstring)))
						add = TRUE;
				}
					
				if(add) {
					gtk_tree_store_append(itemstore, &iter, NULL);
					gtk_tree_store_set(itemstore, &iter,
	     		   				IS_TITLE, (((dirEntryPtr)(direntry->data)))->tags[OCS_TITLE],
							IS_PTR, direntry->data,
							IS_TYPE, ep->type,
							-1);	
				}
				
				direntry = g_slist_next(direntry);
			}

			if(gnome_vfs_is_primary_thread())	/* must not be called from updateFeeds()! */
					showDirectoryInfo(ep);			
			break;
	}
}

static void searchInFeed(gpointer key, gpointer value, gpointer userdata) {

	loadItemList((gchar *)key, (gchar *)userdata);
}

void searchItems(gchar *string) {

	clearItemList();
	//g_mutex_lock(entries_lock);
	g_hash_table_foreach(entries, searchInFeed, string);
	//g_mutex_unlock(entries_lock);	
}

GtkTreeStore * getItemStore(void) {

	if(NULL == itemstore) {
		/* set up a store of these attributes: 
			- item title
			- item state (read/unread)		
			- pointer to item data
			- a string containing the receival time
			- the type of the feed the item belongs to

		 */
		itemstore = gtk_tree_store_new(5, G_TYPE_STRING, 
						  GDK_TYPE_PIXBUF, 
						  G_TYPE_POINTER, 
						  G_TYPE_STRING,
						  G_TYPE_INT);
	}
	
	return itemstore;
}

GtkTreeStore * getEntryStore(void) {

	if(NULL == entrystore) {
		/* set up a store of these attributes: 
			- feed title
			- feed url
			- feed state icon (not available/available)
			- feed key in gconf
			- feed list view type (node/feed/ocs)
		 */
		entrystore = gtk_tree_store_new(5, G_TYPE_STRING, 
						  G_TYPE_STRING, 
						  GDK_TYPE_PIXBUF, 
						  G_TYPE_STRING,
						  G_TYPE_INT);
	}
	
	return entrystore;
}

/* this function can be used to update any of the values by specifying
   the feedkey as first parameter */
void setInEntryList(gchar *key, gchar * title, gchar *source, gint type) {
	GtkTreeIter	iter;
	GtkTreeIter	*topiter;
	gboolean	valid, found = 0;	
	gchar		*tmp_title = NULL;
	gchar		*tmp_url = NULL;
	gchar		*tmp_key;
	gchar		*keyprefix;
	gint		tmp_type;
	entryPtr	ep;
	
	g_assert(NULL != key);
	g_assert(NULL != entrystore);

	g_mutex_lock(entries_lock);
	ep = g_hash_table_lookup(entries, (gpointer)key);
	g_mutex_unlock(entries_lock);
	
	if(NULL == ep) {
		print_status(_("internal error! could not resolve entry!\n"));
		return;
	}
	
	g_assert(NULL != ep->keyprefix);
	if(NULL != (topiter = (GtkTreeIter *)g_hash_table_lookup(folders, (gpointer)(ep->keyprefix)))) {
		if(FALSE == gtk_tree_model_iter_children(GTK_TREE_MODEL(entrystore), &iter, topiter)) {
			g_print(_("internal error! strange there should be entries in this directory..."));
			return;
		}
	} else {
		g_print(_("internal error! could not find directory list entry to insert this entry"));
		return;
	}

	do {
		gtk_tree_model_get(GTK_TREE_MODEL(entrystore), &iter,
				FS_TITLE, &tmp_title,
				FS_URL, &tmp_url,
				FS_KEY, &tmp_key,
				FS_TYPE, &tmp_type,
		  	      -1);

		if(tmp_type == type) {
			g_assert(NULL != tmp_key);
			if(0 == strcmp(tmp_key, key))
				found = 1;
		}

		if(found) {
			/* insert changed row contents */
			gtk_tree_store_set(entrystore, &iter,
					   FS_TITLE, title,
					   FS_URL, source,
					   FS_KEY, key,
					   FS_TYPE, type,
					  -1);
		}
		
		g_free(tmp_title);
		g_free(tmp_url);
		g_free(tmp_key);
		
		if(found)
			return;
		
		valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(entrystore), &iter);
	} while(valid);

	/* if we come here, this is a not yet added feed */
	addToEntryList(ep);
}

void addToEntryList(entryPtr ep) {
	GtkTreeIter	iter;
	GtkTreeIter	*topiter;
	
	g_assert(NULL != ep->key);
	g_assert(NULL != ep->keyprefix);

	if(NULL == (topiter = (GtkTreeIter *)g_hash_table_lookup(folders, (gpointer)(ep->keyprefix)))) {
		g_print(_("internal error! could not find directory list entry to insert this entry"));
		return;
	}
	
	gtk_tree_store_append(entrystore, &iter, topiter);
	gtk_tree_store_set(entrystore, &iter,
			   FS_TITLE, getDefaultEntryTitle(ep->key),
			   FS_URL, ep->source,
			   FS_KEY, ep->key,
			   FS_TYPE, ep->type,
			   -1);		   
}

/* entry list sorting functions */

void moveEntryPosition(gchar *keyprefix, gchar *key, gint mode) {
	GtkTreeIter		iter;
	GSList			*keylist;
	GConfValue		*element;
	entryPtr		ep;

	g_mutex_lock(entries_lock);
	ep = (entryPtr)g_hash_table_lookup(entries, key);
	g_mutex_unlock(entries_lock);	
	
	g_assert(NULL != ep);
	
	switch(mode) {
		case 0:
			moveUpEntryPositionInConfig(keyprefix, key);
			break;
		case 1:
			moveDownEntryPositionInConfig(keyprefix, key);
			break;
		case 2:
			sortEntryKeyList(keyprefix);
			break;
		default:
			g_error(_("invalid feed list reorder move\n"));
			break;
	}
	
	gtk_tree_store_clear(GTK_TREE_STORE(entrystore));	
	
	keylist = getEntryKeyList(keyprefix);
	while(NULL != keylist) {
		element = keylist->data;	// FIXME: remove gconf dependency (convert list to normal list)
		key = (gchar *)gconf_value_get_string(element);
	
		g_mutex_lock(entries_lock);
		ep = (entryPtr)g_hash_table_lookup(entries, key);
		g_mutex_unlock(entries_lock);
		
		addToEntryList(ep);
		
		keylist = g_slist_next(keylist);	
	}
	g_slist_free(keylist);
}

void moveUpEntryPosition(gchar *keyprefix, gchar *key) { moveEntryPosition(keyprefix, key, 0); }

void moveDownEntryPosition(gchar *keyprefix, gchar *key) { moveEntryPosition(keyprefix, key, 1); }

void sortEntries(gchar *keyprefix) { moveEntryPosition(keyprefix, NULL, 2); }
