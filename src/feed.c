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
#include <stdlib.h>

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
#include "update.h"
#include "debug.h"
#include "metadata.h"

#include "ui_feed.h"	/* FIXME: Remove ui_* include from core code */
#include "ui_feedlist.h"
#include "ui_tray.h"
#include "htmlview.h"

/* auto detection lookup table */
static GSList *feedhandlers;

struct feed_type {
	gint id_num;
	gchar *id_str;
};

/* initializing function, only called upon startup */
void feed_init(void) {

	feedhandlers = g_slist_append(feedhandlers, ocs_init_feed_handler()); /* Must come before RSS/RDF */
	feedhandlers = g_slist_append(feedhandlers, rss_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, cdf_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, pie_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, opml_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, vfolder_init_feed_handler());
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
	fp->tmpdata = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	
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
	
	if(str == NULL)
		return NULL;

	for(iter = feedhandlers; iter != NULL; iter = iter->next) {
		fhp = (feedHandlerPtr)iter->data;
		if(!strcmp(str, fhp->typeStr))
			return fhp;
	}

	return NULL;
}

/**
 * General feed source parsing function. Parses the passed feed source
 * and tries to determine the source type. If the type is HTML and 
 * autodiscover is TRUE the function tries to find a feed, tries to
 * download it and parse the feed's source instead of the passed source.
 *
 * @param fp		the feed structure to be filled
 * @param data		the feed source
 * @param dataLength the length of the 'data' string
 * @param autodiscover	TRUE if auto discovery should be possible
 */
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
			
				/* free old temp. parsing data, don't free right after parsing because
				   it can be used until the last feed request is finished, move me 
				   to the place where the last request in list otherRequests is 
				   finished :-) */
				g_hash_table_destroy(fp->tmpdata);
				fp->tmpdata = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
				
				/* we always drop old metadata */
				metadata_list_free(fp->metadata);
				fp->metadata = NULL;
				
				if(FALSE == handler->merge)
					feed_clear_item_list(fp);	/* for directories */
				
				(*(handler->feedParser))(fp, doc, cur);		/* parse it */
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
			feed_set_available(fp, FALSE);
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
	xmlNodePtr 	feedNode;
	GSList		*itemlist;
	gchar		*filename;
	gchar		*tmp;
	itemPtr		ip;
	gint		saveCount = 0;
	gint		saveMaxCount;
			
	debug_enter("feed_save");
	
	if(fp->needsCacheSave == FALSE) {
		debug1(DEBUG_CACHE, "feed does not need to be saved: %s", fp->title);
		return;
	}

	debug1(DEBUG_CACHE, "saving feed: %s", fp->title);	
	g_assert(0 != fp->loaded);

	saveMaxCount = fp->cacheLimit;
	if(saveMaxCount == CACHE_DEFAULT)
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
			
			tmp = g_strdup_printf("%d", (TRUE == feed_get_discontinued(fp))?1:0);
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
				item_save(ip, feedNode);
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
	GList		*items = NULL;
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
				feed_set_discontinued(fp, (0 == atoi(tmp))?FALSE:TRUE);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedLastModified")) {
				fp->lastModified.tv_sec = atol(tmp);
				fp->lastModified.tv_usec = 0L;
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"item")) {
				items = g_list_append(items, item_parse_cache(doc, cur));

			} else if (!xmlStrcmp(cur->name, BAD_CAST"attributes")) {
				fp->metadata = metadata_parse_xml_nodes(doc, cur);
			}
			g_free(tmp);	
			cur = cur->next;
		}
		feed_add_items(fp, items);
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

	g_assert(NULL != fp);
	g_assert(0 <= fp->loaded);	/* could indicate bad loaded reference counting */
	if(0 != fp->loaded) {
		feed_save(fp);		/* save feed before unloading */

		if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
			debug_enter("feed_unload");
			if(1 == fp->loaded) {
				if(FST_FEED == feed_get_type(fp)) {
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
	
	if(feed_get_discontinued(fp)) {
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
	request->callback = ui_feed_process_update_result;
	
	request->user_data = fp;
	/* prepare request url (strdup because it might be
	   changed on permanent HTTP redirection in netio.c) */
	request->source = g_strdup(source);
	request->lastmodified = fp->lastModified;
	request->flags = flags;
	request->priority = (flags & FEED_REQ_PRIORITY_HIGH)? 1 : 0;
	if(feed_get_filter(fp) != NULL)
		request->filtercmd = g_strdup(feed_get_filter(fp));
	
	download_queue(request);
	
}

/**
 * To be used by parser implementation to merge a new orderd list of
 * items to a feed. Ensures properly ordered joint item list. The
 * passed GList is free'd afterwards!
 */
void feed_add_items(feedPtr fp, GList *items) {
	GList	*iter;
	
	/* The parser implementations read the items from the
	   feed from top to bottom. Adding them directly would
	   mean to reverse there order. */
	iter = g_list_last(items);
	while(iter != NULL) {
		feed_add_item(fp, ((itemPtr)iter->data));
		iter = g_list_previous(iter);
	}
	g_list_free(items);
}

/**
 * Can be used to add a single item to a feed. But it's better to
 * use feed_add_items() to keep the item order of parsed feeds.
 * Should be used for vfolders only.
 *
 * Adds an item to the feed. Realizes the item merging logic based on
 * the item id's. 
 *
 * Note: Does not do merging for vfolders. Would be hazardous!
 */
void feed_add_item(feedPtr fp, itemPtr new_ip) {
	GSList		*old_items;
	itemPtr		old_ip;
	gboolean	found, equal = FALSE;
	
	g_assert((0 != fp->loaded) || (FST_VFOLDER == feed_get_type(fp)));
	
	if(FST_VFOLDER != feed_get_type(fp)) {
		/* determine if we should add it... */
		debug1(DEBUG_VERBOSE, "check new item for merging: \"%s\"", item_get_title(new_ip));
		
		/* compare to every existing item in this feed */
		found = FALSE;
		old_items = feed_get_item_list(fp);
		while(NULL != old_items) {
			old_ip = old_items->data;
			
			/* try to compare the two items */

			/* trivial case: one item has item the other doesn't -> they can't be equal */
			if(((item_get_id(old_ip) == NULL) && (item_get_id(new_ip) != NULL)) ||
			   ((item_get_id(old_ip) != NULL) && (item_get_id(new_ip) == NULL))) {	
				/* cannot be equal (different ids) so compare to 
				   next old item */
				old_items = g_slist_next(old_items);
			   	continue;
			} 

			/* just for the case there are no ids: compare titles and HTML descriptions */
			equal = TRUE;

			if(((item_get_title(old_ip) != NULL) && (item_get_title(new_ip) != NULL)) && 
			    (0 != strcmp(item_get_title(old_ip), item_get_title(new_ip))))		
		    		equal = FALSE;

			if(((item_get_description(old_ip) != NULL) && (item_get_description(new_ip) != NULL)) && 
			    (0 != strcmp(item_get_description(old_ip), item_get_description(new_ip))))
		    		equal = FALSE;

			/* best case: they both have ids (position important: id check is useless without knowing if the items are different!) */
			if(NULL != item_get_id(old_ip)) {			
				if(0 == strcmp(item_get_id(old_ip), item_get_id(new_ip))){
					found = TRUE;
					break;
				}
			}
			
			if(equal) {
				found = TRUE;
				break;
			}

			old_items = g_slist_next(old_items);
		}
		
		if(!found) {
			if(FALSE == item_get_read_status(new_ip))
				feed_increase_unread_counter(fp);
			if(TRUE == new_ip->newStatus) {
				fp->newCount++;
				ui_tray_add_new(1);
			}
			fp->items = g_slist_prepend(fp->items, (gpointer)new_ip);
			new_ip->fp = fp;
			
			/* FIXME: remove this migration code with 0.9.x */
			if(0 == new_ip->nr) {
				new_ip->nr = ++(fp->lastItemNr);
				new_ip->fp->needsCacheSave = TRUE;
			}
		
			/* Check if feed filters allow display of this item, we don't
			   delete the item because there can be vfolders which display
			   it. To allow this the parent feed does store the item, but
			   hides it. */
			if(FALSE) { //== vfolder_check_new_item(new_ip)) {
				item_set_hidden(new_ip, TRUE);
				debug0(DEBUG_VERBOSE, "-> item found but hidden due to filter rule!");
			} else {
				debug0(DEBUG_VERBOSE, "-> item added to feed itemlist");
			}
		} else {
			/* if the item was found but has other contents -> update contents */
			if(!equal) {
				/* no item_set_new_status() - we don't treat changed items as new items! */
				item_set_title(old_ip, item_get_title(new_ip));
				item_set_description(old_ip, item_get_description(new_ip));
				item_set_time(old_ip, item_get_time(new_ip));
				item_set_unread(old_ip);	// FIXME: needed?
				metadata_list_free(old_ip->metadata);
				old_ip->metadata = new_ip->metadata;
				vfolder_update_item(old_ip);
				debug0(DEBUG_VERBOSE, "-> item already existing and was updated");
				ui_tray_add_new(1);
			} else {
				debug0(DEBUG_VERBOSE, "-> item already exists");
			}
		}
	} else {
		/* just add the item */
		if(FALSE == item_get_read_status(new_ip))
			feed_increase_unread_counter(fp);
		fp->items = g_slist_append(fp->items, (gpointer)new_ip);
		new_ip->fp = fp;
		/* item number should already be set by item_copy() */
	}
}

void feed_remove_item(feedPtr fp, itemPtr ip) {

	fp->items = g_slist_remove(fp->items, ip);	
	vfolder_remove_item(ip);			/* remove item copies */
	item_free(ip);					/* remove the item */
	fp->needsCacheSave = TRUE;
}

itemPtr feed_lookup_item(feedPtr fp, gulong nr) {
	GSList		*items;
	itemPtr		ip;
	
	g_assert((0 != fp->loaded) || (FST_VFOLDER == feed_get_type(fp)));
	items = fp->items;
	while(NULL != items) {
		ip = (itemPtr)(items->data);
		if(ip->nr == nr)
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

void feed_set_discontinued(feedPtr fp, gboolean discontinued) { fp->discontinued = discontinued; }

gboolean feed_get_discontinued(feedPtr fp) { return fp->discontinued; }

/* Returns a HTML string describing the last retrieval error 
   of this feed. Should only be called when feed_get_available
   returns FALSE. Caller must free returned string! */
gchar * feed_get_error_description(feedPtr fp) { 
	gchar	*tmp1 = NULL;

	if(feed_get_discontinued(fp)) {
		addToHTMLBufferFast(&tmp1, UPDATE_ERROR_START);
		addToHTMLBufferFast(&tmp1, HTTP410_ERROR_TEXT);
		addToHTMLBufferFast(&tmp1, UPDATE_ERROR_END);
	}
	addToHTMLBuffer(&tmp1, fp->errorDescription);
	return tmp1; 
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

	g_assert((0 != fp->loaded) || (FST_VFOLDER == fp->type));
	return fp->items; 
}

void feed_set_sort_column(feedPtr fp, gint sortColumn, gboolean reversed) {
	fp->sortColumn = sortColumn;
	fp->sortReversed = reversed;
	conf_feedlist_schedule_save();
}

/**
 * Method to free all items structures of a feed, does not mean
 * that it removes items from cache! This method is used 
 * for feed unloading.
 */
void feed_clear_item_list(feedPtr fp) {
	GSList	*item;

	item = fp->items;

	while(NULL != item) {
		item_free(item->data);
		item = g_slist_next(item);
	}
	g_slist_free(fp->items);
	fp->items = NULL;
	/* explicitly not forcing feed saving to allow feed unloading */
}

/** 
 * Method to permanently removing all items from the given feed
 */
void feed_remove_items(feedPtr fp) {
	GSList	*item;

	item = fp->items;

	while(NULL != item) {
		vfolder_remove_item(item->data);	/* remove item copies */
		item_free(item->data);			/* remove the item */
		item = g_slist_next(item);
	}
	g_slist_free(fp->items);
	fp->items = NULL;
	fp->needsCacheSave = TRUE;	/* force feed saving to make it permanent */
}

void feed_mark_all_items_read(feedPtr fp) {
	GSList	*item;

	feed_load(fp);
	item = fp->items;
	while(NULL != item) {
		item_set_read((itemPtr)item->data);
		item = g_slist_next(item);
	}
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

		tmp2 = g_strdup_printf(HEAD_LINE, (FST_FEED == feed_get_type(fp))?_("Feed:"):_("VFolder"), tmp);
		g_free(tmp);
		addToHTMLBufferFast(&buffer, tmp2);
		g_free(tmp2);

		/*  -- Source line */
		if((NULL != feed_get_source(fp)) && (FST_VFOLDER != feed_get_type(fp))) {
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

/* method to totally erase a feed, remove it from the config, etc.... */
void feed_free(feedPtr fp) {
	gchar	*filename = NULL;
	GSList	*iter;
	
	if(FST_VFOLDER == fp->type) {
		vfolder_free(fp);	/* some special preparations for vfolders */
	} else {
		g_assert(FST_FEED == fp->type);
	}
	
	if(displayed_node == (nodePtr)fp) { /* This is not strictly necessary. It just speeds deletion of an entire itemlist. */
		ui_htmlview_clear(ui_mainwindow_get_active_htmlview());
		ui_itemlist_clear();
	}

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
	
	/* same goes for other requests */
	iter = fp->otherRequests;
	while(NULL != iter) {
		((struct request *)iter->data)->callback = NULL;
		iter = g_slist_next(iter);
	}
	g_slist_free(fp->otherRequests);

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
	g_hash_table_destroy(fp->tmpdata);
	g_free(fp);
}
