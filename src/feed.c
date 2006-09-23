/**
 * @file feed.c common feed handling
 * 
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include <libxml/tree.h>
#include <libxml/uri.h>
#include <string.h>
#include <time.h>
#include <unistd.h> /* For unlink() */
#include <stdlib.h>

#include "conf.h"
#include "common.h"
#include "debug.h"
#include "favicon.h"
#include "feed.h"
#include "feedlist.h"
#include "html.h"
#include "itemlist.h"
#include "metadata.h"
#include "node.h"
#include "render.h"
#include "support.h"
#include "update.h"
#include "vfolder.h"
#include "net/cookies.h"
#include "parsers/cdf_channel.h"
#include "parsers/rss_channel.h"
#include "parsers/atom10.h"
#include "parsers/pie_feed.h"
#include "ui/ui_feed.h"
#include "ui/ui_enclosure.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"
#include "notification/notif_plugin.h"
#include "scripting/script.h"

/** used for migration purposes: 1.0.x didn't specify a version */
#define FEED_CACHE_VERSION	"1.1"

/* auto detection lookup table */
static GSList *feedhandlers = NULL;

struct feed_type {
	gint id_num;
	gchar *id_str;
};

/* initializing function, only called upon startup */
void feed_init(void) {

	metadata_init();
	feedhandlers = g_slist_append(feedhandlers, rss_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, cdf_init_feed_handler());
	feedhandlers = g_slist_append(feedhandlers, atom10_init_feed_handler());  /* Must be before pie */
	feedhandlers = g_slist_append(feedhandlers, pie_init_feed_handler());
}

#define FEED_PROTOCOL_PREFIX "feed://"

/* function to create a new feed structure */
feedPtr feed_new(const gchar *source, const gchar *filter, updateOptionsPtr options) {
	feedPtr		feed;
	
	feed = g_new0(struct feed, 1);

	/* we don't allocate a request structure this is done
	   during cache loading or first update! */

	if(options)
		feed->updateOptions = options;
	else
		feed->updateOptions = g_new0(struct updateOptions, 1);
		
	feed->updateState = g_new0(struct updateState, 1);	
	feed->updateInterval = -1;
	feed->defaultInterval = -1;
	feed->cacheLimit = CACHE_DEFAULT;

	if(source) {
		gchar *tmp = g_strdup(source);
		g_strstrip(tmp);	/* strip confusing whitespaces */
		
		/* strip feed protocol prefix */
		if(tmp == strstr(tmp, FEED_PROTOCOL_PREFIX))
			tmp += strlen(FEED_PROTOCOL_PREFIX);
			
		feed_set_source(feed, tmp);
		g_free(tmp);
	}
	
	if(filter)
		feed_set_filter(feed, filter);
	
	return feed;
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

static int parse_integer(gchar *str, int def) {
	int num;

	if(str == NULL)
		return def;
	if(0==(sscanf(str,"%d",&num)))
		num = def;
	
	return num;
}

static void feed_import(nodePtr node, nodePtr parent, xmlNodePtr cur, gboolean trusted) {
	gchar		*cacheLimitStr, *filter, *intervalStr, *title; 
	gchar		*htmlUrlStr, *source, *tmp; 
	feedPtr		feed = NULL;

	debug_enter("feed_import");

	if(NULL == (source = xmlGetProp(cur, BAD_CAST"xmlUrl")))
		source = xmlGetProp(cur, BAD_CAST"xmlurl");	/* e.g. for AmphetaDesk */
		
	if(source) {
		xmlChar	*typeStr = xmlGetProp(cur, BAD_CAST"type");
		
		feed = feed_new(NULL, NULL, NULL);
		feed->fhp = feed_type_str_to_fhp(typeStr);
		xmlFree(typeStr);

		if(!trusted && source[0] == '|') {
			/* FIXME: Display warning dialog asking if the command
			   is safe? */
			tmp = g_strdup_printf("unsafe command: %s", source);
			xmlFree(source);
			source = tmp;
		}

		feed_set_source(feed, source);
		xmlFree(source);

		if((filter = xmlGetProp(cur, BAD_CAST"filtercmd"))) {
			if(!trusted) {
				/* FIXME: Display warning dialog asking if the command
				   is safe? */
				tmp = g_strdup_printf("unsafe command: %s", filter);
				xmlFree(filter);
				filter = tmp;
			}

			feed_set_filter(feed, filter);
			xmlFree(filter);
		}
		
		intervalStr = xmlGetProp(cur, BAD_CAST"updateInterval");
		feed_set_update_interval(feed, parse_integer(intervalStr, -1));
		xmlFree(intervalStr);

		title = xmlGetProp(cur, BAD_CAST"title");
		if(!title || !xmlStrcmp(title, BAD_CAST"")) {
			if(title)
				xmlFree(title);
			title = xmlGetProp(cur, BAD_CAST"text");
		}

		node_set_title(node, title);
		xmlFree(title);

		/* Set the feed cache limit */
		cacheLimitStr = xmlGetProp(cur, BAD_CAST"cacheLimit");
		if(cacheLimitStr && !xmlStrcmp(cacheLimitStr, "unlimited"))
			feed->cacheLimit = CACHE_UNLIMITED;
		else
			feed->cacheLimit = parse_integer(cacheLimitStr, CACHE_DEFAULT);
		xmlFree(cacheLimitStr);
	
		/* Obtain the htmlUrl */
		htmlUrlStr = xmlGetProp(cur, BAD_CAST"htmlUrl");
		if(htmlUrlStr && xmlStrcmp(htmlUrlStr, ""))
			feed_set_html_url(feed, htmlUrlStr);
		xmlFree(htmlUrlStr);
	
		tmp = xmlGetProp(cur, BAD_CAST"noIncremental");
		if(tmp && !xmlStrcmp(tmp, BAD_CAST"true"))
			feed->noIncremental = TRUE;
		xmlFree(tmp);
	
		/* enclosure auto download flag */
		tmp = xmlGetProp(cur, BAD_CAST"encAutoDownload");
		if(tmp && !xmlStrcmp(tmp, BAD_CAST"true"))
			feed->encAutoDownload = TRUE;
		xmlFree(tmp);
			
		/* auto item link loading flag */
		tmp = xmlGetProp(cur, BAD_CAST"loadItemLink");
		if(tmp && !xmlStrcmp(tmp, BAD_CAST"true"))
			feed->loadItemLink = TRUE;
		xmlFree(tmp);
			
		/* no proxy flag */
		tmp = xmlGetProp(cur, BAD_CAST"dontUseProxy");
		if(tmp && !xmlStrcmp(tmp, BAD_CAST"true"))
			feed->updateOptions->dontUseProxy = TRUE;
		xmlFree(tmp);
					
		update_state_import(cur, feed->updateState);
		
		node_set_icon(node, favicon_load_from_cache(node->id));
		
		if(favicon_update_needed(node_get_id(node), feed->updateState))
			node_update_favicon(node);

		debug5(DEBUG_CACHE, "import feed: title=%s source=%s typeStr=%s interval=%d lastpoll=%ld", 
		       node_get_title(node), 
		       feed_get_source(feed), 
		       typeStr, 
		       feed_get_update_interval(feed), 
		       feed->updateState->lastPoll.tv_sec);

		node_set_data(node, feed);
		node_add_child(parent, node, -1);
	}

	debug_exit("feed_import");
}

static void feed_export(nodePtr node, xmlNodePtr cur, gboolean trusted) {
	feedPtr feed = (feedPtr)node->data;

	debug_enter("feed_export");

	gchar *interval = g_strdup_printf("%d",feed_get_update_interval(feed));
	gchar *cacheLimit = NULL;

	if(feed_get_html_url(feed))
		xmlNewProp(cur, BAD_CAST"htmlUrl", BAD_CAST feed_get_html_url(feed));
	else
		xmlNewProp(cur, BAD_CAST"htmlUrl", BAD_CAST "");
	xmlNewProp(cur, BAD_CAST"xmlUrl", BAD_CAST feed_get_source(feed));

	if(feed_get_filter(feed))
		xmlNewProp(cur, BAD_CAST"filtercmd", BAD_CAST feed_get_filter(feed));

	if(trusted) {
		xmlNewProp(cur, BAD_CAST"updateInterval", BAD_CAST interval);
		
		if(feed->cacheLimit >= 0)
			cacheLimit = g_strdup_printf("%d", feed->cacheLimit);
		if(feed->cacheLimit == CACHE_UNLIMITED)
			cacheLimit = g_strdup("unlimited");
		if(cacheLimit)
			xmlNewProp(cur, BAD_CAST"cacheLimit", BAD_CAST cacheLimit);

		if(feed->noIncremental)
			xmlNewProp(cur, BAD_CAST"noIncremental", BAD_CAST"true");
			
		if(TRUE == feed->encAutoDownload)
			xmlNewProp(cur, BAD_CAST"encAutoDownload", BAD_CAST"true");
			
		if(TRUE == feed->loadItemLink)
			xmlNewProp(cur, BAD_CAST"loadItemLink", BAD_CAST"true");
			
		if(TRUE == feed->updateOptions->dontUseProxy)
			xmlNewProp(cur, BAD_CAST"dontUseProxy", BAD_CAST"true");
	}

	update_state_export(cur, feed->updateState);
	
	debug3(DEBUG_CACHE, "adding feed: source=%s interval=%s cacheLimit=%s", feed_get_source(feed), interval, cacheLimit);
	g_free(cacheLimit);
	g_free(interval);

	debug_exit("feed_export");
}

feedParserCtxtPtr feed_create_parser_ctxt(void) {
	feedParserCtxtPtr ctxt;

	ctxt = g_new0(struct feedParserCtxt, 1);
	ctxt->itemSet = (itemSetPtr)g_new0(struct itemSet, 1);
	ctxt->itemSet->valid = TRUE;
	ctxt->tmpdata = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	return ctxt;
}

void feed_free_parser_ctxt(feedParserCtxtPtr ctxt) {

	if(NULL != ctxt) {
		/* Don't free the itemset! */
		g_hash_table_destroy(ctxt->tmpdata);
		g_free(ctxt);
	}
}

/**
 * General feed source parsing function. Parses the passed feed source
 * and tries to determine the source type. If the type is HTML and 
 * autodiscover is TRUE the function tries to find a feed, tries to
 * download it and parse the feed's source instead of the passed source.
 *
 * @param ctxt		feed parsing context
 * @param autodiscover	TRUE if auto discovery should be possible
 */
void feed_parse(feedParserCtxtPtr ctxt, gboolean autodiscover) {
	xmlNodePtr	cur;
	gchar		*source;

	debug_enter("feed_parse");

	g_assert(NULL != ctxt->feed);
	g_assert(NULL != ctxt->node);
	g_assert(NULL == ctxt->itemSet->items);
	
	ctxt->failed = TRUE;	/* reset on success ... */
	
	if(ctxt->feed->parseErrors)
		g_string_truncate(ctxt->feed->parseErrors, 0);
	else
		ctxt->feed->parseErrors = g_string_new(NULL);

	/* try to parse buffer with XML and to create a DOM tree */	
	do {
		if(NULL == common_parse_xml_feed(ctxt)) {
			g_string_append_printf(ctxt->feed->parseErrors, _("<p>XML error while reading feed! Feed \"%s\" could not be loaded!</p>"), ctxt->feed->source);
			break;
		}
		
		if(NULL == (cur = xmlDocGetRootElement(ctxt->doc))) {
			g_string_append(ctxt->feed->parseErrors, _("<p>Empty document!</p>"));
			break;
		}
		
		while(cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
		
		if(!cur->name) {
			g_string_append(ctxt->feed->parseErrors, _("<p>Invalid XML!</p>"));
			break;
		}
		
		if(!cur)
			break;
			
		/* determine the syndication format */
		GSList *handlerIter = feedhandlers;
		while(handlerIter) {
			feedHandlerPtr handler = (feedHandlerPtr)(handlerIter->data);
			if(handler && handler->checkFormat && (*(handler->checkFormat))(ctxt->doc, cur)) {
				/* free old temp. parsing data, don't free right after parsing because
				   it can be used until the last feed request is finished, move me 
				   to the place where the last request in list otherRequests is 
				   finished :-) */
				g_hash_table_destroy(ctxt->tmpdata);
				ctxt->tmpdata = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
				
				/* we always drop old metadata */
				metadata_list_free(ctxt->feed->metadata);
				ctxt->feed->metadata = NULL;
				ctxt->failed = FALSE;

				ctxt->feed->fhp = handler;
				(*(handler->feedParser))(ctxt, cur);		/* parse it */

				break;
			}
			handlerIter = handlerIter->next;
		}
	} while(0);
	
	/* if we don't have a feed type here we don't have a feed source yet or
	   the feed source is no more valid and we need to start auto discovery */
	if(!ctxt->feed->fhp) {
		/* test if we have a HTML page */
		if(autodiscover && 
		   (strstr(ctxt->data, "<html>") || strstr(ctxt->data, "<HTML>") ||
		    strstr(ctxt->data, "<html ") || strstr(ctxt->data, "<HTML "))) {
			/* if yes we should scan for links */
			debug1(DEBUG_UPDATE, "HTML detected, starting feed auto discovery (%s)", feed_get_source(ctxt->feed));
			if((source = html_auto_discover_feed(ctxt->data, feed_get_source(ctxt->feed)))) {
				/* now download the first feed link found */
				requestPtr request = update_request_new(ctxt->node);
				debug1(DEBUG_UPDATE, "feed link found: %s", source);
				request->source = g_strdup(source);
				request->options = ctxt->feed->updateOptions;
				update_execute_request_sync(request);
				if(request->data) {
					debug0(DEBUG_UPDATE, "feed link download successful!");
					feed_set_source(ctxt->feed, source);
					ctxt->data = request->data;
					ctxt->dataLength = request->size;
					ctxt->failed = FALSE;
					feed_parse(ctxt, FALSE);
				} else {
					/* if the download fails we do nothing except
					   unsetting the handler so the original source
					   will get a "unsupported type" error */
					debug0(DEBUG_UPDATE, "feed link download failed!");
				}
				g_free(source);
				update_request_free(request);
			} else {
				debug0(DEBUG_UPDATE, "no feed link found!");
				ctxt->feed->available = FALSE;
				g_string_append(ctxt->feed->parseErrors, _("<p>The URL you want Liferea to subscribe to points to a webpage and the auto discovery found no feeds on this page. Maybe this webpage just does not support feed auto discovery.</p>"));
			}
		} else {
			debug0(DEBUG_UPDATE, "neither a known feed type nor a HTML document!");
			ctxt->feed->available = FALSE;
			g_string_append(ctxt->feed->parseErrors, _("<p>Could not determine the feed type.</p>"));
		}
	} else {
		debug1(DEBUG_UPDATE, "discovered feed format: %s", feed_type_fhp_to_str(ctxt->feed->fhp));
	}
	
	if(ctxt->doc) {
		xmlFreeDoc(ctxt->doc);
		ctxt->doc = NULL;
	}
		
	debug_exit("feed_parse");
}

static void feed_add_xml_attributes(nodePtr node, xmlNodePtr feedNode, gboolean rendering) {
	feedPtr	feed = (feedPtr)node->data;
	gchar	*tmp;
	
	xmlNewProp(feedNode, "version", BAD_CAST FEED_CACHE_VERSION);

	xmlNewTextChild(feedNode, NULL, "feedId", node_get_id(node));
	xmlNewTextChild(feedNode, NULL, "feedTitle", node_get_title(node));
	xmlNewTextChild(feedNode, NULL, "feedSource", feed_get_source(feed));
	xmlNewTextChild(feedNode, NULL, "feedOrigSource", feed_get_orig_source(feed));

	if(feed->description)
		xmlNewTextChild(feedNode, NULL, "feedDescription", feed->description);

	if(feed_get_image_url(feed))
		xmlNewTextChild(feedNode, NULL, "feedImage", feed_get_image_url(feed));

	tmp = g_strdup_printf("%d", feed->defaultInterval);
	xmlNewTextChild(feedNode, NULL, "feedUpdateInterval", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", feed->available?1:0);
	xmlNewTextChild(feedNode, NULL, "feedStatus", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", feed->discontinued?1:0);
	xmlNewTextChild(feedNode, NULL, "feedDiscontinued", tmp);
	g_free(tmp);

	if(rendering) {
		tmp = g_strdup_printf("file://%s", node_get_favicon_file(node));
		xmlNewTextChild(feedNode, NULL, "favicon", tmp);
		g_free(tmp);
		
		xmlNewTextChild(feedNode, NULL, "feedLink", feed_get_html_url(feed));

		if(feed->updateError)
			xmlNewTextChild(feedNode, NULL, "updateError", feed->updateError);
		if(feed->httpError) {
			xmlNewTextChild(feedNode, NULL, "httpError", feed->httpError);
			
			tmp = g_strdup_printf("%d", feed->httpErrorCode);
			xmlNewTextChild(feedNode, NULL, "httpErrorCode", tmp);
			g_free(tmp);
		}
		if(feed->filterError)
			xmlNewTextChild(feedNode, NULL, "filterError", feed->filterError);
		if(feed->parseErrors && (strlen(feed->parseErrors->str) > 0))
			xmlNewTextChild(feedNode, NULL, "parseError", feed->parseErrors->str);
	}

	metadata_add_xml_nodes(feed->metadata, feedNode);
}

xmlDocPtr feed_to_xml(nodePtr node, xmlNodePtr feedNode, gboolean rendering) {
	xmlDocPtr	doc = NULL;
	
	if(!feedNode) {
		doc = xmlNewDoc("1.0");
		feedNode = xmlDocGetRootElement(doc);
		feedNode = xmlNewDocNode(doc, NULL, "feed", NULL);
		xmlDocSetRootElement(doc, feedNode);
	}
	feed_add_xml_attributes(node, feedNode, rendering);
	
	return doc;
}

guint feed_get_max_item_count(nodePtr node) {
	feedPtr	feed = (feedPtr)node->data;
	guint	max;
	
	max = feed->cacheLimit;
	if(max == CACHE_DEFAULT)
		max = getNumericConfValue(DEFAULT_MAX_ITEMS);
		
	return max;
}

/*
 * Feeds caches are marked to be saved at a few different places:
 * (1) Inside whe feed_set_* functions where an item is marked or made read or unread
 * (2) Inside of feed_process_result
 * (3) The callback where items are removed from the itemlist
 *
 * This method really saves the feed to disk.
 */
static void feed_save_to_cache(nodePtr node) {
	feedPtr		feed = (feedPtr)node->data;
	gchar		*filename, *tmpfilename;
	gint		saveCount = 0;
	gint		saveMaxCount;
	GList		*iter, *itemlist;
	xmlDocPtr 	doc;
			
	debug_enter("feed_save_to_cache");	

	debug2(DEBUG_CACHE, "saving feed: %s (id=%s)", feed->source, node->id);

	saveMaxCount = feed_get_max_item_count(node);

	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", node->id, NULL);
	tmpfilename = g_strdup_printf("%s~", filename);
	
	/* Create the feed XML document */
	if(NULL != (doc = feed_to_xml(node, NULL, FALSE))) {
		/* If necessary drop items according to cache settings
		   otherwise add them to the feed XML document */		
		itemlist = g_list_copy(node->itemSet->items);
		for(iter = itemlist; iter != NULL; iter = g_list_next(iter)) {
			itemPtr ip = iter->data;

			if(saveMaxCount == CACHE_DISABLE)
				continue;

			if((saveMaxCount != CACHE_UNLIMITED) &&
			   (saveCount >= saveMaxCount) &&
			   (feed->fhp == NULL || feed->fhp->directory == FALSE) &&
			   ! ip->flagStatus) {
				itemlist_remove_item(ip);
			} else {
				item_to_xml(ip, xmlDocGetRootElement(doc), FALSE);
				saveCount++;
			}
		}
		g_list_free(itemlist);

		if(xmlSaveFormatFile(tmpfilename, doc,1) == -1) {
			g_warning("Error attempting to save feed cache file \"%s\"!", tmpfilename);
		} else {
			if(rename(tmpfilename, filename) == -1)
				perror("Error overwriting old cache file"); /* Nothing else can be done... probably the disk is going bad */
		}
		xmlFreeDoc(doc);
	}	
	g_free(tmpfilename);
	g_free(filename);

	debug_exit("feed_save_to_cache");
}

itemSetPtr feed_load_from_cache(nodePtr node) {
	feedParserCtxtPtr	ctxt;
	feedPtr			feed = (feedPtr)(node->data);
	itemSetPtr		itemSet;
	gboolean		migrateCache = TRUE;
	gchar			*filename;
	int			error = 0;

	debug_enter("feed_load_from_cache");
	
	if(feed->parseErrors)
		g_string_truncate(feed->parseErrors, 0);
	else
		feed->parseErrors = g_string_new(NULL);

	ctxt = feed_create_parser_ctxt();
	ctxt->feed = feed;
	ctxt->node = node;
	
	itemSet = ctxt->itemSet;
	itemSet->type = ITEMSET_TYPE_FEED;
	
	filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", node->id, NULL);
	debug2(DEBUG_CACHE, "loading cache file \"%s\" (feed \"%s\")", filename, feed_get_source(feed));
	
	if((!g_file_get_contents(filename, &ctxt->data, &ctxt->dataLength, NULL)) || (*ctxt->data == 0)) {
		debug1(DEBUG_CACHE, "could not load cache file %s", filename);
		ui_mainwindow_set_status_bar(_("Error while reading cache file \"%s\" ! Cache file could not be loaded!"), filename);
		g_free(filename);
		return itemSet;
	}

	do {
		xmlNodePtr cur;
		
		g_assert(NULL != ctxt->data);

		if(NULL == common_parse_xml_feed(ctxt)) {
			g_string_append_printf(feed->parseErrors, _("<p>XML error while parsing cache file! Feed cache file \"%s\" could not be loaded!</p>"), filename);
			error = 1;
			break;
		} 

		if(NULL == (cur = xmlDocGetRootElement(ctxt->doc))) {
			g_string_append(feed->parseErrors, _("<p>Empty document!</p>"));
			error = 1;
			break;
		}

		while(cur && xmlIsBlankNode(cur))
			cur = cur->next;

		if(!xmlStrcmp(cur->name, BAD_CAST"feed")) {
			xmlChar *version;			
			if((version = xmlGetProp(cur, BAD_CAST"version"))) {
				migrateCache = xmlStrcmp(BAD_CAST FEED_CACHE_VERSION, version);
				xmlFree(version);
			}
		} else {
			g_string_append_printf(feed->parseErrors, _("<p>\"%s\" is no valid cache file! Cannot read cache file!</p>"), filename);
			error = 1;
			break;		
		}

		metadata_list_free(feed->metadata);
		feed->metadata = NULL;

		cur = cur->xmlChildrenNode;
		while(cur) {
			gchar *tmp = utf8_fix(xmlNodeListGetString(ctxt->doc, cur->xmlChildrenNode, 1));

			if(!tmp) {
				cur = cur->next;
				continue;
			}

			if(!xmlStrcmp(cur->name, BAD_CAST"feedDescription")) 
				feed_set_description(feed, tmp);
				
			if(!xmlStrcmp(cur->name, BAD_CAST"feedOrigSource")) 
				feed_set_orig_source(feed, tmp);

			else if(!node->title && !xmlStrcmp(cur->name, BAD_CAST"feedTitle")) 
				node_set_title(node, tmp);

			else if(!xmlStrcmp(cur->name, BAD_CAST"feedUpdateInterval")) 
				feed_set_default_update_interval(feed, atoi(tmp));

			else if(!xmlStrcmp(cur->name, BAD_CAST"feedImage")) 
				feed_set_image_url(feed, tmp);

			else if(!xmlStrcmp(cur->name, BAD_CAST"feedStatus")) 
				feed->available = (0 == atoi(tmp))?FALSE:TRUE;

			else if(!xmlStrcmp(cur->name, BAD_CAST"feedDiscontinued")) 
				feed->discontinued = (0 == atoi(tmp))?FALSE:TRUE;

			else if(!xmlStrcmp(cur->name, BAD_CAST"item")) 
				itemset_append_item(itemSet, item_parse_cache(cur, migrateCache));

			else if(!xmlStrcmp(cur->name, BAD_CAST"attributes")) 
				feed->metadata = metadata_parse_xml_nodes(cur);

			g_free(tmp);	
			cur = cur->next;
		}
	} while(FALSE);
	
	if(migrateCache) {
		node->needsCacheSave = TRUE;
		g_print("enabling cache migration from 1.0 for feed \"%s\"\n", node_get_title(node));	
		
		if(feed->description)
			feed_set_description(feed, common_text_to_xhtml(feed->description));
	}

	if(0 != error)
		ui_mainwindow_set_status_bar(_("There were errors while parsing cache file \"%s\""), filename);

	if(ctxt->doc)
		xmlFreeDoc(ctxt->doc);
		
	g_free(ctxt->data);
	g_free(filename);
	feed_free_parser_ctxt(ctxt);
	
	debug_exit("feed_load_from_cache");
	return itemSet;
}

// FIXME: needed?
void feed_cancel_retry(nodePtr node) {

	if(node->updateRequest && update_request_cancel_retry(node->updateRequest))
		node->updateRequest = NULL;
}

/* Checks wether updating a feed makes sense. */
gboolean feed_can_be_updated(nodePtr node) {
	feedPtr		feed = (feedPtr)node->data;

	if(node->updateRequest) {
		ui_mainwindow_set_status_bar(_("This feed \"%s\" is already being updated!"), node_get_title(node));
		return FALSE;
	}
	
	if(feed->discontinued) {
		ui_mainwindow_set_status_bar(_("The feed \"%s\" was discontinued. Liferea won't update it anymore!"), node_get_title(node));
		return FALSE;
	}

	if(!feed_get_source(feed)) {
		g_warning("Feed source is NULL! This should never happen - cannot update!");
		return FALSE;
	}
	return TRUE;
}	

static void feed_reset_update_counter_(feedPtr fp) {

	g_get_current_time(&fp->updateState->lastPoll);
	feedlist_schedule_save();
	debug1(DEBUG_UPDATE, "Resetting last poll counter to %ld.\n", fp->updateState->lastPoll.tv_sec);
}

static void feed_prepare_request(feedPtr feed, struct request *request, guint flags) {

	debug1(DEBUG_UPDATE, "preparing request for \"%s\"\n", feed_get_source(feed));

	feed_reset_update_counter_(feed);

	/* prepare request url (strdup because it might be
  	   changed on permanent HTTP redirection in netio.c) */
	request->source = g_strdup(feed_get_source(feed));
	request->updateState = feed->updateState;
	request->flags = flags;
	request->priority = (flags & FEED_REQ_PRIORITY_HIGH)? 1 : 0;
	request->allowRetries = (flags & FEED_REQ_ALLOW_RETRIES)? 1 : 0;
	if(feed_get_filter(feed))
		request->filtercmd = g_strdup(feed_get_filter(feed));
}

/* ---------------------------------------------------------------------------- */
/* Implementation of the itemset type for feeds					*/
/* ---------------------------------------------------------------------------- */

gboolean feed_merge_check(itemSetPtr sp, itemPtr new_ip) {
	GList		*old_items;
	itemPtr		old_ip = NULL;
	gboolean	found, equal = FALSE;

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

		/* If a new item has enclosures and auto downloading
		   is enabled we start the download. Enclosures added
		   by updated items are not supported. */

		if((TRUE == ((feedPtr)(sp->node->data))->encAutoDownload) &&
		   (TRUE == new_ip->newStatus)) {
			GSList *iter = metadata_list_get(new_ip->metadata, "enclosure");
			while(iter) {
				debug1(DEBUG_UPDATE, "download enclosure (%s)", (gchar *)iter->data);
				ui_enclosure_save(NULL, g_strdup(iter->data), NULL);
				iter = g_slist_next(iter);
			}
		}
		
		debug0(DEBUG_VERBOSE, "-> item is to be added");
	} else {
		/* if the item was found but has other contents -> update contents */
		if(!equal) {
			if(new_ip->itemSet->valid) {	
				/* no item_set_new_status() - we don't treat changed items as new items! */
				item_set_title(old_ip, item_get_title(new_ip));
				item_set_description(old_ip, item_get_description(new_ip));
				old_ip->time = new_ip->time;
				old_ip->updateStatus = TRUE;
				metadata_list_free(old_ip->metadata);
				old_ip->metadata = new_ip->metadata;
				new_ip->metadata = NULL;
				vfolder_update_item(old_ip);
				debug0(DEBUG_VERBOSE, "-> item already existing and was updated");
			} else {
				debug0(DEBUG_VERBOSE, "-> item updates not merged because of parser errors");
			}
		} else {
			debug0(DEBUG_VERBOSE, "-> item already exists");
		}
	}

	return !found;
}

/* ---------------------------------------------------------------------------- */
/* feed attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

gint feed_get_default_update_interval(feedPtr feed) { return feed->defaultInterval; }
void feed_set_default_update_interval(feedPtr feed, gint interval) { feed->defaultInterval = interval; }

gint feed_get_update_interval(feedPtr fp) { return fp->updateInterval; }

void feed_set_update_interval(feedPtr feed, gint interval) {
	
	if(0 == interval) {
		interval = -1;	/* This is evil, I know, but when this method
				   is called to set the update interval to 0
				   we mean never updating. The updating logic
				   expects -1 for never updating and 0 for
				   updating according to the global update
				   interval... */
	}
	feed->updateInterval = interval;
	feedlist_schedule_save();
}

feedHandlerPtr feed_get_fhp(feedPtr feed) {
	return feed->fhp;
}

const gchar * feed_get_description(feedPtr feed) { return feed->description; }
void feed_set_description(feedPtr fp, const gchar *description) {
	g_free(fp->description);
	if(description != NULL)
		fp->description = g_strdup(description);
	else
		fp->description = NULL;
}

const gchar * feed_get_orig_source(feedPtr feed) { return feed->origSource; }
const gchar * feed_get_source(feedPtr feed) { return feed->source; }
const gchar * feed_get_filter(feedPtr feed) { return feed->filtercmd; }

void feed_set_orig_source(feedPtr feed, const gchar *source) {

	g_free(feed->origSource);
	feed->origSource = g_strchomp(g_strdup(source));
	feedlist_schedule_save();
}

void feed_set_source(feedPtr feed, const gchar *source) {

	g_free(feed->source);

	feed->source = g_strchomp(g_strdup(source));
	feedlist_schedule_save();
	
	g_free(feed->updateState->cookies);
	if('|' != source[0])
		/* check if we've got matching cookies ... */
		feed->updateState->cookies = cookies_find_matching(source);
	else 
		feed->updateState->cookies = NULL;
	
	if(NULL == feed_get_orig_source(feed))
		feed_set_orig_source(feed, source);
}

void feed_set_filter(feedPtr feed, const gchar *filter) {
	g_free(feed->filtercmd);

	feed->filtercmd = g_strdup(filter);
	feedlist_schedule_save();
}

const gchar * feed_get_html_url(feedPtr feed) { return feed->htmlUrl; };
void feed_set_html_url(feedPtr feed, const gchar *htmlUrl) {

	g_free(feed->htmlUrl);
	if(htmlUrl)
		feed->htmlUrl = g_strchomp(g_strdup(htmlUrl));
	else
		feed->htmlUrl = NULL;
}

const gchar * feed_get_image_url(feedPtr feed) { return feed->imageUrl; };
void feed_set_image_url(feedPtr feed, const gchar *imageUrl) {

	g_free(feed->imageUrl);
	if(imageUrl != NULL)
		feed->imageUrl = g_strchomp(g_strdup(imageUrl));
	else
		feed->imageUrl = NULL;
}

/**
 * Creates a new error description according to the passed
 * HTTP status and the feeds parser errors. If the HTTP
 * status is a success status and no parser errors occurred
 * no error messages is created.
 *
 * @param fp		feed
 * @param httpstatus	HTTP status
 * @param resultcode	the update code's return code (see update.h)
 */
static void feed_update_error_status(feedPtr feed, gint httpstatus, gint resultcode, gchar *filterError) {
	gchar		*tmp = NULL;
	gboolean	errorFound = FALSE;

	if(feed->filterError)
		g_free(feed->filterError);
	if(feed->httpError)
		g_free(feed->httpError);
	if(feed->updateError)
		g_free(feed->updateError);
		
	feed->filterError = g_strdup(filterError);
	feed->updateError = NULL;
	feed->httpError = NULL;
	feed->httpErrorCode = httpstatus;
	
	if(((httpstatus >= 200) && (httpstatus < 400)) && /* HTTP codes starting with 2 and 3 mean no error */
	   (NULL == feed->filterError))
		return;
	
	if((200 != httpstatus) || (resultcode != NET_ERR_OK)) {
		/* first specific codes */
		switch(httpstatus) {
			case 401:tmp = _("You are unauthorized to download this feed. Please update your username and "
			                 "password in the feed properties dialog box.");break;
			case 402:tmp = _("Payment Required");break;
			case 403:tmp = _("Access Forbidden");break;
			case 404:tmp = _("Resource Not Found");break;
			case 405:tmp = _("Method Not Allowed");break;
			case 406:tmp = _("Not Acceptable");break;
			case 407:tmp = _("Proxy Authentication Required");break;
			case 408:tmp = _("Request Time-Out");break;
			case 410:tmp = _("Gone. Resource doesn't exist. Please unsubscribe!");break;
		}
		/* Then, netio errors */
		if(!NULL) {
			switch(resultcode) {
				case NET_ERR_URL_INVALID:    tmp = _("URL is invalid"); break;
				case NET_ERR_PROTO_INVALID:  tmp = _("Unsupported network protocol"); break;
				case NET_ERR_UNKNOWN:
				case NET_ERR_CONN_FAILED:
				case NET_ERR_SOCK_ERR:       tmp = _("Error connecting to remote host"); break;
				case NET_ERR_HOST_NOT_FOUND: tmp = _("Hostname could not be found"); break;
				case NET_ERR_CONN_REFUSED:   tmp = _("Network connection was refused by the remote host"); break;
				case NET_ERR_TIMEOUT:        tmp = _("Remote host did not finish sending data"); break;
					/* Transfer errors */
				case NET_ERR_REDIRECT_COUNT_ERR: tmp = _("Too many HTTP redirects were encountered"); break;
				case NET_ERR_REDIRECT_ERR:
				case NET_ERR_HTTP_PROTO_ERR: 
				case NET_ERR_GZIP_ERR:           tmp = _("Remote host sent an invalid response"); break;
					/* These are handled above	
					   case NET_ERR_HTTP_410:
					   case NET_ERR_HTTP_404:
					   case NET_ERR_HTTP_NON_200:
					*/
				case NET_ERR_AUTH_FAILED:
				case NET_ERR_AUTH_NO_AUTHINFO: tmp = _("Authentication failed"); break;
				case NET_ERR_AUTH_GEN_AUTH_ERR:
				case NET_ERR_AUTH_UNSUPPORTED: tmp = _("Webserver's authentication method incompatible with Liferea"); break;
			}
		}
		/* And generic messages in the unlikely event that the above didn't work */
		if(!tmp) {
			switch(httpstatus / 100) {
				case 3:tmp = _("Feed not available: Server requested unsupported redirection!");break;
				case 4:tmp = _("Client Error");break;
				case 5:tmp = _("Server Error");break;
				default:tmp = _("(unknown networking error happened)");break;
			}
		}
		errorFound = TRUE;
		feed->httpError = g_strdup(tmp);
	}
	
	/* if none of the above error descriptions matched... */
	if(!errorFound)
		feed->updateError = g_strdup(_("There was a problem while reading this subscription. Please check the URL and console output."));
}

/* method to free a feed structure and associated request data */
static void feed_free(feedPtr feed) {

	g_string_free(feed->parseErrors, TRUE);
	g_free(feed->updateError);
	g_free(feed->filterError);
	g_free(feed->httpError);
	g_free(feed->htmlUrl);
	g_free(feed->imageUrl);
	g_free(feed->description);
	g_free(feed->source);
	g_free(feed->filtercmd);

	g_free(feed->updateOptions);
	update_state_free(feed->updateState);
	metadata_list_free(feed->metadata);
	g_free(feed);
}

/* method to totally erase the cache file of a given feed.... */
static void feed_remove_from_cache(nodePtr node) {
	gchar		*filename = NULL;
	
	if(node->id && node->id[0] != '\0')
		filename = common_create_cache_filename("cache" G_DIR_SEPARATOR_S "feeds", node->id, NULL);

	if(filename) {
		if(0 != unlink(filename)) {
			/* Oh well.... Can't do anything about it. 99% of the time,
		   	this is spam anyway. */;
		}
		g_free(filename);
	}

	feed_free(node->data);
}

static void feed_favicon_downloaded(gpointer user_data) {
	nodePtr	node = (nodePtr)user_data;
	
	node_set_icon(node, favicon_load_from_cache(node_get_id(node)));
	ui_node_update(node);
}

void feed_update_favicon(nodePtr node) {
	feedPtr		feed = (feedPtr)node->data;
	
	debug1(DEBUG_UPDATE, "trying to download favicon.ico for \"%s\"\n", node_get_title(node));
	ui_mainwindow_set_status_bar(_("Updating feed icon for \"%s\""), node_get_title(node));
	node->needsCacheSave = TRUE;
	g_get_current_time(&feed->updateState->lastFaviconPoll);
	favicon_download(node->id, 
	                 feed_get_html_url(feed), 
			 feed_get_source(feed),
			 feed->updateOptions,
	                 feed_favicon_downloaded, 
			 (gpointer)node);
	
}

/* implementation of feed node update request processing callback */

void feed_process_update_result(struct request *request) {
	feedParserCtxtPtr	ctxt;
	nodePtr			node = (nodePtr)request->user_data;
	feedPtr			feed = (feedPtr)node->data;
	gchar			*old_title, *old_source;
	gint			old_update_interval;

	debug_enter("ui_feed_process_update_result");
	
	node_load(node);

	/* no matter what the result of the update is we need to save update
	   status and the last update time to cache */
	node->needsCacheSave = TRUE;
	feed->available = FALSE;
	
	/* note this is to update the feed URL on permanent redirects */
	if(!strcmp(request->source, feed_get_source(feed))) {
		feed_set_source(feed, request->source);
		ui_mainwindow_set_status_bar(_("The URL of \"%s\" has changed permanently and was updated"), node_get_title(node));
	}
	
	if(401 == request->httpstatus) { /* unauthorized */
		if(request->flags & FEED_REQ_AUTH_DIALOG)
			ui_feed_authdialog_new(node, request->flags);
	} else if(410 == request->httpstatus) { /* gone */
		feed->discontinued = TRUE;
		feed->available = TRUE;
		ui_mainwindow_set_status_bar(_("\"%s\" is discontinued. Liferea won't updated it anymore!"), node_get_title(node));
	} else if(304 == request->httpstatus) {
		feed->available = TRUE;
		ui_mainwindow_set_status_bar(_("\"%s\" has not changed since last update"), node_get_title(node));
	} else if(NULL != request->data) {
		/* we save all properties that should not be overwritten in all cases */
		old_update_interval = feed_get_update_interval(feed);
		old_title = g_strdup(node_get_title(node));
		old_source = g_strdup(feed_get_source(feed));

		/* parse the new downloaded feed into feed and itemSet */
		ctxt = feed_create_parser_ctxt();
		ctxt->feed = feed;
		ctxt->node = node;
		ctxt->data = request->data;
		ctxt->dataLength = request->size;
		feed_parse(ctxt, request->flags & FEED_REQ_AUTO_DISCOVER);

		if(ctxt->failed) {
			g_string_prepend(feed->parseErrors, _("<p>Could not detect the type of this feed! Please check if the source really points to a resource provided in one of the supported syndication formats!</p>"
			                                      "XML Parser Output:<br /><div class='xmlparseroutput'>"));
			g_string_append(feed->parseErrors, "</div>");
		} else {
			/* merge the resulting items into the node's item set */
			node_merge_items(node, ctxt->itemSet->items);
		
			/* restore user defined properties if necessary */
			if(!(request->flags & FEED_REQ_RESET_TITLE)) 
				node_set_title(node, old_title);
				
			if(!(request->flags & FEED_REQ_AUTO_DISCOVER))
				feed_set_source(feed, old_source);

			if(request->flags & FEED_REQ_RESET_UPDATE_INT)
				feed_set_update_interval(feed, feed_get_default_update_interval(feed));
			else
				feed_set_update_interval(feed, old_update_interval);

			ui_mainwindow_set_status_bar(_("\"%s\" updated..."), node_get_title(node));

			itemlist_merge_itemset(node->itemSet);
			
			if(request->flags & FEED_REQ_SHOW_PROPDIALOG)
				ui_feed_properties(node);

			feed->available = TRUE;
		}
				
		g_free(old_title);
		g_free(old_source);

		feed_free_parser_ctxt(ctxt);
	} else {	
		ui_mainwindow_set_status_bar(_("\"%s\" is not available"), node_get_title(node));
	}
	
	feed_update_error_status(feed, request->httpstatus, request->returncode, request->filterErrors);

	node->updateRequest = NULL; 

	if(request->flags & FEED_REQ_DOWNLOAD_FAVICON)
		feed_update_favicon(node);

	ui_node_update(node);
	notification_node_has_new_items(node);
	node_unload(node);
	
	script_run_for_hook(SCRIPT_HOOK_FEED_UPDATED);

	debug_exit("ui_feed_process_update_result");
}

/* implementation of the node type interface */

static void feed_load(nodePtr node) {

	debug2(DEBUG_CACHE, "+ feed_load (%s, ref count=%d)", node_get_title(node), node->loaded);
	node->loaded++;

	if(1 < node->loaded) {
		debug1(DEBUG_CACHE, "no loading %s because it is already loaded", node_get_title(node));
		return;
	}
	
	/* node->itemSet will be NULL here, except when cache is disabled */
	node_set_itemset(node, feed_load_from_cache(node));
	g_assert(NULL != node->itemSet);

	debug2(DEBUG_CACHE, "- feed_load (%s, new ref count=%d)", node_get_title(node), node->loaded);
}

static void feed_save(nodePtr node) {

	if(0 == node->loaded)
		return;

	g_assert(NULL != node->itemSet);

	if(FALSE == node->needsCacheSave)
		return;

	feed_save_to_cache(node);
	node->needsCacheSave = FALSE;
}

static void feed_unload(nodePtr node) {
	feedPtr feed = (feedPtr)node->data;
	GList	*iter;

	debug2(DEBUG_CACHE, "+ node_unload (%s, ref count=%d)", node_get_title(node), node->loaded);

	if(0 >= node->loaded) {
		debug0(DEBUG_CACHE, "node is not loaded, nothing to do...");
		return;
	}

	node_save(node);	/* save before unloading */

	if(!getBooleanConfValue(KEEP_FEEDS_IN_MEMORY)) {
		if(1 == node->loaded) {
			g_assert(NULL != node->itemSet);
			if(CACHE_DISABLE == feed->cacheLimit) {
				debug1(DEBUG_CACHE, "not unloading node (%s) because cache is disabled", node_get_title(node));
			} else {
				debug1(DEBUG_CACHE, "unloading node (%s)", node_get_title(node));
				g_assert(NULL != node->itemSet);
				iter = node->itemSet->items;
				while(iter) {
					item_free((itemPtr)iter->data);
					iter = g_list_next(iter);
				}
				g_list_free(node->itemSet->items);
				g_free(node->itemSet);
				node->itemSet = NULL;	
			} 
			/* node->itemSet will be NULL here, except when cache is disabled */
		} else {
			debug1(DEBUG_CACHE, "not unloading %s because it is still used", node_get_title(node));
		}
		node->loaded--;
	}
	debug2(DEBUG_CACHE, "- node_unload (%s, new ref count=%d)", node_get_title(node), node->loaded);
}

/**
 * Used to process feeds directly after feed list loading.
 * Loads the given feed or requests a download. During feed
 * loading its items are automatically checked against all 
 * vfolder rules.
 */
static void feed_initial_load(nodePtr node) {

	feed_load(node);
	vfolder_check_node(node);	/* copy items to matching vfolders */
	feed_unload(node);
	ui_node_update(node);
}

static void feed_reset_update_counter(nodePtr node) {

	feed_reset_update_counter_((feedPtr)node->data);
}

static void feed_schedule_update(nodePtr node, guint flags) {
	feedPtr			feed = (feedPtr)node->data;
	struct request		*request;
	
	debug1(DEBUG_UPDATE, "Scheduling %s to be updated", node_get_title(node));

	/* Retries that might have long timeouts must be 
	   cancelled to immediately execute the user request. */
	if(node->updateRequest)
		update_request_cancel_retry(node->updateRequest);
	
	if(feed_can_be_updated(node)) {
		ui_mainwindow_set_status_bar(_("Updating \"%s\""), node_get_title(node));
		request = update_request_new(node);
		request->user_data = node;
		request->callback = feed_process_update_result;
		request->options = feed->updateOptions;
		feed_prepare_request(feed, request, flags);
		node->updateRequest = request;
		update_execute_request(request);
	}
}

static void feed_request_update(nodePtr node, guint flags) {

	feed_schedule_update(node, flags | FEED_REQ_PRIORITY_HIGH);
}

static void feed_request_auto_update(nodePtr node) {
	feedPtr		feed = (feedPtr)node->data;
	GTimeVal	now;
	gint		interval;
	guint		flags = 0;

	g_get_current_time(&now);
	interval = feed_get_update_interval(feed);
	
	if(-2 >= interval)
		return;		/* don't update this feed */
		
	if(-1 == interval)
		interval = getNumericConfValue(DEFAULT_UPDATE_INTERVAL);
	
	if(getBooleanConfValue(ENABLE_FETCH_RETRIES))
		flags |= FEED_REQ_ALLOW_RETRIES;

	if(interval > 0)
		if(feed->updateState->lastPoll.tv_sec + interval*60 <= now.tv_sec)
			feed_schedule_update(node, flags);

	/* And check for favicon updating */
	if(feed->updateState->lastFaviconPoll.tv_sec + 30*24*60*60 <= now.tv_sec)
		feed_update_favicon(node);
}

static void feed_remove(nodePtr node) {

	favicon_remove_from_cache(node->id);
	notification_node_removed(node);
	ui_node_remove_node(node);
	
	feed_remove_from_cache(node);
}

static void feed_mark_all_read(nodePtr node) {

	itemlist_mark_all_read(node->itemSet);
}

static gchar * feed_render(nodePtr node) {
	gchar		**params = NULL, *output = NULL;
	xmlDocPtr	doc;

	doc = feed_to_xml(node, NULL, TRUE);
	params = render_add_parameter(params, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");
	output = render_xml(doc, "feed", params);
	xmlFree(doc);

	return output;
}

static struct nodeType nti = {
	0,
	"feed",	/* not used, feed format ids are used instead */
	NODE_TYPE_FEED,
	feed_import,
	feed_export,
	feed_initial_load,
	feed_load,
	feed_save,
	feed_unload,
	feed_reset_update_counter,
	feed_request_update,
	feed_request_auto_update,
	feed_remove,
	feed_mark_all_read,
	feed_render,
	ui_feed_add,
	ui_feed_properties
};

nodeTypePtr feed_get_node_type(void) { return &nti; }
