/**
 * @file feed.c common feed handling
 * 
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/uri.h>
#include <string.h>
#include <time.h>
#include <unistd.h> /* For unlink() */
#include <stdlib.h>

#include "conf.h"
#include "common.h"

#include "support.h"
#include "html.h"
#include "parsers/cdf_channel.h"
#include "parsers/rss_channel.h"
#include "parsers/atom10.h"
#include "parsers/pie_feed.h"
#include "parsers/ocs_dir.h"
#include "parsers/opml.h"
#include "vfolder.h"
#include "feed.h"
#include "favicon.h"
#include "callbacks.h"
#include "net/cookies.h"
#include "update.h"
#include "debug.h"
#include "metadata.h"

#include "ui/ui_enclosure.h"
#include "ui/ui_feed.h"	
#include "ui/ui_feedlist.h"
#include "ui/ui_tray.h"
#include "ui/ui_htmlview.h"

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
	feedhandlers = g_slist_append(feedhandlers, atom10_init_feed_handler());  /* Must be before pie */
	feedhandlers = g_slist_append(feedhandlers, pie_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, opml_init_feed_handler());
}

/* function to create a new feed structure */
feedPtr feed_new(void) {
	feedPtr		fp;
	
	fp = g_new0(struct feed, 1);

	/* we don't allocate a request structure this is done
	   during cache loading or first update! */
	
	fp->updateInterval = -1;
	fp->defaultInterval = -1;
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
 * @param sp		the item set to be filled
 * @param data		the feed source
 * @param dataLength the length of the 'data' string
 * @param autodiscover	TRUE if auto discovery should be possible
 */
feedHandlerPtr feed_parse(feedPtr fp, itemSetPtr sp, gchar *data, size_t dataLength, gboolean autodiscover) {
	gchar			*source;
	xmlDocPtr 		doc;
	xmlNodePtr 		cur;
	GSList			*handlerIter;
	gboolean		handled = FALSE;
	feedHandlerPtr		handler = NULL;

	debug_enter("feed_parse");

	g_assert(NULL == sp->items);
	
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
		while(cur != NULL && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
		if(NULL == cur->name) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Invalid XML!</p>"));
			break;
		}
		
		/* determine the syndication format */
		handlerIter = feedhandlers;
		while(handlerIter != NULL) {
			handler = (feedHandlerPtr)(handlerIter->data);
			if(handler != NULL && handler->checkFormat != NULL && (*(handler->checkFormat))(doc, cur)) {
			
				/* free old temp. parsing data, don't free right after parsing because
				   it can be used until the last feed request is finished, move me 
				   to the place where the last request in list otherRequests is 
				   finished :-) */
				g_hash_table_destroy(fp->tmpdata);
				fp->tmpdata = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
				
				/* we always drop old metadata */
				metadata_list_free(fp->metadata);
				fp->metadata = NULL;
				
				(*(handler->feedParser))(fp, sp, doc, cur);		/* parse it */
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
					handler = feed_parse(fp, sp, request->data, request->size, FALSE);
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
				feed_set_available(fp, FALSE);
				addToHTMLBuffer(&(fp->parseErrors), _("<p>The URL you want Liferea to subscribe to points to a webpage and the auto discovery found no feeds on this page. Maybe this webpage just does not support feed auto discovery.</p>"));
			}
		} else {
			debug0(DEBUG_UPDATE, "neither a known feed type nor a HTML document!");
			feed_set_available(fp, FALSE);
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not determine the feed type. Please check that it is a <a href=\"http://feedvalidator.org\">valid</a> type and listed in the <a href=\"http://liferea.sourceforge.net/supported_formats.htm\">supported formats</a>.</p>"));
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
 *
 * This method really saves the feed to disk.
 */
void feed_save_to_cache(feedPtr fp, itemSetPtr sp, const gchar *id) {
	xmlDocPtr 	doc;
	xmlNodePtr 	feedNode;
	GList		*itemlist, *iter;
	gchar		*filename, *tmpfilename;
	gchar		*tmp;
	itemPtr		ip;
	gint		saveCount = 0;
	gint		saveMaxCount;
			
	debug_enter("feed_save_to_cache");
	
	debug1(DEBUG_CACHE, "saving feed: %s", fp->title);	

	saveMaxCount = fp->cacheLimit;
	if(saveMaxCount == CACHE_DEFAULT)
		saveMaxCount = getNumericConfValue(DEFAULT_MAX_ITEMS);
	
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", id, NULL);
	tmpfilename = g_strdup_printf("%s~", filename);
	
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
			
			if(feed_get_lastmodified(fp) != NULL) {
				xmlNewTextChild(feedNode, NULL, "feedLastModified", feed_get_lastmodified(fp));
			}
			
			metadata_add_xml_nodes(fp->metadata, feedNode);

			itemlist = g_list_copy(sp->items);
			for(iter = itemlist; iter != NULL; iter = g_list_next(iter)) {
				ip = iter->data;
				g_assert(NULL != ip);
				
				if(saveMaxCount == CACHE_DISABLE)
					continue;

				if((saveMaxCount != CACHE_UNLIMITED) &&
				   (saveCount >= saveMaxCount) &&
				   (fp->fhp == NULL || fp->fhp->directory == FALSE) &&
				   ! ip->flagStatus) {
				   	itemlist_remove_item(ip);
				} else {
					item_save(ip, feedNode);
					saveCount++;
				}
			}
			g_list_free(itemlist);
		} else {
			g_warning("could not create XML feed node for feed cache document!");
		}
		if (xmlSaveFormatFile(tmpfilename, doc,1) == -1) {
			g_warning("Error attempting to save feed cache file \"%s\"!", tmpfilename);
		} else {
			if (rename(tmpfilename, filename) == -1)
				perror("Error overwriting old cache file"); /* Nothing else can be done... probably the disk is going bad */
		}
		xmlFreeDoc(doc);
	} else {
		g_warning("could not create XML document!");
	}
	
	g_free(tmpfilename);
	g_free(filename);

	debug_exit("feed_save_to_cache");
}

itemSetPtr feed_load_from_cache(feedPtr fp, const gchar *id) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	itemSetPtr	sp = NULL;
	gchar		*filename, *tmp, *data = NULL;
	int		error = 0;
	gsize 		length;

	debug_enter("feed_load_from_cache");

	debug1(DEBUG_CACHE, "feed_load for %s\n", feed_get_source(fp));
	
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", id, NULL);
	debug1(DEBUG_CACHE, "loading cache file \"%s\"", filename);
		
	if((!g_file_get_contents(filename, &data, &length, NULL)) || (*data == 0)) {
		ui_mainwindow_set_status_bar(_("Error while reading cache file \"%s\" ! Cache file could not be loaded!"), filename);
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

		metadata_list_free(fp->metadata);
		fp->metadata = NULL;
		sp = g_new0(struct itemSet, 1);

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
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedUpdateInterval")) {
				feed_set_default_update_interval(fp, atoi(tmp));
								
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedImage")) {
				feed_set_image_url(fp, tmp);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedStatus")) {
				feed_set_available(fp, (0 == atoi(tmp))?FALSE:TRUE);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedDiscontinued")) {
				feed_set_discontinued(fp, (0 == atoi(tmp))?FALSE:TRUE);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"feedLastModified")) {
				feed_set_lastmodified(fp, tmp);
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"item")) {
				itemset_add_item(sp, item_parse_cache(doc, cur));

			} else if (!xmlStrcmp(cur->name, BAD_CAST"attributes")) {
				fp->metadata = metadata_parse_xml_nodes(doc, cur);
			}
			g_free(tmp);	
			cur = cur->next;
		}
		//favicon_load(np); FIXME!!!
	} while(FALSE);
	
	if(0 != error)
		ui_mainwindow_set_status_bar(_("There were errors while parsing cache file \"%s\""), filename);
	
	if(NULL != doc)
		xmlFreeDoc(doc);
	g_free(data);
	g_free(filename);
	
	debug_exit("feed_load_from_cache");
	return sp;
}

/**
 * method to be called to schedule a feed to be updated
 */
void feed_schedule_update(feedPtr fp, guint flags) {
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
	request->cookies = fp->cookies;
	if (feed_get_lastmodified(fp) != NULL)
		request->lastmodified = g_strdup(feed_get_lastmodified(fp));
	if (feed_get_etag(fp) != NULL)
		request->etag = g_strdup(feed_get_etag(fp));
	request->flags = flags;
	request->priority = (flags & FEED_REQ_PRIORITY_HIGH)? 1 : 0;
	if(feed_get_filter(fp) != NULL)
		request->filtercmd = g_strdup(feed_get_filter(fp));
	
	download_queue(request);
}

/* ---------------------------------------------------------------------------- */
/* Implementation of the itemset type for feeds					*/
/* ---------------------------------------------------------------------------- */

gboolean feed_merge_check(itemSetPtr sp, itemPtr new_ip) {
	GList		*old_items;
	itemPtr		old_ip = NULL;
	gboolean	found, equal = FALSE;
	GSList		*iter, *enclosures;

	/* determine if we should add it... */
	debug1(DEBUG_VERBOSE, "check new item for merging: \"%s\"", item_get_title(new_ip));
		
	/* compare to every existing item in this feed */
	found = FALSE;
	old_items = sp->items;
	while(NULL != old_items) {
		old_ip = old_items->data;
		
		/* try to compare the two items */

		/* trivial case: one item has id the other doesn't -> they can't be equal */
		if(((item_get_id(old_ip) == NULL) && (item_get_id(new_ip) != NULL)) ||
		   ((item_get_id(old_ip) != NULL) && (item_get_id(new_ip) == NULL))) {	
			/* cannot be equal (different ids) so compare to 
			   next old item */
			old_items = g_list_next(old_items);
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
			} else {
				/* different ids, but the content might be still equal (e.g. empty)
				   so we need to explicitly unset the equal flag !!!  */
				equal = FALSE;
			}
		}
			
		if(equal) {
			found = TRUE;
			break;
		}

		old_items = g_list_next(old_items);
	}
		
	if(!found) {
		/* if new item has no number yet */
		if(0 == new_ip->nr)
			new_ip->nr = ++(sp->lastItemNr);
		
		/* ensure that the feed last item nr is at maximum */
		if(new_ip->nr > sp->lastItemNr)
			sp->lastItemNr = new_ip->nr;

		/* ensure that the item nr's are unique */
		if(NULL != itemset_lookup_item(sp, new_ip->nr)) {
			g_warning("The item number to be added is not unique! Item (%s) (%lu)\n", new_ip->title, new_ip->nr);
			new_ip->nr = ++(sp->lastItemNr);
		}

		/* If a new item has enclosures and auto downloading
		   is enabled we start the download. Enclosures added
		   by updated items are not supported. */

		// FIXME: doesn't work at this place...
		// it is a task to be decided at feed parsing
		// time and not on itemset merging time...
		
		//if((TRUE == fp->encAutoDownload) &&
		//   (TRUE == new_ip->newStatus)) {
	//		iter = enclosures = metadata_list_get(new_ip->metadata, "enclosure");
	//		while(NULL != iter) {
	//			debug1(DEBUG_UPDATE, "download enclosure (%s)", (gchar *)iter->data);
	//			ui_enclosure_save(NULL, g_strdup(iter->data), NULL);
	//			iter = g_slist_next(iter);
	//		}
	//	}
		
		debug0(DEBUG_VERBOSE, "-> item added to feed itemlist");
			
		vfolder_check_item(new_ip);
	} else {
		/* if the item was found but has other contents -> update contents */
		if(!equal) {
			/* no item_set_new_status() - we don't treat changed items as new items! */
			item_set_title(old_ip, item_get_title(new_ip));
			item_set_description(old_ip, item_get_description(new_ip));
			item_set_time(old_ip, item_get_time(new_ip));
			old_ip->updateStatus = TRUE;
			metadata_list_free(old_ip->metadata);
			old_ip->metadata = new_ip->metadata;
			vfolder_update_item(old_ip);
			debug0(DEBUG_VERBOSE, "-> item already existing and was updated");
		} else {
			debug0(DEBUG_VERBOSE, "-> item already exists");
		}
	}

	return !found;
}

/* ---------------------------------------------------------------------------- */
/* feed attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

gint feed_get_default_update_interval(feedPtr fp) { return fp->defaultInterval; }
void feed_set_default_update_interval(feedPtr fp, gint interval) { fp->defaultInterval = interval; }

gint feed_get_update_interval(feedPtr fp) { return fp->updateInterval; }

void feed_set_update_interval(feedPtr fp, gint interval) {
	
	if(0 == interval) {
		interval = -1;	/* This is evil, I know, but when this method
				   is called to set the update interval to 0
				   we mean never updating. The updating logic
				   expects -1 for never updating and 0 for
				   updating according to the global update
				   interval... */
	}
	fp->updateInterval = interval;
	feedlist_schedule_save();
}

feedHandlerPtr feed_get_fhp(feedPtr fp) {
	return fp->fhp;
}

void feed_reset_update_counter(feedPtr fp) {

	g_get_current_time(&fp->lastPoll);
	feedlist_schedule_save();
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

time_t feed_get_time(feedPtr fp) { return (fp != NULL ? fp->time : 0); }
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
	feedlist_schedule_save();
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

	fp->source = g_strchomp(g_strdup(source));
	feedlist_schedule_save();
	
	g_free(fp->cookies);
	if('|' != source[0])
		/* check if we've got matching cookies ... */
		fp->cookies = cookies_find_matching(source);
	else 
		fp->cookies = NULL;
}

void feed_set_filter(feedPtr fp, const gchar *filter) {
	g_free(fp->filtercmd);

	fp->filtercmd = g_strdup(filter);
	feedlist_schedule_save();
}

const gchar * feed_get_html_url(feedPtr fp) { return fp->htmlUrl; };
void feed_set_html_url(feedPtr fp, const gchar *htmlUrl) {

	g_free(fp->htmlUrl);
	if(htmlUrl != NULL)
		fp->htmlUrl = g_strchomp(g_strdup(htmlUrl));
	else
		fp->htmlUrl = NULL;
}

const gchar * feed_get_lastmodified(feedPtr fp) { return fp->lastModified; };
void feed_set_lastmodified(feedPtr fp, const gchar *lastmodified) {

	g_free(fp->lastModified);
	if(lastmodified != NULL)
		fp->lastModified = g_strdup(lastmodified);
	else
		fp->lastModified = NULL;
}

const gchar * feed_get_etag(feedPtr fp) { return fp->etag; };
void feed_set_etag(feedPtr fp, const gchar *etag) {

	g_free(fp->etag);
	if(etag != NULL)
		fp->etag = g_strdup(etag);
	else
		fp->etag = NULL;
}

const gchar * feed_get_image_url(feedPtr fp) { return fp->imageUrl; };
void feed_set_image_url(feedPtr fp, const gchar *imageUrl) {

	g_free(fp->imageUrl);
	if(imageUrl != NULL)
		fp->imageUrl = g_strchomp(g_strdup(imageUrl));
	else
		fp->imageUrl = NULL;
}

/**
 * Method to free all items structures of a feed, does not mean
 * that it removes items from cache! This method is used 
 * for feed unloading.
 */
// FIXME: shouldn't be needed anymore
//void feed_clear_item_list(feedPtr fp) {
//	GList	*item;
//
//	item = feed_get_item_list(fp);
//
//	while(NULL != item) {
//		item_free(item->data);
//		item = g_list_next(item);
//		/* explicitly not changing the item state counters */
//	}
//	g_list_free(fp->items);
//	fp->items = NULL;
//	/* explicitly not forcing feed saving to allow feed unloading */
//}

// FIXME: shouldn't be needed anymore
//void feed_remove_items(feedPtr fp) {
//	GList *item;
//	
//	item = feed_get_item_list(fp);
//
//	while(NULL != item) {
//		vfolder_remove_item(item->data);	/* remove item copies */
//		item_free(item->data);			/* remove the item */
//		item = g_list_next(item);
//	}
//	g_list_free(fp->items);
//	feedlist_update_counters((-1)*fp->unreadCount, (-1)*fp->newCount);
//	fp->unreadCount = 0;
//	fp->popupCount = 0;
//	fp->newCount = 0;
//	fp->items = NULL;
//}

/**
 * Creates a new error description according to the passed
 * HTTP status and the feeds parser errors. If the HTTP
 * status is a success status and no parser errors occurred
 * no error messages is created. The created error message 
 * can be queried with feed_get_error_description().
 *
 * @param fp		feed
 * @param httpstatus	HTTP status
 * @param resultcode the update code's return code (see update.h)
 */
void feed_set_error_description(feedPtr fp, gint httpstatus, gint resultcode, gchar *filterErrors) {
	gchar		*tmp1, *tmp2 = NULL, *buffer = NULL;
	gboolean	errorFound = FALSE;

	g_assert(NULL != fp);
	g_free(fp->errorDescription);
	fp->errorDescription = NULL;
	
	if(((httpstatus >= 200) && (httpstatus < 400)) && /* HTTP codes starting with 2 and 3 mean no error */
	   (NULL == filterErrors) && (NULL == fp->parseErrors))
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
			case NET_ERR_PROTO_INVALID:    tmp2 = g_strdup(_("Unsupported network protocol")); break;
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
	
	/* add filtering error messages */
	if(NULL != filterErrors) {	
		errorFound = TRUE;
		tmp1 = g_markup_printf_escaped(FILTER_ERROR_TEXT2, _("Show Details"), filterErrors);
		addToHTMLBuffer(&buffer, FILTER_ERROR_TEXT);
		addToHTMLBuffer(&buffer, tmp1);
		g_free(tmp1);
	}
	
	/* add parsing error messages */
	if(NULL != fp->parseErrors) {
		errorFound = TRUE;
		tmp1 = g_strdup_printf(PARSE_ERROR_TEXT2, _("Show Details"), fp->parseErrors);
		addToHTMLBuffer(&buffer, PARSE_ERROR_TEXT);
		addToHTMLBuffer(&buffer, tmp1);
		if (feed_get_source(fp) != NULL && (NULL != strstr(feed_get_source(fp), "://"))) {
			xmlChar *escsource;
			addToHTMLBufferFast(&buffer,_("<br>You may want to validate the feed using "
			                              "<a href=\"http://feedvalidator.org/check.cgi?url="));
			escsource = xmlURIEscapeStr(feed_get_source(fp),NULL);
			addToHTMLBufferFast(&buffer,escsource);
			xmlFree(escsource);
			addToHTMLBuffer(&buffer,_("\">FeedValidator</a>."));
		}
		addToHTMLBuffer(&buffer, "</span>");
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

// FIXME: split of vfolder_render()
gchar *feed_render(feedPtr fp) {
	struct displayset	displayset;
	gchar			*buffer = NULL;
	gchar			*tmp, *tmp2;
	xmlURIPtr		uri;

	displayset.headtable = NULL;
	displayset.head = NULL;
	displayset.body = g_strdup(feed_get_description(fp));
	displayset.foot = NULL;
	displayset.foottable = NULL;	

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
	if(NULL != feed_get_source(fp)) {
		if(feed_get_source(fp)[0] == '|') {
			tmp = g_strdup(_("user defined command"));
		} else if(feed_get_source(fp)[0] == '/') {
				tmp = g_strdup_printf("<a href=\"file://%s\">%s</a>", /* file names should be safe to display.... */
								  feed_get_source(fp),
								  feed_get_source(fp));			
		} else {
			/* remove user and password from URL ... */
			uri = xmlParseURI(feed_get_source(fp));
			if (uri != NULL) {
				g_free(uri->user);
				uri->user = NULL;
				tmp2 = xmlSaveUri(uri);
				tmp = g_strdup_printf("<a href=\"%s\">%s</a>",
								  tmp2,
								  tmp2);
				xmlFree(tmp2);
				xmlFreeURI(uri);
			} else
				tmp = g_strdup_printf("<a href=\"%s\">%s</a>",
								  feed_get_source(fp),
								  feed_get_source(fp));			
		}

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

	/* Body */
	if(displayset.body != NULL) {
		addToHTMLBufferFast(&buffer, displayset.body);
		g_free(displayset.body);
	}

	/* Foot */
	if(displayset.foot != NULL) {
		addToHTMLBufferFast(&buffer, displayset.foot);
		g_free(displayset.foot);
	}

	addToHTMLBufferFast(&buffer, "<br/><br/>");	/* instead of the technorati link image shown for items */

	if(displayset.foottable != NULL) {
		addToHTMLBufferFast(&buffer, FEED_FOOT_TABLE_START);
		addToHTMLBufferFast(&buffer, displayset.foottable);
		addToHTMLBufferFast(&buffer, FEED_FOOT_TABLE_START);
		g_free(displayset.foottable);
	}
	
	return buffer;
}

/* method to free a feed structure and associated request data */
void feed_free(feedPtr fp) {
	GSList	*iter;

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

	g_free(fp->parseErrors);
	g_free(fp->errorDescription);
	g_free(fp->title);
	g_free(fp->htmlUrl);
	g_free(fp->imageUrl);
	g_free(fp->description);
	g_free(fp->source);
	g_free(fp->filtercmd);
	g_free(fp->lastModified);
	g_free(fp->etag);
	g_free(fp->cookies);
	
	metadata_list_free(fp->metadata);
	g_hash_table_destroy(fp->tmpdata);
	g_free(fp);
}

/* method to totally erase a feed, remove it from the config, etc.... */
void feed_remove_from_cache(feedPtr fp, const gchar *id) {
	gchar	*filename = NULL;
	
	if(id && id[0] != '\0')
		filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", id, NULL);
	
	/* FIXME: Move this to a better place. The cache file does not
	   need to always be deleted, for example when freeing a
	   feedstruct used for updating. */
	if(NULL != filename) {
		if(0 != unlink(filename)) {
			/* Oh well.... Can't do anything about it. 99% of the time,
		   	this is spam anyway. */;
		}
		g_free(filename);
	}

	feed_free(fp);
}
