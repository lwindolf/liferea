/**
 * @file feed.c common feed handling
 * 
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version. 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <glib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <string.h>
#include <unistd.h> /* For unlink() */

#include "conf.h"
#include "common.h"

#include "support.h"
#include "cdf_channel.h"
#include "rss_channel.h"
#include "pie_feed.h"
#include "ocs_dir.h"
#include "opml.h"
#include "vfolder.h"
#include "net/netio.h"
#include "feed.h"
#include "folder.h"
#include "favicon.h"
#include "callbacks.h"
#include "filter.h"
#include "update.h"

#include "ui_feedlist.h"
#include "ui_tray.h"
#include "htmlview.h"

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
	{ FST_CDF,	"<channel>" 	},	/* has to be after RSS!!! */
	{ FST_PIE,	"<feed" 	},
	{ FST_OPML,	"<opml" 	},
	{ FST_OPML,	"<outlineDocument" },	/* outlineDocument for older OPML */
	{ FST_OPML,	"<oml" 		},	/* OML is parsed as OPML */
	/* { FST_HTML	"<html"		},*/	/* HTML with link discovery */
	/* { FST_HTML	"<HTML"		},*/	/* HTML with link discovery */
	{ FST_INVALID,	NULL 		}
};

/* hash table to look up feed type handlers */
GHashTable	*feedHandler = NULL;

/* a list containing all items of all feeds, used for VFolder
   and searching functionality */
feedPtr		allItems = NULL;

/* prototypes */
static gboolean update_timer_main(gpointer data);

/* ------------------------------------------------------------ */
/* feed type registration					*/
/* ------------------------------------------------------------ */

static void feed_register_type(gint type, feedHandlerPtr fhp) {
	gint	*typeptr;
		
	typeptr = g_new0(gint, 1);
	*typeptr = type;
	g_hash_table_insert(feedHandler, (gpointer)typeptr, (gpointer)fhp);
}

gint feed_detect_type(gchar *data) {
	detectStrPtr		pattern = detectPattern;
	gint			type = FST_INVALID;
	
	g_assert(NULL != pattern);
	g_assert(NULL != data);
	
	while(NULL != pattern->string) {	
		if(NULL != strstr(data, pattern->string)) {
			type = pattern->type;
			break;
		}
		
		pattern++;
	} 
		
	return type;
}

/* initializing function, only called upon startup */
void feed_init(void) {

	allItems = feed_new();
	allItems->type = FST_VFOLDER;
	
	feedHandler = g_hash_table_new(g_int_hash, g_int_equal);

	feed_register_type(FST_RSS,		initRSSFeedHandler());
	feed_register_type(FST_HELPFEED,	initRSSFeedHandler());
	feed_register_type(FST_OCS,		initOCSFeedHandler());
	feed_register_type(FST_CDF,		initCDFFeedHandler());
	feed_register_type(FST_PIE,		initPIEFeedHandler());
	feed_register_type(FST_OPML,		initOPMLFeedHandler());
	feed_register_type(FST_VFOLDER,		initVFolderFeedHandler());
	
	update_thread_init();	/* start thread for update request processing */
	
	/* setup one minute timer for automatic updating */
 	g_timeout_add(60*1000, update_timer_main, NULL);

	initFolders();
}

/* function to create a new feed structure */
feedPtr feed_new(void) {
	feedPtr		fp;
	
	fp = g_new0(struct feed, 1);

	/* we dont allocate a request structure this is done
	   during cache loading or first update! */
	
	fp->updateInterval = -1;
	fp->defaultInterval = -1;
	fp->available = FALSE;
	fp->type = FST_INVALID;
	fp->parseErrors = NULL;
	fp->ui_data = NULL;
	fp->updateRequested = FALSE;
	
	return fp;
}

void feed_save(feedPtr fp) {
	xmlDocPtr 	doc;
	xmlNodePtr 	feedNode, itemNode;
	GSList		*itemlist;
	gchar		*filename;
	gchar		*tmp;
	itemPtr		ip;
	gint		saveCount = 0;
	gint		saveMaxCount;
			
	saveMaxCount = getNumericConfValue(DEFAULT_MAX_ITEMS);	
	filename = getCacheFileName(fp->id, NULL);

	if(NULL != (doc = xmlNewDoc("1.0"))) {	
		if(NULL != (feedNode = xmlNewDocNode(doc, NULL, "feed", NULL))) {
			xmlDocSetRootElement(doc, feedNode);

			xmlNewTextChild(feedNode, NULL, "feedTitle", feed_get_title(fp));
			
			if(NULL != fp->description)
				xmlNewTextChild(feedNode, NULL, "feedDescription", fp->description);

			tmp = g_strdup_printf("%d", fp->defaultInterval);
			xmlNewTextChild(feedNode, NULL, "feedUpdateInterval", tmp);
			g_free(tmp);
			
			tmp = g_strdup_printf("%d", (TRUE == fp->available)?1:0);
			xmlNewTextChild(feedNode, NULL, "feedStatus", tmp);
			g_free(tmp);
			
			if(NULL != fp->request) {
				if(NULL != ((struct feed_request *)(fp->request))->lastmodified)
					xmlNewTextChild(feedNode, NULL, "feedLastModified", 
							((struct feed_request *)(fp->request))->lastmodified);
			}

			itemlist = feed_get_item_list(fp);
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
				}

				itemlist = g_slist_next(itemlist);
				
				if((saveCount >= saveMaxCount) && (IS_FEED(feed_get_type(fp))))
					break;
			}
		} else {
			g_warning(_("could not create XML feed node for feed cache document!"));
		}
		xmlSaveFormatFileEnc(filename, doc, NULL, 1);
		g_free(filename);
	} else {
		g_warning(_("could not create XML document!"));
	}
}

/* function which is called to load a feed's cache file */
static feedPtr loadFeed(gint type, gchar *id) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	gchar		*filename, *tmp, *data = NULL;
	feedPtr		fp;
	int		error = 0;
	
	filename = getCacheFileName(id, NULL);
	
	if((!g_file_get_contents(filename, &data, NULL, NULL)) || (*data == 0)) {
		g_message("Error while reading cache file");
		ui_mainwindow_set_status_bar(_("Error while reading cache file \"%s\" ! Cache file could not be loaded!"), filename);
		g_free(filename);
		return NULL;
	}

	fp = feed_new();		
	do {
		g_assert(NULL != data);

		if(NULL == (doc = parseBuffer(data, &(fp->parseErrors)))) {
			addToHTMLBuffer(&(fp->parseErrors), g_strdup_printf(_("<p>XML error while parsing cache file! Feed cache file \"%s\" could not be loaded!</p>"), filename));
			error = 1;
			break;
		} 

		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Empty document!</p>"));
			error = 1;
			break;
		}
		
		while(cur && xmlIsBlankNode(cur))
			cur = cur->next;

		if(xmlStrcmp(cur->name, BAD_CAST"feed")) {
			addToHTMLBuffer(&(fp->parseErrors), g_strdup_printf(_("<p>\"%s\" is no valid cache file! Cannot read cache file!</p>"), filename));
			error = 1;
			break;		
		}

		fp->available = TRUE;		
		fp->id = g_strdup(id);

		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			tmp = CONVERT(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));

			if(!xmlStrcmp(cur->name, BAD_CAST"feedDescription")) {
				fp->description = g_strdup(tmp);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedTitle")) {
				feed_set_title(fp, tmp);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedUpdateInterval")) {
				fp->defaultInterval = atoi(tmp);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedStatus")) {
				fp->available = (0 == atoi(tmp))?FALSE:TRUE;
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedLastModified")) {
				update_request_new(fp);
				((struct feed_request *)(fp->request))->lastmodified = g_strdup(tmp);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"item")) {
				feed_add_item((feedPtr)fp, parseCacheItem(doc, cur));
			}			
			g_free(tmp);	
			cur = cur->next;
		}
	
		loadFavIcon(fp);
	} while (FALSE);
	
	if(0 != error) {
		ui_mainwindow_set_status_bar(_("There were errors while parsing cache file \"%s\"!"), filename);
	}
	
	if (NULL != data)
		g_free(data);
	if(NULL != doc)
		xmlFreeDoc(doc);
	g_free(filename);
		
	return fp;
}

feedPtr feed_add(gint type, gchar *url, struct folder *parent, gchar *feedName, gchar *id, gint interval, gboolean showPropDialog) {
	feedPtr fp = NULL;
	gboolean needs_update=FALSE;
	g_message("feed_add(type=%d,url=%s,parent=%x,name=%s,id=%s,interval=%d,propd=%d)",type,url,parent,feedName,id,interval,showPropDialog);

	g_assert(url != NULL);

	if (id != NULL && type != FST_HELPFEED)
		fp = loadFeed(type, id);
	
	if (fp == NULL) {
		fp = feed_new();
		if (id == NULL) {
			fp->id = conf_new_id();
			needs_update=TRUE;
		} else
			fp->id = g_strdup(id);
	}
	
	fp->type = type;
	fp->parent = parent;
	fp->displayProps = showPropDialog;

	if(NULL != feedName)
		feed_set_title(fp, feedName);
	
	if(NULL != url) {
		fp->source = g_strdup(url);
	} else if (NULL == fp->source) {
		fp->source = g_strdup(_("http://example.com/this.url.is.invalid"));
	}

	feed_set_update_interval(fp, interval);

	folder_add_feed(parent, fp, -1);
	ui_folder_add_feed(fp, -1);

	if(needs_update) {
		favicon_download(fp);	/* FIXME: I think thats evil, should be done only on first download!!! (09.04.04, Lars)*/
		feed_update(fp);
	}

	ui_update_feed(fp);
	return fp;
}

/* Merges the feeds specified by old_fp and new_fp, so that
   the resulting feed is stored in the structure old_fp points to.
   The feed structure of new_fp 'll be freed. */
void feed_merge(feedPtr old_fp, feedPtr new_fp) {
	GSList		*new_list, *old_list, *diff_list = NULL;
	itemPtr		new_ip, old_ip;
	gboolean	found, equal=FALSE;
	gint		newcount = 0;
	gint		traycount = 0;

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
			/* scan the old list to see if the new_fp item does already exist */
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
					feed_increase_unread_counter(old_fp);
					traycount++;
				}
				newcount++;
				diff_list = g_slist_append(diff_list, (gpointer)new_ip);
			} else {
				/* if the item was found but has other contents -> update */
				if(!equal) {
					g_free(old_ip->title);
					g_free(old_ip->description);
					old_ip->title = g_strdup(new_ip->title);
					old_ip->description = g_strdup(new_ip->description);
					markItemAsUnread(old_ip);
					newcount++;
					traycount++;
				} else {
					new_ip->readStatus = TRUE;
				}
			}

			new_list = g_slist_next(new_list);

			/* any found new_fp items are not needed anymore */
			if(found && (old_fp->type != FST_HELPFEED)) { 
				new_ip->fp = new_fp;	/* else freeItem() would decrease the unread counter of old_fp */
				freeItem(new_ip);
			}
		}
		
		/* now we distinguish between incremental merging
		   for all normal feeds, and skipping old item
		   merging for help feeds... */
		if(old_fp->type != FST_HELPFEED) {
			g_slist_free(new_fp->items);	/* dispose new item list */
			
			if(NULL == diff_list)
				ui_mainwindow_set_status_bar(_("\"%s\" has no new items."), old_fp->title);
			else 
				ui_mainwindow_set_status_bar(_("\"%s\" has %d new items."), old_fp->title, newcount);
			
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

		/* copy description and default update interval */
		g_free(old_fp->description);
		old_fp->description = g_strdup(new_fp->description);
		old_fp->defaultInterval = new_fp->defaultInterval;
	}

	g_free(old_fp->parseErrors);
	old_fp->parseErrors = new_fp->parseErrors;
	new_fp->parseErrors = NULL;
	old_fp->available = new_fp->available;
	new_fp->items = NULL;
	feed_free(new_fp);
	
	doTrayIcon(traycount);		/* finally update the tray icon */
}

/**
 * method to be called by other threads to update feeds
 */
void feed_update(feedPtr fp) { 
	gchar		*source;
	
	g_assert(NULL != fp);

	g_print("Queueing update of %s\n", feed_get_title(fp));
	
	if(TRUE == fp->updateRequested) {
		ui_mainwindow_set_status_bar("This feed \"%s\" is already being updated!", feed_get_title(fp));
		return;
	}

	ui_mainwindow_set_status_bar("updating \"%s\"", feed_get_title(fp));
	
	if(NULL == (source = feed_get_source(fp))) {
		g_warning("Feed source is NULL! This should never happen - cannot update!");
		return;
	}
	
	feed_reset_update_counter(fp);
	fp->updateRequested = TRUE;

	if(NULL == fp->request)
		update_request_new(fp);

	/* prepare request url (strdup because it might be
	   changed on permanent HTTP redirection in netio.c) */
	((struct feed_request *)fp->request)->feedurl = g_strdup(source);

	update_thread_add_request((struct feed_request *)fp->request);
}

static void feed_check_update_counter(feedPtr fp) {
	GTimeVal	now;
	
	g_get_current_time(&now);

	if(feed_get_update_interval(fp) > 0 && fp->scheduledUpdate.tv_sec <= now.tv_sec)
		feed_update(fp);
}

// FIXME: does this function belong here? I think that it belongs in update.c -Nathan
static gboolean update_timer_main(void *data) {

	g_message("Checking to see if feeds need to be updated");
	ui_feedlist_do_for_all(NULL, ACTION_FILTER_FEED, (gpointer)feed_check_update_counter);
 
	return TRUE;
}

void feed_add_item(feedPtr fp, itemPtr ip) {

	ip->fp = fp;
	if(FALSE == ip->readStatus)
		feed_increase_unread_counter(fp);
	fp->items = g_slist_append(fp->items, (gpointer)ip);
	allItems->items = g_slist_append(allItems->items, (gpointer)ip);
}

/* ---------------------------------------------------------------------------- */
/* feed attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

gchar *feed_get_id(feedPtr fp) {return fp->id;};
void feed_set_type(feedPtr fp, gint type) {
	fp->type = type;
	conf_feedlist_schedule_save();
}
gint feed_get_type(feedPtr fp) { return fp->type; }
gpointer feed_get_favicon(feedPtr fp) { return fp->icon; }

void feed_increase_unread_counter(feedPtr fp) {
	fp->unreadCount++;
}
void feed_decrease_unread_counter(feedPtr fp) {
	fp->unreadCount--;
}
gint feed_get_unread_counter(feedPtr fp) { return fp->unreadCount; }

gint feed_get_default_update_interval(feedPtr fp) { return fp->defaultInterval; }
gint feed_get_update_interval(feedPtr fp) { return fp->updateInterval; }

void feed_set_update_interval(feedPtr fp, gint interval) {
	fp->updateInterval = interval; 

	if (interval > 0)
		feed_reset_update_counter(fp);
	conf_feedlist_schedule_save();
}

void feed_reset_update_counter(feedPtr fp) {

	g_get_current_time(&fp->scheduledUpdate);
	printf("%ld: ", fp->scheduledUpdate.tv_sec);
	fp->scheduledUpdate.tv_sec += fp->updateInterval*60;
	printf("Reseting update counter for %s to %ld.\n", fp->title, fp->scheduledUpdate.tv_sec);
}

gboolean feed_get_available(feedPtr fp) { return fp->available; }

/* Returns a HTML string describing the last retrieval error 
   of this feed. Should only be called when feed_get_available
   returns FALSE. Caller must free returned string! */
gchar * feed_get_error_description(feedPtr fp) {
	gchar		*tmp1, *tmp2 = NULL, *buffer = NULL;
	gint 		httpstatus;
	gboolean	errorFound = FALSE;
	
	if(NULL == fp->request)
		return NULL;
		
	if((0 == ((struct feed_request *)fp->request)->problem) &&
	   (NULL == fp->parseErrors))
		return NULL;
	
	addToHTMLBuffer(&buffer, UPDATE_ERROR_START);
	
	httpstatus = ((struct feed_request *)fp->request)->lasthttpstatus;
	/* httpstatus is always zero for file subscriptions... */
	if((200 != httpstatus) && (0 != httpstatus)) {
		/* first specific codes */
		switch(httpstatus) {
			case 401:tmp2 = g_strdup(_("The feed no longer exists. Please unsubscribe!"));break;
			case 402:tmp2 = g_strdup(_("Payment Required"));break;
			case 403:tmp2 = g_strdup(_("Access Forbidden"));break;
			case 404:tmp2 = g_strdup(_("Ressource Not Found"));break;
			case 405:tmp2 = g_strdup(_("Method Not Allowed"));break;
			case 406:tmp2 = g_strdup(_("Not Acceptable"));break;
			case 407:tmp2 = g_strdup(_("Proxy Authentication Required"));break;
			case 408:tmp2 = g_strdup(_("Request Time-Out"));break;
			case 410:tmp2 = g_strdup(_("Gone. Resource doesn't exist. Please unsubscribe!"));break;
		}

		/* next classes */
		if(NULL == tmp2) {
			switch(httpstatus / 100) {
				case 3:tmp2 = g_strdup(_("Feed not available: Server signalized unsupported redirection!"));break;
				case 4:tmp2 = g_strdup(_("Client Error"));break;
				case 5:tmp2 = g_strdup(_("Server Error"));break;
				default:tmp2 = g_strdup(_("(unknown error class)"));break;
			}
		}
		errorFound = TRUE;
		tmp1 = g_strdup_printf(_(HTTP_ERROR_TEXT), httpstatus, tmp2);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
		g_free(tmp2);
	}
	
	/* add parsing error messages */
	if(NULL != fp->parseErrors) {
		if(errorFound)
			addToHTMLBuffer(&buffer, HTML_NEWLINE);			
		errorFound = TRUE;
		tmp1 = g_strdup_printf(_(PARSE_ERROR_TEXT), fp->parseErrors);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	/* if none of the above error descriptions matched... */
	if(!errorFound) {
		tmp1 = g_strdup_printf(_("There was a problem while reading this subscription. Please check the URL and console output!"));
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	addToHTMLBuffer(&buffer, UPDATE_ERROR_END);
	
	return buffer;
}

gchar * feed_get_title(feedPtr fp) { 

	if(NULL != fp->title)
		return fp->title; 
	else
		return fp->source;
}

void feed_set_title(feedPtr fp, gchar *title) {
	if (fp->title)
		g_free(fp->title);
	fp->title = g_strdup(title);
	conf_feedlist_schedule_save();
}

gchar * feed_get_description(feedPtr fp) { return fp->description; }
gchar * feed_get_source(feedPtr fp) { return fp->source; }

void feed_set_source(feedPtr fp, gchar *source) {
	if(fp->source)
		g_free(fp->source);

	fp->source = g_strdup(source);
	conf_feedlist_schedule_save();
}

GSList * feed_get_item_list(feedPtr fp) { return fp->items; }

/* method to free all items of a feed */
void feed_clear_item_list(feedPtr fp) {
	GSList	*item;
	
	item = fp->items;
	while(NULL != item) {
		allItems->items = g_slist_remove(allItems->items, item->data);
		freeItem(item->data);
		item = g_slist_next(item);
	}
	fp->items = NULL;
}

void feed_mark_all_items_read(feedPtr fp) {
	GSList	*item;
	
	item = fp->items;
	while(NULL != item) {
		markItemAsRead((itemPtr)item->data);
		item = g_slist_next(item);
	}
}

/* Method to copy the info payload of the structure given by
   new_fp to the structure fp points to. Essential model
   specific keys of fp are kept. The feed structure of new_fp 
   is freed afterwards. 
   
   This method is primarily used for feeds which do not want
   to incrementally update items like directories. */
void feed_copy(feedPtr fp, feedPtr new_fp) {
	feedPtr		tmp_fp;
	itemPtr		ip;
	GSList		*item;

	/* To prevent updating feed ptr in the tree store and
	   feeds hashtable we reuse the old structure! */
	
	/* in the next step we will copy the new_fp structure
	   to fp, but we need to keep some fp attributes... */
	g_free(new_fp->title);
	new_fp->id = fp->id;
	g_free(new_fp->source);
	new_fp->title = fp->title;
	new_fp->source = fp->source;
	new_fp->type = fp->type;
	new_fp->request = fp->request;
	new_fp->ui_data = fp->ui_data;
	new_fp->ui_data = fp->ui_data;
	new_fp->parent = fp->parent;
	
	tmp_fp = feed_new();
	memcpy(tmp_fp, fp, sizeof(struct feed));	/* make a copy of the old fp pointers... */
	memcpy(fp, new_fp, sizeof(struct feed));
	tmp_fp->id = NULL;				/* to prevent removal of reused attributes... */
	tmp_fp->items = NULL;
	tmp_fp->title = NULL;
	tmp_fp->source = NULL;
	tmp_fp->request = NULL;
	tmp_fp->ui_data = NULL;
	tmp_fp->parent = NULL;
	feed_free(tmp_fp);				/* we use tmp_fp to free almost all infos
							   allocated by old feed structure */
	g_free(new_fp);
	
	/* adjust item parent pointer of new items from new_fp to fp */
	item = feed_get_item_list(fp);
	while(NULL != item) {
		ip = item->data;
		ip->fp = fp;
		item = g_slist_next(item);
	}
}

/* method to totally erase a feed, remove it from the config, etc.... */
void feed_free(feedPtr fp) {
	gchar *filename = NULL;
	
	g_assert(IS_FEED(fp->type) || IS_DIRECTORY(fp->type));
	
	if (fp->id && fp->id[0] != '\0')
		filename = getCacheFileName(fp->id, NULL);
	
	/* free UI info */
	if(fp->ui_data)
		ui_folder_remove_feed(fp);
		
	/* free items */

	/* FIXME: Move this to a better place. The cache file does not
	   need to always be deleted, for example when freeing a
	   feedstruct used for updating. */
	if(filename && 0 != unlink(filename)) {
		g_warning(_("Could not delete cache file %s! Please remove manually!"), filename);
	}

	// FIXME: free filter structures too when implemented
	
	/* Don't free active feed requests here, because they might
	   still be processed in the update queues! Abandoned
	   requests are free'd in update.c. */
	if (fp->request != NULL) {
		if(FALSE == fp->updateRequested)
			update_request_free(fp->request);
		else
			((struct feed_request *)fp->request)->fp = NULL;
	}

	feed_clear_item_list(fp);

	if (fp->id) {
		removeFavIcon(fp);
		conf_feedlist_schedule_save();
		g_free(fp->id);
	}
	
	if (fp->parent) {
		folderPtr folder = fp->parent;
		ui_update_folder(folder);
	}
	g_free(fp->title);
	g_free(fp->description);
	g_free(fp->source);
	g_free(fp->parseErrors);
	g_free(fp);

}

void feed_set_pos(feedPtr fp, folderPtr dest_folder, int position) {
	gboolean ui=FALSE;
	g_assert(NULL != dest_folder);
	g_assert(NULL != fp);
	g_assert(NULL != fp->parent);
	
	if(fp->ui_data) {
		ui=TRUE;
		ui_folder_remove_feed(fp);
	}

	fp->parent = dest_folder;
	/* move key in configuration */
	
	if(ui) {
		ui_folder_add_feed(fp, position);
	}
	conf_feedlist_schedule_save();
}
