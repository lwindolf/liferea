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
#include <time.h>
#include <unistd.h> /* For unlink() */

#include "conf.h"
#include "common.h"

#include "support.h"
#include "html.h"
#include "cdf_channel.h"
#include "rss_channel.h"
#include "pie_feed.h"
#include "ocs_dir.h"
#include "opml.h"
#include "vfolder.h"
#include "feed.h"
#include "folder.h"
#include "favicon.h"
#include "callbacks.h"
#include "filter.h"
#include "update.h"
#include "debug.h"
#include "metadata.h"

#include "ui_queue.h"	/* FIXME: Remove ui_* include from core code */
#include "ui_feed.h"
#include "ui_feedlist.h"
#include "ui_tray.h"
#include "ui_notification.h"
#include "htmlview.h"

/* auto detection lookup table */
static GSList *feedhandlers;

struct feed_type {
	gint id_num;
	gchar *id_str;
};

/* prototypes */
static void feed_set_error_description(feedPtr fp, gint httpstatus, gint resultcode);
static void feed_replace(feedPtr fp, feedPtr new_fp);


/* initializing function, only called upon startup */
void feed_init(void) {

	feedhandlers = g_slist_append(feedhandlers, ocs_init_feed_handler()); /* Must come before RSS/RDF */
	feedhandlers = g_slist_append(feedhandlers, rss_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, cdf_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, pie_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, opml_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, vfolder_init_feed_handler());
	
	initFolders();
}

/* function to create a new feed structure */
feedPtr feed_new(void) {
	feedPtr		fp;
	
	fp = g_new0(struct feed, 1);

	/* we don't allocate a request structure this is done
	   during cache loading or first update! */
	
	fp->updateInterval = -1;
	fp->defaultInterval = -1;
	fp->type = FST_FEED;
	fp->cacheLimit = CACHE_DEFAULT;
	
	return fp;
}

/* ------------------------------------------------------------ */
/* feed type registration					*/
/* ------------------------------------------------------------ */

const gchar *feed_type_fhp_to_str(feedHandlerPtr fhp) {
	if (fhp == NULL)
		return NULL;
	return fhp->typeStr;
}

feedHandlerPtr feed_type_str_to_fhp(const gchar *str) {
	GSList *iter;
	feedHandlerPtr fhp = NULL;
	if (str == NULL)
		return NULL;

	for(iter = feedhandlers; iter != NULL; iter = iter->next) {
		fhp = (feedHandlerPtr)iter->data;
		if (!strcmp(str, fhp->typeStr))
			return fhp;
	}

	return NULL;
}

feedHandlerPtr feed_parse(feedPtr fp, gchar *data, size_t dataLength, gboolean autodiscover) {
	gchar			*source;
	xmlDocPtr 		doc;
	xmlNodePtr 		cur;
	GSList			*handlerIter;
	gboolean		handled = FALSE;
	feedHandlerPtr		handler = NULL;

	debug_enter("feed_parse");
	
	/* try to parse buffer with XML and to create a DOM tree */	
	do {
		if(NULL == (doc = parseBuffer(data, dataLength, &(fp->parseErrors)))) {
			gchar *msg = g_strdup_printf(_("<p>XML error while reading feed! Feed \"%s\" could not be loaded!</p>"), fp->source);
			addToHTMLBuffer(&(fp->parseErrors), msg);
			g_free(msg);
			break;
		}
		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Empty document!</p>"));
			break;
		}
		while(cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
		if(NULL == cur->name) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Invalid XML!</p>"));
			break;
		}
		
		/* determine the syndication format */
		handlerIter = feedhandlers;
		while (handlerIter != NULL) {
			handler = (feedHandlerPtr)(handlerIter->data);
			if (handler != NULL && handler->checkFormat != NULL && (*(handler->checkFormat))(doc, cur)) {
				(*(handler->feedParser))(fp, doc, cur);
				handled = TRUE;
				break;
			}
			handlerIter = handlerIter->next;
		}
	} while(0);
	
	if(!handled) {
		handler = NULL;
		
		/* test if we have a HTML page */
		if(autodiscover && (
		   (NULL != strstr(data, "<html>")) || (NULL != strstr(data, "<HTML>")) ||
		   (NULL != strstr(data, "<html ")) || (NULL != strstr(data, "<HTML "))
		  )) {
			/* if yes we should scan for links */
			debug1(DEBUG_UPDATE, "HTML detected, starting feed auto discovery (%s)", feed_get_source(fp));
			if(NULL != (source = html_auto_discover_feed(data, feed_get_source(fp)))) {
				/* now download the first feed link found */
				struct request *request = download_request_new(NULL);
				debug1(DEBUG_UPDATE, "feed link found: %s", source);
				request->source = g_strdup(source);
				download_process(request);
				if(NULL != request->data) {
					debug0(DEBUG_UPDATE, "feed link download successful!");
					feed_set_source(fp, source);
					handler = feed_parse(fp, request->data, request->size, FALSE);
				} else {
					/* if the download fails we do nothing except
					   unsetting the handler so the original source
					   will get a "unsupported type" error */
					handler = NULL;
					debug0(DEBUG_UPDATE, "feed link download failed!");
				}
				g_free(source);
				download_request_free(request);
			} else {
				debug0(DEBUG_UPDATE, "no feed link found!");
			}
		} else {		
			debug0(DEBUG_UPDATE, "There were errors while parsing a feed!");
			ui_mainwindow_set_status_bar(_("There were errors while parsing a feed"));
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not determine the feed type. Please check that it is <a href=\"http://feedvalidator.org\">valid</a> and in a <a href=\"http://liferea.sourceforge.net/index.php#supported_formats\">supported format</a>.</p>"));
		}
	} else {
		debug1(DEBUG_UPDATE, "discovered feed format: %s", feed_type_fhp_to_str(handler));
	}
	
	if(doc != NULL)
		xmlFreeDoc(doc);
		
	debug_exit("feed_parse");

	return handler;
}

/*
 * Feeds caches are marked to be saved at a few different places:
 * (1) Inside whe feed_set_* functions where an item is marked or made read or unread
 * (2) Inside of feed_process_result
 * (3) The callback where items are removed from the itemlist
 */
void feed_save(feedPtr fp) {
	xmlDocPtr 	doc;
	xmlNodePtr 	feedNode, itemNode;
	GSList		*itemlist;
	gchar		*filename;
	gchar		*tmp;
	itemPtr		ip;
	gint		saveCount = 0;
	gint		saveMaxCount;
			
	debug_enter("feed_save");
	
	if (fp->needsCacheSave == FALSE) {
		debug1(DEBUG_CACHE, "feed does not need to be saved: %s", fp->title);
		return;
	}

	debug1(DEBUG_CACHE, "saving feed: %s", fp->title);	
	g_assert(0 != fp->loaded);

	saveMaxCount = fp->cacheLimit;
	if (saveMaxCount == CACHE_DEFAULT)
		saveMaxCount = getNumericConfValue(DEFAULT_MAX_ITEMS);
	
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", fp->id, NULL);
	
	if(NULL != (doc = xmlNewDoc("1.0"))) {	
		if(NULL != (feedNode = xmlNewDocNode(doc, NULL, "feed", NULL))) {
			xmlDocSetRootElement(doc, feedNode);

			xmlNewTextChild(feedNode, NULL, "feedTitle", feed_get_title(fp));
			xmlNewTextChild(feedNode, NULL, "feedSource", feed_get_source(fp));
			
			if(NULL != fp->description)
				xmlNewTextChild(feedNode, NULL, "feedDescription", fp->description);

			if(NULL != feed_get_image_url(fp))
				xmlNewTextChild(feedNode, NULL, "feedImage", feed_get_image_url(fp));
	
			tmp = g_strdup_printf("%d", fp->defaultInterval);
			xmlNewTextChild(feedNode, NULL, "feedUpdateInterval", tmp);
			g_free(tmp);
			
			tmp = g_strdup_printf("%d", (TRUE == feed_get_available(fp))?1:0);
			xmlNewTextChild(feedNode, NULL, "feedStatus", tmp);
			g_free(tmp);
			
			tmp = g_strdup_printf("%d", (TRUE == fp->discontinued)?1:0);
			xmlNewTextChild(feedNode, NULL, "feedDiscontinued", tmp);
			g_free(tmp);
			
			if(NULL != fp->request && 0 != (fp->request->lastmodified.tv_sec)) {
				tmp = g_strdup_printf("%ld", fp->request->lastmodified.tv_sec);
				xmlNewTextChild(feedNode, NULL, "feedLastModified", tmp);
				g_free(tmp);
			}
			metadata_add_xml_nodes(fp->metadata, feedNode);

			itemlist = feed_get_item_list(fp);
			for(itemlist = feed_get_item_list(fp); itemlist != NULL; itemlist = g_slist_next(itemlist)) {
				ip = itemlist->data;
				g_assert(NULL != ip);
				
				if(saveMaxCount != CACHE_UNLIMITED &&
				   saveCount >= saveMaxCount &&
				   (fp->fhp == NULL || fp->fhp->directory == FALSE) &&
				   !item_get_mark(ip)) {
					continue;
				}
				if(NULL != (itemNode = xmlNewChild(feedNode, NULL, "item", NULL))) {
					
					/* should never happen... */
					if(NULL == item_get_title(ip))
						item_set_title(ip, "");
					xmlNewTextChild(itemNode, NULL, "title", item_get_title(ip));
					
					if(NULL != item_get_description(ip))
						xmlNewTextChild(itemNode, NULL, "description", item_get_description(ip));
					
					if(NULL != item_get_source(ip))
						xmlNewTextChild(itemNode, NULL, "source", item_get_source(ip));
						
					if(NULL != item_get_real_source_title(ip))
						xmlNewTextChild(itemNode, NULL, "real_source_title", item_get_real_source_title(ip));
						
					if(NULL != item_get_real_source_url(ip))
						xmlNewTextChild(itemNode, NULL, "real_source_url", item_get_real_source_url(ip));

					if(NULL != item_get_id(ip))
						xmlNewTextChild(itemNode, NULL, "id", item_get_id(ip));

					tmp = g_strdup_printf("%d", (TRUE == item_get_read_status(ip))?1:0);
					xmlNewTextChild(itemNode, NULL, "readStatus", tmp);
					g_free(tmp);
					
					tmp = g_strdup_printf("%d", (TRUE == item_get_mark(ip))?1:0);
					xmlNewTextChild(itemNode, NULL, "mark", tmp);
					g_free(tmp);

					tmp = g_strdup_printf("%ld", item_get_time(ip));
					xmlNewTextChild(itemNode, NULL, "time", tmp);
					g_free(tmp);
					
					metadata_add_xml_nodes(ip->metadata, itemNode);
					
				} else {
					g_warning("could not write XML item node!\n");
				}

				saveCount++;
			}
		} else {
			g_warning("could not create XML feed node for feed cache document!");
		}
		xmlSaveFormatFileEnc(filename, doc, NULL, 1);
		xmlFreeDoc(doc);
		g_free(filename);
	} else {
		g_warning("could not create XML document!");
	}
	
	fp->needsCacheSave = FALSE;
	debug_exit("feed_save");
}

/* Function which is called to load a feed's into memory. This function
   might be called multiple times even if the feed was already loaded.
   Each time the method is called a reference counter is incremented. */
gboolean feed_load(feedPtr fp) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	gchar		*filename, *tmp, *data = NULL;
	int		error = 0;
	gsize 		length;
	
	debug_enter("feed_load");
	debug1(DEBUG_CACHE, "feed_load for %s\n", feed_get_source(fp));
	g_assert(NULL != fp);	
	g_assert(NULL != fp->id);
	if(0 != (fp->loaded)++) {
		debug0(DEBUG_CACHE, "feed already loaded!\n");
		return TRUE;
	}

	if(FST_VFOLDER == feed_get_type(fp)) {
		debug0(DEBUG_CACHE, "it's a vfolder, nothing to do...");
		fp->loaded++;
		return TRUE;
	}
	
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", fp->id, NULL);
	debug1(DEBUG_CACHE, "loading cache file \"%s\"", filename);
		
	if((!g_file_get_contents(filename, &data, &length, NULL)) || (*data == 0)) {
		g_warning(_("Error while reading cache file \"%s\" ! Cache file could not be loaded!"), filename);
		ui_mainwindow_set_status_bar(_("Error while reading cache file \"%s\" ! Cache file could not be loaded!"), filename);
		fp->needsCacheSave = TRUE;
		g_free(filename);
		return FALSE;
	}
	
	do {
		g_assert(NULL != data);

		if(NULL == (doc = parseBuffer(data, length, &(fp->parseErrors)))) {
			g_free(fp->parseErrors);
			fp->parseErrors = NULL;
			addToHTMLBuffer(&(fp->parseErrors), g_strdup_printf(_("<p>XML error while parsing cache file! Feed cache file \"%s\" could not be loaded!</p>"), filename));
			error = 1;
			break;
		} 

		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			g_free(fp->parseErrors);
			fp->parseErrors = NULL;
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Empty document!</p>"));
			error = 1;
			break;
		}
		
		while(cur && xmlIsBlankNode(cur))
			cur = cur->next;

		if(xmlStrcmp(cur->name, BAD_CAST"feed")) {
			g_free(fp->parseErrors);
			fp->parseErrors = NULL;
			addToHTMLBuffer(&(fp->parseErrors), g_strdup_printf(_("<p>\"%s\" is no valid cache file! Cannot read cache file!</p>"), filename));
			error = 1;
			break;		
		}

		fp->unreadCount = 0;
		metadata_list_free(fp->metadata);
		fp->metadata = NULL;

		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			tmp = utf8_fix(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));

			if(tmp == NULL) {
				cur = cur->next;
				continue;
			}

			if(fp->description == NULL && !xmlStrcmp(cur->name, BAD_CAST"feedDescription")) {
				feed_set_description(fp, tmp);
				
			} else if(fp->title == NULL && !xmlStrcmp(cur->name, BAD_CAST"feedTitle")) {
				feed_set_title(fp, tmp);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedImage")) {
				feed_set_image_url(fp, tmp);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedStatus")) {
				feed_set_available(fp, (0 == atoi(tmp))?FALSE:TRUE);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedDiscontinued")) {
				fp->discontinued = (0 == atoi(tmp))?FALSE:TRUE;
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedLastModified")) {
				fp->lastModified.tv_sec = atol(tmp);
				fp->lastModified.tv_usec = 0L;
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"item")) {
				feed_add_item((feedPtr)fp, item_parse_cache(doc, cur));

			} else if (!xmlStrcmp(cur->name, BAD_CAST"attributes")) {
				fp->metadata = metadata_parse_xml_nodes(doc, cur);
			}
			g_free(tmp);	
			cur = cur->next;
		}
	
		favicon_load(fp);
	} while (FALSE);
	
	if(0 != error) {
		ui_mainwindow_set_status_bar(_("There were errors while parsing cache file \"%s\""), filename);
	}
	
	if (NULL != data)
		g_free(data);
	if(NULL != doc)
		xmlFreeDoc(doc);
	g_free(filename);
	
	debug_exit("feed_load");	
	return TRUE;
}

/* Only some feed informations are kept in memory to lower memory
   usage. This method unloads everything besides necessary infos. 
   
   If the feed parameter is NULL the function is called for all feeds.
   
   Each time this function is called the reference counter of all
   feeds is decremented and if it zero the unnecessary feed infos are 
   free'd */
void feed_unload(feedPtr fp) {
	gint 	unreadCount;

	if(NULL == fp) {
		if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
			debug0(DEBUG_CACHE, "unloading everything...");
			ui_feedlist_do_for_all((nodePtr)fp, ACTION_FILTER_FEED | ACTION_FILTER_DIRECTORY, feed_unload);
		}
	} else {
		g_assert(0 <= fp->loaded);	/* could indicate bad loaded reference counting */
		if(0 != fp->loaded) {
			feed_save(fp);		/* save feed before unloading */

			if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
				debug_enter("feed_unload");
				if(1 == fp->loaded) {
					if(IS_FEED(feed_get_type(fp))) {
						debug1(DEBUG_CACHE, "feed_unload (%s)", feed_get_source(fp));

						/* free items */
						unreadCount = fp->unreadCount;
						feed_clear_item_list(fp);						
						fp->unreadCount = unreadCount;
					} else {
						debug1(DEBUG_CACHE, "not unloading vfolder (%s)",  feed_get_title(fp));
					}
				} else {
					debug2(DEBUG_CACHE, "not unloading (%s) because it's used (%d references)...", feed_get_source(fp), fp->loaded);
				}
				fp->loaded--;
				debug_exit("feed_unload");
			}
		}
	}
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

	debug1(DEBUG_VERBOSE, "merging feed: \"%s\"", old_fp->title);
	feed_load(old_fp);
	if(TRUE == feed_get_available(new_fp)) {
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
			debug1(DEBUG_VERBOSE, "processing new item: \"%s\"", item_get_title(new_ip));
			
			found = FALSE;
			/* scan the old list to see if the new_fp item does already exist */
			old_list = old_fp->items;
			while(old_list) {
				old_ip = old_list->data;
				
				/* try to compare the two items */
				
				/* both items must have either ids or none */
				if(((item_get_id(old_ip) == NULL) && (item_get_id(new_ip) != NULL)) ||
				   ((item_get_id(old_ip) != NULL) && (item_get_id(new_ip) == NULL))) {	
					/* cannot be equal (different ids) so compare to 
					   next old item */
					old_list = g_slist_next(old_list);
			   		continue;
				} 
						
				/* compare titles and HTML descriptions */
				equal = TRUE;

				if(((item_get_title(old_ip) != NULL) && (item_get_title(new_ip) != NULL)) && 
				    (0 != strcmp(item_get_title(old_ip), item_get_title(new_ip))))		
			    		equal = FALSE;

				if(((item_get_description(old_ip) != NULL) && (item_get_description(new_ip) != NULL)) && 
				    (0 != strcmp(item_get_description(old_ip), item_get_description(new_ip))))
			    		equal = FALSE;

				if(NULL != item_get_id(old_ip)) {			
					/* if they have ids, compare them */
					if(0 == strcmp(item_get_id(old_ip), item_get_id(new_ip))){
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
				if(FALSE) { //== vfolder_check_new_item(new_ip)) {
					item_set_hidden(new_ip, TRUE);
					debug0(DEBUG_VERBOSE, "-> item found but hidden due to filter rule!");
				} else {
					old_fp->unreadCount++;
					feed_increase_new_counter(old_fp);
					debug0(DEBUG_VERBOSE, "-> item added to feed itemlist");
					traycount++;
				}
				newcount++;
				diff_list = g_slist_append(diff_list, (gpointer)new_ip);
			} else {
				/* if the item was found but has other contents -> update */
				if(!equal) {
					item_set_title(old_ip, item_get_title(new_ip));
					item_set_description(old_ip, item_get_description(new_ip));
					item_set_time(old_ip, item_get_time(new_ip));
					item_set_unread(old_ip);
					vfolder_update_item(old_ip);
					debug0(DEBUG_VERBOSE, "-> item already existing and was updated");
					newcount++;
					traycount++;
				} else {
					item_set_read_status(new_ip, TRUE);
					/* no item_set_new_status() - we don't treat changed items as new items! */
					debug0(DEBUG_VERBOSE, "-> item already exists");
				}

				/* any found new_fp items are not needed anymore */
				if(old_fp->type != FST_HELPFEED) { 
					new_ip->fp = new_fp;	/* else freeItem() would decrease the unread counter of old_fp */
					item_free(new_ip);					
				}
			}
			
			new_list = g_slist_next(new_list);
		}
		
		/* now we distinguish between incremental merging
		   for all normal feeds, and skipping old item
		   merging for help feeds... */
		if(old_fp->type != FST_HELPFEED) {
			debug0(DEBUG_VERBOSE, "postprocessing normal feed...");
			g_slist_free(new_fp->items);	/* dispose new item list */
			
			if(NULL == diff_list)
				ui_mainwindow_set_status_bar(_("\"%s\" has no new items"), old_fp->title);
			else 
				ui_mainwindow_set_status_bar(_("\"%s\" has %d new items"), old_fp->title, newcount);
			
			/* mark all items in new_fp as new (needed for notification features) */
			new_list = diff_list;
			while(new_list) {
				new_ip = new_list->data;
				item_set_new_status(new_ip, TRUE);
				new_list = g_slist_next(new_list);
			}
			
			old_list = g_slist_concat(diff_list, old_fp->items);
			old_fp->items = old_list;
		} else {
			debug0(DEBUG_VERBOSE, "postprocessing help feed...");
			/* free old list and items of old list */
			old_list = old_fp->items;
			while(NULL != old_list) {
				item_free((itemPtr)old_list->data);
				old_list = g_slist_next(old_list);
			}
			g_slist_free(old_fp->items);
			
			/* parent feed pointers are already correct, we can reuse simply the new list */
			old_fp->items = new_fp->items;
		}		

		/* copy description and default update interval */
		feed_set_description(old_fp, feed_get_description(new_fp));
		old_fp->defaultInterval = new_fp->defaultInterval;
	}

	g_free(old_fp->parseErrors);
	old_fp->parseErrors = new_fp->parseErrors;
	new_fp->parseErrors = NULL;

	feed_set_available(old_fp, feed_get_available(new_fp));
	new_fp->items = NULL;

	metadata_list_free(old_fp->metadata);
	old_fp->metadata = new_fp->metadata;
	new_fp->metadata = NULL;

	g_free(old_fp->htmlUrl);
	old_fp->htmlUrl = new_fp->htmlUrl;
	new_fp->htmlUrl = NULL;
	
	g_free(old_fp->imageUrl);
	old_fp->imageUrl = new_fp->imageUrl;
	new_fp->imageUrl = NULL;

	feed_free(new_fp);
	
	ui_tray_add_new(traycount);		/* finally update the tray icon */
	old_fp->needsCacheSave = TRUE;
	feed_unload(old_fp);
}

/**
 * method to be called to schedule a feed to be updated
 */
void feed_schedule_update(feedPtr fp, gint flags) {
	const gchar		*source;
	struct request		*request;
	g_assert(NULL != fp);

	debug1(DEBUG_CONF, "Scheduling %s to be updated", feed_get_title(fp));
	
	if(fp->request != NULL) {
		ui_mainwindow_set_status_bar(_("This feed \"%s\" is already being updated!"), feed_get_title(fp));
		return;
	}
	
	if(fp->discontinued) {
		ui_mainwindow_set_status_bar(_("The feed was discontinued. Liferea won't update it anymore!"));
		return;
	}
	
	ui_mainwindow_set_status_bar(_("Updating \"%s\""), feed_get_title(fp));
	
	if(NULL == (source = feed_get_source(fp))) {
		g_warning("Feed source is NULL! This should never happen - cannot update!");
		return;
	}
	
	feed_reset_update_counter(fp);

	request = download_request_new();
	fp->request = request;
	request->callback = feed_process_update_result;
	
	request->user_data = fp;
	request->source = g_strdup(source);
	request->lastmodified = fp->lastModified;
	request->flags = flags;
	request->priority = (flags & FEED_REQ_PRIORITY_HIGH)? 1 : 0;
	if (feed_get_filter(fp) != NULL)
		request->filtercmd = g_strdup(feed_get_filter(fp));
	/* prepare request url (strdup because it might be
	   changed on permanent HTTP redirection in netio.c) */
	ui_feed_update(fp); /* Change icon to arrows <- FIXME still needed? */
	
	download_queue(request);
	
}

/*------------------------------------------------------------------------------*/
/* timeout callback to check for update results					*/
/*------------------------------------------------------------------------------*/

void feed_process_update_result(struct request *request) {
	feedPtr			old_fp = (feedPtr)request->user_data;
	feedPtr			new_fp;
	feedHandlerPtr		fhp;
	
	ui_lock();
	g_assert(NULL != request);

	feed_set_available(old_fp, TRUE);

	if(401 == request->httpstatus) { /* unauthorized */
		feed_set_available(old_fp, FALSE);
		if (request->flags & FEED_REQ_AUTH_DIALOG)
			ui_feed_authdialog_new(GTK_WINDOW(mainwindow), old_fp, request->flags);
	} else if(410 == request->httpstatus) { /* gone */
		feed_set_available(old_fp, FALSE);
		old_fp->discontinued = TRUE;
		ui_mainwindow_set_status_bar(_("\"%s\" is discontinued. Liferea won't updated it anymore!"), feed_get_title(old_fp));
	} else if(304 == request->httpstatus) {
		ui_mainwindow_set_status_bar(_("\"%s\" has not changed since last update"), feed_get_title(old_fp));
	} else if(NULL != request->data) {
		do {
			old_fp->lastModified = request->lastmodified;
			/* note this is to update the feed URL on permanent redirects */
			if(0 != strcmp(request->source, feed_get_source(old_fp))) {
				feed_set_source(old_fp, request->source);
				ui_mainwindow_set_status_bar(_("The URL of \"%s\" has changed permanently and was updated"), feed_get_title(old_fp));
			}

			new_fp = feed_new();
			new_fp->loaded = 1;
			feed_set_source(new_fp, feed_get_source(old_fp)); /* Used by the parser functions to determine source */
			/* parse the new downloaded feed into new_fp */
			fhp = feed_parse(new_fp, request->data, request->size, request->flags & FEED_REQ_AUTO_DISCOVER);
			if(fhp == NULL) {
				feed_set_available(old_fp, FALSE);
				g_free(old_fp->parseErrors);
				old_fp->parseErrors = g_strdup(_("<p>Could not detect the type of this feed! Please check if the source really points to a resource provided in one of the supported syndication formats!</p>"));
				addToHTMLBuffer(&(old_fp->parseErrors), new_fp->parseErrors);
				feed_free(new_fp);
				break;
			} else {
				if(request->flags & FEED_REQ_AUTO_DISCOVER)
					feed_set_source(old_fp, feed_get_source(new_fp)); /* Reset autodiscovered source */
			}
			
			old_fp->fhp = fhp;
			
			if(new_fp != NULL && feed_get_title(new_fp) != NULL && request->flags & FEED_REQ_RESET_TITLE) {
				gchar *tmp = filter_title(g_strdup(feed_get_title(new_fp)));
				feed_set_title(old_fp, tmp);
				g_free(tmp);
			}

			if(new_fp != NULL && request->flags & FEED_REQ_RESET_UPDATE_INT)
				feed_set_update_interval(old_fp, feed_get_default_update_interval(new_fp));

			if(TRUE == fhp->merge)
				/* If the feed type supports merging... */
				feed_merge(old_fp, new_fp);
			else {
				/* Otherwise we simply use the new feed info... */
				feed_replace(old_fp, new_fp);
				ui_mainwindow_set_status_bar(_("\"%s\" updated..."), feed_get_title(old_fp));
			}

			if((feedPtr)ui_feedlist_get_selected() == old_fp) {
				ui_itemlist_load((nodePtr)old_fp);
			}
			if(request->flags & FEED_REQ_SHOW_PROPDIALOG)
				ui_feed_propdialog_new(GTK_WINDOW(mainwindow),old_fp);
		} while(0);
	} else {	
		ui_mainwindow_set_status_bar(_("\"%s\" is not available"), feed_get_title(old_fp));
		feed_set_available(old_fp, FALSE);
	}
	
	feed_set_error_description(old_fp, request->httpstatus, request->returncode);

	old_fp->request = NULL; /* Done before updating the UI so that the icon can be properly reset */
	ui_feed_update(old_fp);
	ui_notification_update(old_fp);	
	ui_feedlist_update();
	
	if(request->flags & FEED_REQ_DOWNLOAD_FAVICON)
		favicon_download(old_fp);
	
	ui_unlock();
}

void feed_add_item(feedPtr fp, itemPtr ip) {

	g_assert((0 != fp->loaded) || (FST_VFOLDER == feed_get_type(fp)));
	ip->fp = fp;
	if(FALSE == ip->readStatus)
		fp->unreadCount++;
	fp->items = g_slist_append(fp->items, (gpointer)ip);
}

itemPtr feed_lookup_item(feedPtr fp, gchar *id) {
	GSList		*items;
	itemPtr		ip;
	
	g_assert((0 != fp->loaded) || (FST_VFOLDER == feed_get_type(fp)));
	if(NULL == id) {
		g_warning("lookup for NULL id!");
		return (itemPtr)-1;
	}
	
	items = fp->items;
	while(NULL != items) {
		ip = (itemPtr)(items->data);
		if(NULL == ip->id) {
			g_warning("item (%s) with NULL id!", item_get_title(ip));
			return (itemPtr)-1;
		}
		if(0 == strcmp(ip->id, id))
			return ip;
		items = g_slist_next(items);
	}
	
	return NULL;
}

/* ---------------------------------------------------------------------------- */
/* feed attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

void feed_set_id(feedPtr fp, const gchar *id) {

	g_free(fp->id);
	fp->id = g_strdup(id);
}
const gchar *feed_get_id(feedPtr fp) { return fp->id; }

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

void feed_increase_new_counter(feedPtr fp) {
	fp->newCount++;
}
void feed_decrease_new_counter(feedPtr fp) {
	fp->newCount--;
}
gint feed_get_new_counter(feedPtr fp) { return fp->newCount; }

gint feed_get_default_update_interval(feedPtr fp) { return fp->defaultInterval; }
void feed_set_default_update_interval(feedPtr fp, gint interval) { fp->defaultInterval = interval; }

gint feed_get_update_interval(feedPtr fp) { return fp->updateInterval; }

void feed_set_update_interval(feedPtr fp, gint interval) {
	fp->updateInterval = interval; 

	conf_feedlist_schedule_save();
}

const feedHandlerPtr feed_get_fhp(feedPtr fp) {
	return fp->fhp;
}

void feed_reset_update_counter(feedPtr fp) {

	g_get_current_time(&fp->lastPoll);
	conf_feedlist_schedule_save();
	debug2(DEBUG_CONF, "Reseting last poll counter for %s to %ld.\n", fp->title, fp->lastPoll.tv_sec);
}

void feed_set_available(feedPtr fp, gboolean available) { fp->available = available; }
gboolean feed_get_available(feedPtr fp) { return fp->available; }

/* Returns a HTML string describing the last retrieval error 
   of this feed. Should only be called when feed_get_available
   returns FALSE. Caller must free returned string! */
gchar * feed_get_error_description(feedPtr fp) { 
	gchar	*tmp1 = NULL;

	if(fp->discontinued) {
		addToHTMLBufferFast(&tmp1, UPDATE_ERROR_START);
		addToHTMLBufferFast(&tmp1, HTTP410_ERROR_TEXT);
		addToHTMLBufferFast(&tmp1, UPDATE_ERROR_END);
	}
	addToHTMLBuffer(&tmp1, fp->errorDescription);
	return tmp1; 
}

/**
 * Creates a new error description according to the passed
 * HTTP status and the feeds parser errors. If the HTTP
 * status is a success status and no parser errors occured
 * no error messages is created. The created error message 
 * can be queried with feed_get_error_description().
 *
 * @param fp		feed
 * @param httpstatus	HTTP status
 * @param resultcode the update code's return code (see update.h)
 */
static void feed_set_error_description(feedPtr fp, gint httpstatus, gint resultcode) {
	gchar		*tmp1, *tmp2 = NULL, *buffer = NULL;
	gboolean	errorFound = FALSE;

	g_assert(NULL != fp);
	g_free(fp->errorDescription);
	fp->errorDescription = NULL;
	
	if(((httpstatus >= 200) && (httpstatus < 400)) && /* HTTP codes starting with 2 and 3 mean no error */
	   (NULL == fp->parseErrors))
		return;
	addToHTMLBuffer(&buffer, UPDATE_ERROR_START);
	
	if((200 != httpstatus) || (resultcode != NET_ERR_OK)) {
		/* first specific codes */
		switch(httpstatus) {
			case 401:tmp2 = g_strdup(_("You are unauthorized to download this feed. Please update your username and "
								  "password in the feed properties dialog box."));break;
			case 402:tmp2 = g_strdup(_("Payment Required"));break;
			case 403:tmp2 = g_strdup(_("Access Forbidden"));break;
			case 404:tmp2 = g_strdup(_("Resource Not Found"));break;
			case 405:tmp2 = g_strdup(_("Method Not Allowed"));break;
			case 406:tmp2 = g_strdup(_("Not Acceptable"));break;
			case 407:tmp2 = g_strdup(_("Proxy Authentication Required"));break;
			case 408:tmp2 = g_strdup(_("Request Time-Out"));break;
			case 410:tmp2 = g_strdup(_("Gone. Resource doesn't exist. Please unsubscribe!"));break;
		}
		/* Then, netio errors */
		if(tmp2 == NULL) {
			switch(resultcode) {
			case NET_ERR_URL_INVALID:    tmp2 = g_strdup(_("URL is invalid")); break;
			case NET_ERR_UNKNOWN:
			case NET_ERR_CONN_FAILED:
			case NET_ERR_SOCK_ERR:       tmp2 = g_strdup(_("Error connecting to remote host")); break;
			case NET_ERR_HOST_NOT_FOUND: tmp2 = g_strdup(_("Hostname could not be found")); break;
			case NET_ERR_CONN_REFUSED:   tmp2 = g_strdup(_("Network connection was refused by the remote host")); break;
			case NET_ERR_TIMEOUT:        tmp2 = g_strdup(_("Remote host did not finish sending data")); break;
				/* Transfer errors */
			case NET_ERR_REDIRECT_COUNT_ERR: tmp2 = g_strdup(_("Too many HTTP redirects were encountered")); break;
			case NET_ERR_REDIRECT_ERR:
			case NET_ERR_HTTP_PROTO_ERR: 
			case NET_ERR_GZIP_ERR:           tmp2 = g_strdup(_("Remote host sent an invalid response")); break;
				/* These are handled above	
				   case NET_ERR_HTTP_410:
				   case NET_ERR_HTTP_404:
				   case NET_ERR_HTTP_NON_200:
				*/
			case NET_ERR_AUTH_FAILED:
			case NET_ERR_AUTH_NO_AUTHINFO: tmp2 = g_strdup(_("Authentication failed")); break;
			case NET_ERR_AUTH_GEN_AUTH_ERR:
			case NET_ERR_AUTH_UNSUPPORTED: tmp2 = g_strdup(_("Webserver's authentication method incompatible with Liferea")); break;
			}
		}
		/* And generic messages in the unlikely event that the above didn't work */
		if(NULL == tmp2) {
			switch(httpstatus / 100) {
			case 3:tmp2 = g_strdup(_("Feed not available: Server requested unsupported redirection!"));break;
			case 4:tmp2 = g_strdup(_("Client Error"));break;
			case 5:tmp2 = g_strdup(_("Server Error"));break;
			default:tmp2 = g_strdup(_("(unknown networking error happened)"));break;
			}
		}
		errorFound = TRUE;
		tmp1 = g_strdup_printf(HTTP_ERROR_TEXT, httpstatus, tmp2);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
		g_free(tmp2);
	}
	
	/* add parsing error messages */
	if(NULL != fp->parseErrors) {
		if(errorFound)
			addToHTMLBuffer(&buffer, HTML_NEWLINE);			
		errorFound = TRUE;
		tmp1 = g_strdup_printf(PARSE_ERROR_TEXT, fp->parseErrors);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	/* if none of the above error descriptions matched... */
	if(!errorFound) {
		tmp1 = g_strdup_printf(_("There was a problem while reading this subscription. Please check the URL and console output."));
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	addToHTMLBuffer(&buffer, UPDATE_ERROR_END);
	fp->errorDescription = buffer;
}

const time_t feed_get_time(feedPtr fp) { return (fp != NULL ? fp->time : 0); }
void feed_set_time(feedPtr fp, const time_t t) { fp->time = t; }

const gchar * feed_get_title(feedPtr fp) { 

	if(NULL != fp->title)
		return fp->title; 
	else if (NULL != fp->source)
		return fp->source;
	else
		return NULL;
}

void feed_set_title(feedPtr fp, const gchar *title) {
	g_free(fp->title);
	if (title != NULL)
		fp->title = g_strdup(title);
	else
		fp->title = NULL;
	conf_feedlist_schedule_save();
}

const gchar * feed_get_description(feedPtr fp) { return fp->description; }
void feed_set_description(feedPtr fp, const gchar *description) {
	g_free(fp->description);
	if (description != NULL)
		fp->description = g_strdup(description);
	else
		fp->description = NULL;
}

const gchar * feed_get_source(feedPtr fp) { return fp->source; }
const gchar * feed_get_filter(feedPtr fp) { return fp->filtercmd; }

void feed_set_source(feedPtr fp, const gchar *source) {
	g_free(fp->source);

	fp->source = g_strdup(source);
	conf_feedlist_schedule_save();
}

void feed_set_filter(feedPtr fp, const gchar *filter) {
	g_free(fp->filtercmd);

	fp->filtercmd = g_strdup(filter);
	conf_feedlist_schedule_save();
}

const gchar * feed_get_html_url(feedPtr fp) { return fp->htmlUrl; };
void feed_set_html_url(feedPtr fp, const gchar *htmlUrl) {

	g_free(fp->htmlUrl);
	if(htmlUrl != NULL)
		fp->htmlUrl = g_strdup(htmlUrl);
	else
		fp->htmlUrl = NULL;
}

const gchar * feed_get_image_url(feedPtr fp) { return fp->imageUrl; };
void feed_set_image_url(feedPtr fp, const gchar *imageUrl) {

	g_free(fp->imageUrl);
	if(imageUrl != NULL)
		fp->imageUrl = g_strdup(imageUrl);
	else
		fp->imageUrl = NULL;
}

/* returns feed's list of items, if necessary loads the feed from cache */
GSList * feed_get_item_list(feedPtr fp) { 

	g_assert(0 != fp->loaded);
	return fp->items; 
}

/* method to free all items of a feed */
void feed_clear_item_list(feedPtr fp) {
	GSList	*item;

	item = fp->items;
	while(NULL != item) {
		item_free(item->data);
		item = g_slist_next(item);
	}
	g_slist_free(fp->items);
	fp->items = NULL;
}

void feed_mark_all_items_read(feedPtr fp) {
	GSList	*item;

	feed_load(fp);
	item = fp->items;
	while(NULL != item) {
		item_set_read((itemPtr)item->data);
		item = g_slist_next(item);
	}
	ui_feedlist_update();
	feed_unload(fp);
}

gchar *feed_render(feedPtr fp) {
	struct displayset	displayset;
	gchar			*buffer = NULL;
	gchar			*tmp, *tmp2;
	gboolean		migration = FALSE;

	g_assert(0 != fp->loaded);	
	displayset.headtable = NULL;
	displayset.head = NULL;
	displayset.body = g_strdup(feed_get_description(fp));
	displayset.foot = NULL;
	displayset.foottable = NULL;	

	/* FIXME: remove with 0.9.x */
	if((NULL != displayset.body) &&
	   (NULL != strstr(displayset.body, "class=\"itemhead\"")))	/* I hope this is unique enough...*/
		migration = TRUE;

	if(FALSE == migration) {	
		metadata_list_render(fp->metadata, &displayset);

		/* Error description */
		if(NULL != (tmp = feed_get_error_description(fp))) {
			addToHTMLBufferFast(&buffer, tmp);
			g_free(tmp);
		}

		/* Head table */
		addToHTMLBufferFast(&buffer, HEAD_START);
		/*  -- Feed line */
		if(feed_get_html_url(fp) != NULL)
			tmp = g_strdup_printf("<a href=\"%s\">%s</a>",
							  feed_get_html_url(fp),
							  feed_get_title(fp));
		else
			tmp = g_strdup(feed_get_title(fp));

		tmp2 = g_strdup_printf(HEAD_LINE, _("Feed:"), tmp);
		g_free(tmp);
		addToHTMLBufferFast(&buffer, tmp2);
		g_free(tmp2);

		/*  -- Source line */
		if(feed_get_source(fp) != NULL) {
			tmp = g_strdup_printf("<a href=\"%s\">%s</a>",
							  feed_get_source(fp),
							  feed_get_source(fp));

			tmp2 = g_strdup_printf(HEAD_LINE, _("Source:"), tmp);
			g_free(tmp);
			addToHTMLBufferFast(&buffer, tmp2);
			g_free(tmp2);
		}

		addToHTMLBufferFast(&buffer, displayset.headtable);
		g_free(displayset.headtable);
		addToHTMLBufferFast(&buffer, HEAD_END);

		/* Head */
		if(displayset.head != NULL) {
			addToHTMLBufferFast(&buffer, displayset.head);
			g_free(displayset.head);
		}

		/* feed/channel image */
		if(NULL != feed_get_image_url(fp)) {
			addToHTMLBufferFast(&buffer, "<img class=\"feed\" src=\"");
			addToHTMLBufferFast(&buffer, feed_get_image_url(fp));
			addToHTMLBufferFast(&buffer, "\"><br>");
		}
	}

	/* Body */
	if(displayset.body != NULL) {
		addToHTMLBufferFast(&buffer, displayset.body);
		g_free(displayset.body);
	}

	if(FALSE == migration) {
		/* Foot */
		if(displayset.foot != NULL) {
			addToHTMLBufferFast(&buffer, displayset.foot);
			g_free(displayset.foot);
		}

		if(displayset.foottable != NULL) {
			addToHTMLBufferFast(&buffer, FEED_FOOT_TABLE_START);
			addToHTMLBufferFast(&buffer, displayset.foottable);
			addToHTMLBufferFast(&buffer, FEED_FOOT_TABLE_START);
			g_free(displayset.foottable);
		}
	}	
	return buffer;
}

/* Method to copy the info payload of the structure given by
   new_fp to the structure fp points to. Essential model
   specific keys of fp are kept. The feed structure of new_fp 
   is freed afterwards. 
   
   This method is primarily used for feeds which do not want
   to incrementally update items like directories. */
static void feed_replace(feedPtr fp, feedPtr new_fp) {
	feedPtr		tmp_fp;
	itemPtr		ip;
	GSList		*item;

	feed_load(fp);
	
	/* To prevent updating feed ptr in the tree store and
	   feeds hashtable we reuse the old structure! */
	
	/* in the next step we will copy the new_fp structure
	   to fp, but we need to keep some fp attributes */
	g_free(new_fp->title);
	new_fp->title = fp->title;

	g_free(new_fp->source);
	new_fp->source = fp->source;

	if(new_fp->icon != NULL)
		g_object_unref(new_fp->icon);
	new_fp->icon = fp->icon;
	new_fp->id = fp->id;
	new_fp->filtercmd = fp->filtercmd;
	new_fp->type = fp->type;
	new_fp->request = fp->request;
	new_fp->faviconRequest = fp->faviconRequest;
	new_fp->ui_data = fp->ui_data;
	new_fp->fhp = fp->fhp;	
	tmp_fp = feed_new();
	memcpy(tmp_fp, fp, sizeof(struct feed));	/* make a copy of the old fp pointers... */
	memcpy(fp, new_fp, sizeof(struct feed));
	tmp_fp->id = NULL;				/* to prevent removal of reused attributes... */
	tmp_fp->items = NULL;
	tmp_fp->title = NULL;
	tmp_fp->source = NULL;
	tmp_fp->request = NULL;
	tmp_fp->ui_data = NULL;
	tmp_fp->filtercmd = NULL;
	tmp_fp->faviconRequest = NULL;
	tmp_fp->icon = NULL;
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

	fp->needsCacheSave = TRUE;
	feed_unload(fp);
}

/* method to totally erase a feed, remove it from the config, etc.... */
void feed_free(feedPtr fp) {
	gchar *filename = NULL;
	
	if(FST_VFOLDER == fp->type) {
		vfolder_free(fp);	/* some special preparations for vfolders */
	} else {
		g_assert(IS_FEED(fp->type));
	}
	
	if(displayed_node == (nodePtr)fp) { /* This is not strictly necessary. It just speeds deletion of an entire itemlist. */
		ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
		ui_itemlist_clear();
	}
	
	/* removes an existing notification for this feed */
	ui_notification_remove_feed(fp);
	
	/* free UI info */
	if(fp->ui_data)
		ui_folder_remove_node((nodePtr)fp);

	/* free items */
	feed_clear_item_list(fp);

	if(fp->id && fp->id[0] != '\0')
		filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", fp->id, NULL);

	/* FIXME: Move this to a better place. The cache file does not
	   need to always be deleted, for example when freeing a
	   feedstruct used for updating. */
	if(filename && 0 != unlink(filename))
		/* Oh well.... Can't do anything about it. 99% of the time,
		   this is spam anyway. */;
		g_free(filename);

	/* Don't free active feed requests here, because they might still
	   be processed in the update queues! Abandoned requests are
	   free'd in feed_process. They must be freed in the main thread
	   for locking reasons. */
	if(fp->request != NULL)
		fp->request->callback = NULL;
	if(fp->faviconRequest != NULL)
		fp->faviconRequest->callback = NULL;

	if(fp->icon != NULL)
		g_object_unref(fp->icon);
	
	if(fp->id) {
		favicon_remove(fp);
		conf_feedlist_schedule_save();
		g_free(fp->id);
	}
	
	g_free(fp->title);
	g_free(fp->description);
	g_free(fp->errorDescription);
	g_free(fp->source);
	g_free(fp->filtercmd);
	g_free(fp->htmlUrl);
	g_free(fp->imageUrl);
	g_free(fp->parseErrors);
	metadata_list_free(fp->metadata);
	g_free(fp);
}
