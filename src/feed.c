/*
   common feed (channel) handling
   
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

#include <glib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "conf.h"
#include "common.h"

#include "support.h"
#include "cdf_channel.h"
#include "rss_channel.h"
#include "pie_feed.h"
#include "ocs_dir.h"
#include "opml.h"
#include "vfolder.h"
#include "netio.h"
#include "feed.h"
#include "favicon.h"

/* auto detection lookup table */
typedef struct detectStr {
	gint	type;
	gchar	*string;
} *detectStrPtr;

struct detectStr detectPattern[] = {
	{ FST_OCS,	"xmlns:ocs" 	},	/* must be before RSS!!! because OCS 0.4 is basically RDF */
	{ FST_OCS,	"<ocs:" 	},	/* must be before RSS!!! because OCS 0.4 is basically RDF */
	{ FST_OCS,	"<directory" 	},	/* OCS 0.5 */
	{ FST_RSS,	"<rdf:RDF" 	},
	{ FST_RSS,	"<rss" 		},
	{ FST_CDF,	"<channel>" 	},	/* have to be after RSS!!! */
	{ FST_PIE,	"<feed" 	},	
	{ FST_OPML,	"<opml" 	},
	{ FST_OPML,	"<outlineDocument" },	/* outlineDocument for older OPML */
	{ FST_OPML,	"<oml" 		},	/* OML is parsed as OPML */
	{ FST_INVALID,	NULL 		}
};

/* hash table to look up feed type handlers */
GHashTable	*feedHandler = NULL;

/* used to lookup a feed pointer specified by a key */
GHashTable	*feeds = NULL;

/* a list containing all items of all feeds, used for VFolder
   and searching functionality */
feedPtr		allItems = NULL;

GMutex * feeds_lock = NULL;

void registerFeedType(gint type, feedHandlerPtr fhp) {
	gint	*typeptr;
		
	if(NULL != (typeptr = (gint *)g_malloc(sizeof(gint)))) {
		*typeptr = type;
		g_hash_table_insert(feedHandler, (gpointer)typeptr, (gpointer)fhp);
	}
}

static gint autoDetectFeedType(gchar *url, gchar **data) {
	struct feed_request	request;
	detectStrPtr		pattern = detectPattern;
	gint			type = FST_INVALID;
	
	g_assert(NULL != pattern);
	g_assert(NULL != url);
	
	request.feedurl = g_strdup(url);
	if(NULL != (*data = downloadURL(&request))) {
		while(NULL != pattern->string) {	
			if(NULL != strstr(*data, pattern->string)) {
				type = pattern->type;
				break;
			}
			
			pattern++;
		} 
	}
	g_free(request.feedurl);
	g_free(request.lastmodified);

	return type;
}

/* initializing function, called upon initialization and each
   preference change */
void initBackend() {

	if(NULL == feeds_lock)
		feeds_lock = g_mutex_new();
	
	if(NULL == feeds) {
		g_mutex_lock(feeds_lock);		
		feeds = g_hash_table_new(g_str_hash, g_str_equal);
		g_mutex_unlock(feeds_lock);
	}

	if(NULL == allItems) {
		allItems = getNewFeedStruct();
		allItems->type = FST_VFOLDER;
	}
	
	feedHandler = g_hash_table_new(g_int_hash, g_int_equal);

	registerFeedType(FST_RSS,	initRSSFeedHandler());
	registerFeedType(FST_HELPFEED,	initRSSFeedHandler());
	registerFeedType(FST_OCS,	initOCSFeedHandler());
	registerFeedType(FST_CDF,	initCDFFeedHandler());
	registerFeedType(FST_PIE,	initPIEFeedHandler());
	registerFeedType(FST_OPML,	initOPMLFeedHandler());	
	registerFeedType(FST_VFOLDER,	initVFolderFeedHandler());
	
	initFolders();
}

/* function to create a new feed structure */
feedPtr getNewFeedStruct(void) {
	feedPtr		fp;
	
	/* initialize channel structure */
	if(NULL == (fp = (feedPtr) malloc(sizeof(struct feed)))) {
		g_error("not enough memory!\n");
		exit(1);
	}
	
	memset(fp, 0, sizeof(struct feed));
	fp->updateCounter = -1;
	fp->updateInterval = -1;
	fp->defaultInterval = -1;
	fp->available = FALSE;
	fp->type = FST_INVALID;
	
	return fp;
}

gint saveFeed(feedPtr fp) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur, feedNode, itemNode;
	GSList		*itemlist;
	gchar		*filename, *extension;
	gchar		*tmp;
	itemPtr		ip;
	gint		error = 0;
	gint		saveCount = 0;
	gint		saveMaxCount;
	char		*buf;
			
	saveMaxCount = getNumericConfValue(DEFAULT_MAX_ITEMS);	
	filename = getCacheFileName(fp->keyprefix, fp->key, getExtension(fp->type));

	if(NULL != (doc = xmlNewDoc("1.0"))) {	
		if(NULL != (feedNode = xmlNewDocNode(doc, NULL, "feed", NULL))) {
			xmlDocSetRootElement(doc, feedNode);		

			xmlNewTextChild(feedNode, NULL, "feedTitle", getFeedTitle(fp));
			
			if(NULL != fp->description)
				xmlNewTextChild(feedNode, NULL, "feedDescription", fp->description);

			tmp = g_strdup_printf("%d", fp->defaultInterval);
			xmlNewTextChild(feedNode, NULL, "feedUpdateInterval", tmp);
			g_free(tmp);
			
			tmp = g_strdup_printf("%d", (TRUE == fp->available)?1:0);
			xmlNewTextChild(feedNode, NULL, "feedStatus", tmp);
			g_free(tmp);

			itemlist = getFeedItemList(fp);
			while(NULL != itemlist) {
				saveCount ++;
				ip = itemlist->data;
				g_assert(NULL != ip);
				if(NULL != (itemNode = xmlNewChild(feedNode, NULL, "item", NULL))) {

					/* should never happen... */
					if(NULL == ip->title)
						ip->title = g_strdup("");
					xmlNewTextChild(itemNode, NULL, "title", ip->title);

					if(NULL != ip->description)
						xmlNewTextChild(itemNode, NULL, "description", ip->description);
					
					if(NULL != ip->source)
						xmlNewTextChild(itemNode, NULL, "source", ip->source);

					if(NULL != ip->id)
						xmlNewTextChild(itemNode, NULL, "id", ip->id);

					tmp = g_strdup_printf("%d", (TRUE == ip->readStatus)?1:0);
					xmlNewTextChild(itemNode, NULL, "readStatus", tmp);
					g_free(tmp);
					
					tmp = g_strdup_printf("%d", (TRUE == ip->marked)?1:0);
					xmlNewTextChild(itemNode, NULL, "mark", tmp);
					g_free(tmp);

					tmp = g_strdup_printf("%ld", ip->time);
					xmlNewTextChild(itemNode, NULL, "time", tmp);
					g_free(tmp);

				} else {
					g_warning(_("could not write XML item node!\n"));
					error = 1;
				}

				itemlist = g_slist_next(itemlist);
				
				if((saveCount >= saveMaxCount) && (IS_FEED(getFeedType(fp))))
					break;
			}
		} else {
			g_warning(_("could not create XML feed node for feed cache document!"));				error = 1;
		}
		xmlSaveFormatFileEnc(filename, doc, NULL, 1);
	} else {
		g_warning(_("could not create XML document!"));
		error = 1;
	}
	
	return error;
}

/* hash table foreach wrapper function */
static void saveFeedFunc(gpointer key, gpointer value, gpointer userdata) {
	feedPtr	fp = (feedPtr)value;
	
	if(IS_FEED(fp->type)) {
		saveFeed(fp);
	}
}

/* function to be called on program shutdown to save read stati */
void saveAllFeeds(void) {

	/* we must save here to save changed read flags */	
	print_status(_("saving all feeds..."));
	updateUI();
	
	g_mutex_lock(feeds_lock);
	g_hash_table_foreach(feeds, saveFeedFunc, NULL);
	g_mutex_unlock(feeds_lock);
}

void loadFavIcon(feedPtr fp) {
	gchar		*filename, *tmp;
	struct stat	statinfo;
	
	/* try to load a saved favicon */
	filename = getCacheFileName(fp->keyprefix, fp->key, "xpm");
	if(0 == stat(filename, &statinfo)) {
		/* remove path, because create_pixbuf allows no absolute pathnames */
		tmp = strrchr(filename, '/');
		fp->icon = gdk_pixbuf_scale_simple(create_pixbuf(++tmp), 16, 16, GDK_INTERP_BILINEAR);
	}
	g_free(filename);
}

/* function which is called to load a feed's cache file */
static feedPtr loadFeed(gint type, gchar *key, gchar *keyprefix) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	gchar		*filename, *tmp;
	feedPtr		fp = NULL;
	itemPtr		ip;

	filename = getCacheFileName(keyprefix, key, getExtension(type));
	doc = xmlParseFile(filename);
	
	while(1) {	
		if(NULL == doc) {
			print_status(g_strdup_printf(_("XML error wile reading cache file \"%s\" ! Cache file could not be loaded!"), filename));
			break;
		} 

		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			print_status(_("Empty document! Feed cache file should not be empty..."));
			break;
		}
		
		while(cur && xmlIsBlankNode(cur))
			cur = cur->next;

		if(xmlStrcmp(cur->name, BAD_CAST"feed")) {
			print_status(g_strdup_printf(_("\"%s\" is no valid cache file! Cannot read cache file!"), filename));
			break;		
		}
		
		fp = getNewFeedStruct();
		fp->available = TRUE;
		fp->key = key;
		fp->keyprefix = keyprefix;
		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			if ((!xmlStrcmp(cur->name, BAD_CAST"feedDescription"))) 
				fp->description = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
			if ((!xmlStrcmp(cur->name, BAD_CAST"feedTitle"))) 
				fp->title = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
			if ((!xmlStrcmp(cur->name, BAD_CAST"feedUpdateInterval"))) {
				tmp = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
				fp->defaultInterval = atoi(tmp);
				xmlFree(tmp);
			}
			if (!xmlStrcmp(cur->name, BAD_CAST"feedStatus")) {
				tmp = xmlNodeListGetString(doc, cur->xmlChildrenNode, 1);
				fp->available = (0 == atoi(tmp))?FALSE:TRUE;
				xmlFree(tmp);
			}

			if ((!xmlStrcmp(cur->name, BAD_CAST"item"))) {
				ip = parseCacheItem(doc, cur);
				addItem(fp, ip);
			}
			
			cur = cur->next;
		}

		break;
	}
	
	if(NULL != doc)
		xmlFreeDoc(doc);
	g_free(filename);
	
	if(NULL != fp)
		loadFavIcon(fp);
		
	return fp;
}

/* function to add a feed to the feed list */
feedPtr addFeed(gint type, gchar *url, gchar *key, gchar *keyprefix, gchar *feedname, gint interval) {
	feedPtr		new_fp;
	
	g_assert(NULL != key);
	if(NULL == (new_fp = loadFeed(type, key, keyprefix)))
		/* maybe cache file was deleted or entry has no cache 
		   (like help entries) so we reload the entry from its URL */
		new_fp = getNewFeedStruct();
	
	new_fp->type = type;
	new_fp->key = key;	
	new_fp->keyprefix = keyprefix;

	if(NULL != feedname) {
		g_free(new_fp->title);
		new_fp->title = feedname;
	}

	g_free(new_fp->source);
	new_fp->source = url;

	if(IS_FEED(type))
		new_fp->updateCounter = new_fp->updateInterval = interval;
	
	if(FALSE == getFeedAvailable(new_fp))
		setFeedUpdateCounter(new_fp, 0);

	g_mutex_lock(feeds_lock);
	g_hash_table_insert(feeds, (gpointer)key, (gpointer)new_fp);
	g_mutex_unlock(feeds_lock);

	addToFeedList(new_fp, TRUE);
	
	return new_fp;
}

/* function for first time loading of a newly subscribed feed */
feedPtr newFeed(gint type, gchar *url, gchar *keyprefix) {
	feedHandlerPtr		fhp;
	struct feed_request	request;
	unsigned char		*icodata;
	gchar			*baseurl;
	gchar			*key;
	gchar			*tmp;
	feedPtr			fp;

	fp = getNewFeedStruct();
	fp->source = url;
	
	g_assert(NULL != fp);
	if(FST_AUTODETECT == type) {
		/* if necessary download and detect type */
		if(FST_INVALID == (type = autoDetectFeedType(url, &(fp->data)))) {	// FIXME: pass fp to adjust URL
			showErrorBox(_("Could not detect feed type! Please manually select a feed type."));
			g_free(fp->data);
			return NULL;
		}
	} else {
		/* else only download */
		request.feedurl = g_strdup(url);
		fp->data = downloadURL(&request);
		g_free(request.feedurl);
		g_free(request.lastmodified);
	}

	if(NULL != fp->data) {
		/* parse data */
		g_assert(NULL != feedHandler);
		if(NULL != (fhp = g_hash_table_lookup(feedHandler, (gpointer)&type))) {
			g_assert(NULL != fhp->readFeed);
			(*(fhp->readFeed))(fp);
		} else {
			g_error(_("internal error! unknown feed type in newFeed()!"));
			return NULL;
		}
	}
	
	/* postprocess read feed */
	if(NULL != (key = addFeedToConfig(keyprefix, url, type))) {

		fp->type = type;
		fp->keyprefix = keyprefix;
		fp->key = key;

		if(TRUE == fp->available)		
			saveFeed(fp);

		/* try to download favicon */
		baseurl = g_strdup(url);
		if(NULL != (tmp = strstr(baseurl, "://"))) {
			tmp += 3;
			if(NULL != (tmp = strchr(tmp, '/'))) {
				*tmp = 0;
				tmp = g_strdup_printf("%s/favicon.ico", baseurl);
				request.lastmodified = NULL;
				request.feedurl = tmp;
				icodata = downloadURL(&request);
				g_free(request.feedurl);
				g_free(request.lastmodified);

				tmp = getCacheFileName(keyprefix, key, "xpm");
				if(NULL != icodata) {
					 convertIcoToXPM(tmp, icodata, 10000000);
					 loadFavIcon(fp);
				}
				g_free(tmp);
			}
		}
		g_free(baseurl);
	
		/* and finally add the feed to the feed list */
		g_mutex_lock(feeds_lock);
		g_hash_table_insert(feeds, (gpointer)getFeedKey(fp), (gpointer)fp);
		g_mutex_unlock(feeds_lock);
	} else {
		g_print(_("error! could not add feed to configuration!\n"));
		return NULL;
	}

	return fp;
}

/* Merges the feeds specified by old_fp and new_fp, so that
   the resulting feed is stored in the structure old_fp points to.
   The feed structure of new_fp 'll be freed. */
void mergeFeed(feedPtr old_fp, feedPtr new_fp) {
	GSList		*new_list, *old_list, *diff_list = NULL;
	itemPtr		new_ip, old_ip;
	gboolean	found, equal;
	gint		newcount = 0;

	if(TRUE == new_fp->available) {
		/* adjust the new_fp's items parent feed pointer to old_fp, just
		   in case they are reused... */
		new_list = new_fp->items;
		while(new_list) {
			new_ip = new_list->data;
			new_ip->fp = old_fp;	
			new_list = g_slist_next(new_list);
		}

		/* merge item lists ... */
		new_list = new_fp->items;
		while(new_list) {
			new_ip = new_list->data;

			found = FALSE;
			/* scan the old list to see if the new list item did already exist */
			old_list = old_fp->items;
			while(old_list) {
				old_ip = old_list->data;

				/* try to compare the two items */

				/* both items must have either ids or none */
				if(((old_ip->id == NULL) && (new_ip->id != NULL)) ||
				   ((old_ip->id != NULL) && (new_ip->id == NULL))) {	
					/* cannot be equal (different ids) so compare to 
					   next old item */
					old_list = g_slist_next(old_list);
			   		continue;
				}

				/* compare titles and HTML descriptions */
				equal = TRUE;

				if(((old_ip->title != NULL) && (new_ip->title != NULL)) && 
				    (0 != strcmp(old_ip->title, new_ip->title)))		
			    		equal = FALSE;

				if(((old_ip->description != NULL) && (new_ip->description != NULL)) && 
				    (0 != strcmp(old_ip->description, new_ip->description)))
			    		equal = FALSE;

				if(NULL != old_ip->id) {			
					/* if they have ids, compare them */
					if(0 == strcmp(old_ip->id, new_ip->id)){
						found = TRUE;
						break;
					}
				} 

				if(equal) {
					found = TRUE;
					break;					
				}

				old_list = g_slist_next(old_list);
			}

			if(!found) {
				/* Check if feed filters allow display of this item, we don't
				   delete the item because there can be vfolders which display
				   it. To allow this the parent feed does store the item, but
				   hides it. */
				if(FALSE == checkNewItem(new_ip)) {
					new_ip->hidden = TRUE;
				} else {
					doTrayIcon();
					increaseUnreadCount(old_fp);
				}
				newcount ++;
				diff_list = g_slist_append(diff_list, (gpointer)new_ip);
			} else {
				/* if the item was found but has other contents -> update */
				if(!equal) {
					doTrayIcon();
					g_free(old_ip->title);
					g_free(old_ip->description);
					old_ip->title = g_strdup(new_ip->title);
					old_ip->description = g_strdup(new_ip->description);
					markItemAsUnread(old_ip);
					newcount ++;
				} else {
					new_ip->readStatus = TRUE;
				}
			}

			new_list = g_slist_next(new_list);

			/* any found new item list items are not needed anymore */
			if(found && (old_fp->type != FST_HELPFEED)) { 
				new_ip->fp = new_fp;	/* because freeItem() would decrease the unread counter of old_fp */
				freeItem(new_ip);
			}
		}
		
		/* now we distinguish between incremental merging
		   for all normal feeds, and skipping old item
		   merging for help feeds... */
		if(old_fp->type != FST_HELPFEED) {
			g_slist_free(new_fp->items);	/* dispose new item list */
			
			if(NULL == diff_list)
				print_status(g_strdup_printf(_("\"%s\" has no new items."), old_fp->title));
			else
				print_status(g_strdup_printf(_("\"%s\" has %d new items."), old_fp->title, newcount));
			old_list = g_slist_concat(diff_list, old_fp->items);
			old_fp->items = old_list;
		} else {
			/* free old list and items of old list */
			old_list = old_fp->items;
			while(NULL != old_list) {
				freeItem((itemPtr)old_list->data);
				old_list = g_slist_next(old_list);
			}
			g_slist_free(old_fp->items);
			
			/* parent feed pointers are already correct, we can reuse simply the new list */
			old_fp->items = new_fp->items;
		}		

		/* copy description */
		g_free(old_fp->description);
		old_fp->description = new_fp->description;
	}
	
	old_fp->available = new_fp->available;
	g_free(new_fp->source);
	g_free(new_fp->title);
	g_free(new_fp);			/* dispose new feed structure */
}

void removeFeed(feedPtr fp) {
	gchar	*filename;
	
	filename = getCacheFileName(fp->keyprefix, fp->key, getExtension(fp->type));
	if(0 != unlink(filename)) {
		g_warning(g_strdup_printf(_("Could not delete cache file %s! Please remove manually!"), filename));
	}
	g_hash_table_remove(feeds, (gpointer)fp);
	removeFeedFromConfig(fp->keyprefix, fp->key);
}

/* "foreground" user caused update executed in the main thread to update
   the selected and displayed feed */
void updateFeed(feedPtr fp) {
	
	print_status(g_strdup_printf("updating \"%s\"", getFeedTitle(fp)));
	requestUpdate(fp);
}

static void updateFeedHelper(gpointer key, gpointer value, gpointer userdata) {
	feedPtr		fp = (feedPtr)value;

	g_assert(NULL != fp);
	updateFeed(fp);
}

/* this method is called upon the refresh all button... */
void updateAllFeeds(void) {

	print_status(g_strdup(_("updating all feeds...")));
	g_mutex_lock(feeds_lock);
	g_hash_table_foreach(feeds, updateFeedHelper, NULL);
	g_mutex_unlock(feeds_lock);
}

void addItem(feedPtr fp, itemPtr ip) {

	ip->fp = fp;
	if(FALSE == ip->readStatus)
		increaseUnreadCount(fp);
	fp->items = g_slist_append(fp->items, (gpointer)ip);
	allItems->items = g_slist_append(allItems->items, (gpointer)ip);
}

/* ---------------------------------------------------------------------------- */
/* feed attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

feedPtr getFeed(gchar *feedkey) {
	feedPtr	fp;

	g_assert(NULL != feeds);
	g_mutex_lock(feeds_lock);
	if(NULL == (fp = g_hash_table_lookup(feeds, (gpointer)feedkey))) {
		g_warning(g_strdup_printf(_("internal error! there is no feed assigned to feedkey %s!"), feedkey));
	}
	g_mutex_unlock(feeds_lock);	
	
	return fp;
}

gint getFeedType(feedPtr fp) { return fp->type; }
gpointer getFeedIcon(feedPtr fp) { return fp->icon; }
gchar * getFeedKey(feedPtr fp) { return fp->key; }
gchar * getFeedKeyPrefix(feedPtr fp) { return fp->keyprefix; }

void increaseUnreadCount(feedPtr fp) { fp->unreadCount++; }
void decreaseUnreadCount(feedPtr fp) { fp->unreadCount--; }
gint getFeedUnreadCount(feedPtr fp) { return fp->unreadCount; }

gint getFeedDefaultInterval(feedPtr fp) { return fp->defaultInterval; }
gint getFeedUpdateInterval(feedPtr fp) { return fp->updateInterval; }

void setFeedUpdateInterval(feedPtr fp, gint interval) { 

	fp->updateInterval = interval; 
	setFeedUpdateIntervalInConfig(fp->key, interval);
}

gint getFeedUpdateCounter(feedPtr fp) { return fp->updateCounter; }
void setFeedUpdateCounter(feedPtr fp, gint count) { fp->updateCounter = count; }

gboolean getFeedAvailable(feedPtr fp) { return fp->available; }

gchar * getFeedTitle(feedPtr fp) { 

	if(NULL != fp->title)
		return fp->title; 
	else
		return fp->source;
}

void setFeedTitle(feedPtr fp, gchar *title) {

	if(NULL != fp->title)
		g_free(fp->title);

	fp->title = title;
	setFeedTitleInConfig(fp->key, title);
}

gchar * getFeedDescription(feedPtr fp) { return fp->description; }
gchar * getFeedSource(feedPtr fp) { return fp->source; }

void setFeedSource(feedPtr fp, gchar *source) {

	if(NULL != fp->source)
		g_free(fp->source);

	fp->source = source;
	setFeedURLInConfig(fp->key, source);
}

GSList * getFeedItemList(feedPtr fp) { return fp->items; }

/* Method to copy the infos of the structure given by
   new_fp to the structure fp points to. The feed infos
   of new_fp are freed afterwards. */
void copyFeed(feedPtr fp, feedPtr new_fp) {
	gchar		*tmp_key, *tmp_keyprefix, *tmp_title;
	feedPtr		tmp_fp;
	itemPtr		ip;
	GSList		*item;
	
	/* To prevent updating feed ptr in the tree store and
	   feeds hashtable we reuse the old structure! */
	g_free(new_fp->title),
	new_fp->key = fp->key;			/* reuse some attributes */
	new_fp->keyprefix = fp->keyprefix;
	new_fp->title = fp->title;
	new_fp->type = fp->type;
	
	tmp_fp = getNewFeedStruct();
	memcpy(tmp_fp, fp, sizeof(struct feed));	/* make a copy of the old fp pointers... */
	memcpy(fp, new_fp, sizeof(struct feed));
	tmp_fp->key = NULL;			/* to prevent removal of reused attributes... */
	tmp_fp->items = NULL;
	tmp_fp->title = NULL;
	freeFeed(tmp_fp);			/* free all infos allocated by old feed */
	g_free(new_fp);
	
	/* adjust item parent pointer */
	item = getFeedItemList(fp);
	while(NULL != item) {
		ip = item->data;
		ip->fp = fp;
		item = g_slist_next(item);
	}
}

/* method to free all memory allocated by a feed */
void freeFeed(feedPtr fp) {
	GSList	*item;

	/* free items */
	item = getFeedItemList(fp);
	while(NULL != item) {
		allItems->items = g_slist_remove(allItems->items, item->data);
		freeItem(item->data);
		item = g_slist_next(item);
	}
	/* free feed info */
	g_free(fp->title);
	g_free(fp->description);
	g_free(fp->source);
	g_free(fp->key);
	g_free(fp);
}
