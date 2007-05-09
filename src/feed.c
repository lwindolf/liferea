/**
 * @file feed.c common feed handling
 * 
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include "conf.h"
#include "common.h"
#include "db.h"
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
#include "parsers/cdf_channel.h"
#include "parsers/rss_channel.h"
#include "parsers/atom10.h"
#include "parsers/pie_feed.h"
#include "ui/ui_auth.h"
#include "ui/ui_subscription.h"
#include "ui/ui_enclosure.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"
#include "notification/notif_plugin.h"
#include "scripting/script.h"

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

/* function to create a new feed structure */
feedPtr feed_new(void) {
	feedPtr		feed;
	
	feed = g_new0(struct feed, 1);

	feed->cacheLimit = CACHE_DEFAULT;
	feed->valid = TRUE;

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
		
		feed = feed_new();
		feed->fhp = feed_type_str_to_fhp(typeStr);
		xmlFree(typeStr);
		
		node_set_data(node, feed);
		node_set_subscription(node, subscription_new(NULL, NULL, NULL));

		if(!trusted && source[0] == '|') {
			/* FIXME: Display warning dialog asking if the command
			   is safe? */
			tmp = g_strdup_printf("unsafe command: %s", source);
			xmlFree(source);
			source = tmp;
		}

		subscription_set_source(node->subscription, source);
		xmlFree(source);

		if((filter = xmlGetProp(cur, BAD_CAST"filtercmd"))) {
			if(!trusted) {
				/* FIXME: Display warning dialog asking if the command
				   is safe? */
				tmp = g_strdup_printf("unsafe command: %s", filter);
				xmlFree(filter);
				filter = tmp;
			}

			subscription_set_filter(node->subscription, filter);
			xmlFree(filter);
		}
		
		intervalStr = xmlGetProp(cur, BAD_CAST"updateInterval");
		subscription_set_update_interval(node->subscription, parse_integer(intervalStr, -1));
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
			feed_set_html_url(feed, "", htmlUrlStr);
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
			node->subscription->updateOptions->dontUseProxy = TRUE;
		xmlFree(tmp);
		
		update_state_import(cur, node->subscription->updateState);
						
		node_set_icon(node, favicon_load_from_cache(node->id));
		
		debug4(DEBUG_CACHE, "import feed: title=%s source=%s typeStr=%s interval=%d", 
		       node_get_title(node), 
		       subscription_get_source(node->subscription), 
		       typeStr, 
		       subscription_get_update_interval(node->subscription));

		node_add_child(parent, node, -1);
	}

	debug_exit("feed_import");
}

static void feed_export(nodePtr node, xmlNodePtr cur, gboolean trusted) {
	feedPtr feed = (feedPtr)node->data;

	debug_enter("feed_export");

	gchar *interval = g_strdup_printf("%d", subscription_get_update_interval (node->subscription));
	gchar *cacheLimit = NULL;

	if(feed_get_html_url(feed))
		xmlNewProp(cur, BAD_CAST"htmlUrl", BAD_CAST feed_get_html_url(feed));
	else
		xmlNewProp(cur, BAD_CAST"htmlUrl", BAD_CAST "");
	xmlNewProp(cur, BAD_CAST"xmlUrl", BAD_CAST subscription_get_source (node->subscription));

	if(subscription_get_filter (node->subscription))
		xmlNewProp(cur, BAD_CAST"filtercmd", BAD_CAST subscription_get_filter (node->subscription));

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
			
		if(TRUE == node->subscription->updateOptions->dontUseProxy)
			xmlNewProp(cur, BAD_CAST"dontUseProxy", BAD_CAST"true");
	}

	debug3(DEBUG_CACHE, "adding feed: source=%s interval=%s cacheLimit=%s",
	       subscription_get_source (node->subscription), 
	       interval, (cacheLimit != NULL ? cacheLimit : ""));
	g_free(cacheLimit);
	g_free(interval);

	debug_exit("feed_export");
}

feedParserCtxtPtr feed_create_parser_ctxt(void) {
	feedParserCtxtPtr ctxt;

	ctxt = g_new0(struct feedParserCtxt, 1);
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
 * This function tries to find a feed link for a given HTTP URI. It
 * tries to download it. If it finds a valid feed source it parses
 * this source instead into the given feed parsing context. It also
 * replaces the HTTP URI with the found feed source.
 *
 * @param ctxt		feed parsing context
 */
static void
feed_auto_discover (feedParserCtxtPtr ctxt)
{
	gchar	*source;
	
	if(ctxt->feed->parseErrors)
		g_string_truncate(ctxt->feed->parseErrors, 0);
	else
		ctxt->feed->parseErrors = g_string_new(NULL);
		
	debug1(DEBUG_UPDATE, "Starting feed auto discovery (%s)", subscription_get_source (ctxt->subscription));
	if((source = html_auto_discover_feed (ctxt->data, subscription_get_source (ctxt->subscription)))) {
		/* now download the first feed link found */
		requestPtr request = update_request_new(0);
		debug1(DEBUG_UPDATE, "feed link found: %s", source);
		request->source = g_strdup(source);
		request->options = ctxt->subscription->updateOptions;
		update_execute_request_sync(request);
		if(request->data) {
			debug0(DEBUG_UPDATE, "feed link download successful!");
			subscription_set_source(ctxt->subscription, source);
			ctxt->data = request->data;
			ctxt->dataLength = request->size;
			ctxt->failed = FALSE;
			feed_parse(ctxt);
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
		g_string_append(ctxt->feed->parseErrors, _("The URL you want Liferea to subscribe to points to a webpage and the auto discovery found no feeds on this page. Maybe this webpage just does not support feed auto discovery."));
	}
}

/**
 * General feed source parsing function. Parses the passed feed source
 * and tries to determine the source type. 
 *
 * @param ctxt		feed parsing context
 *
 * @returns FALSE if auto discovery is indicated, 
 *          TRUE if feed type was recognized and parsing was successful
 */
gboolean feed_parse(feedParserCtxtPtr ctxt) {
	xmlNodePtr	cur;
	gboolean	success = FALSE;

	debug_enter("feed_parse");

	g_assert(NULL == ctxt->items);
	
	ctxt->failed = TRUE;	/* reset on success ... */

	if(ctxt->feed->parseErrors)
		g_string_truncate(ctxt->feed->parseErrors, 0);
	else
		ctxt->feed->parseErrors = g_string_new(NULL);

	/* try to parse buffer with XML and to create a DOM tree */	
	do {
		if(NULL == common_parse_xml_feed(ctxt)) {
			g_string_append_printf(ctxt->feed->parseErrors, _("XML error while reading feed! Feed \"%s\" could not be loaded!"), subscription_get_source (ctxt->subscription));
			break;
		}
		
		if(NULL == (cur = xmlDocGetRootElement(ctxt->doc))) {
			g_string_append(ctxt->feed->parseErrors, _("Empty document!"));
			break;
		}
		
		while(cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
		
		if(!cur->name) {
			g_string_append(ctxt->feed->parseErrors, _("Invalid XML!"));
			break;
		}
		
		if(!cur)
			break;
			
		/* determine the syndication format and start parser */
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
		if((strstr(ctxt->data, "<html>") || strstr(ctxt->data, "<HTML>") ||
		    strstr(ctxt->data, "<html ") || strstr(ctxt->data, "<HTML "))) {
			debug0(DEBUG_UPDATE, "HTML document detected!");
			g_string_append(ctxt->feed->parseErrors, _("Source points to HTML document."));
		} else {
			debug0(DEBUG_UPDATE, "neither a known feed type nor a HTML document!");
			g_string_append(ctxt->feed->parseErrors, _("Could not determine the feed type."));
		}
	} else {
		debug1(DEBUG_UPDATE, "discovered feed format: %s", feed_type_fhp_to_str(ctxt->feed->fhp));
		success = TRUE;
	}
	
	if(ctxt->doc) {
		xmlFreeDoc(ctxt->doc);
		ctxt->doc = NULL;
	}
		
	debug_exit("feed_parse");
	
	return success;
}

static void feed_add_xml_attributes(nodePtr node, xmlNodePtr feedNode) {
	feedPtr	feed = (feedPtr)node->data;
	gchar	*tmp;
	
	xmlNewTextChild(feedNode, NULL, "feedId", node_get_id(node));
	xmlNewTextChild(feedNode, NULL, "feedTitle", node_get_title(node));
	xmlNewTextChild(feedNode, NULL, "feedSource", subscription_get_source (node->subscription));
	xmlNewTextChild(feedNode, NULL, "feedOrigSource", subscription_get_orig_source (node->subscription));

	if(feed->description)
		xmlNewTextChild(feedNode, NULL, "feedDescription", feed->description);

	if(feed_get_image_url(feed))
		xmlNewTextChild(feedNode, NULL, "feedImage", feed_get_image_url(feed));

	tmp = g_strdup_printf("%d", subscription_get_default_update_interval (node->subscription));
	xmlNewTextChild(feedNode, NULL, "feedUpdateInterval", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", node->available?1:0);
	xmlNewTextChild(feedNode, NULL, "feedStatus", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", node->subscription->discontinued?1:0);
	xmlNewTextChild(feedNode, NULL, "feedDiscontinued", tmp);
	g_free(tmp);

	tmp = g_strdup_printf("file://%s", node_get_favicon_file(node));
	xmlNewTextChild(feedNode, NULL, "favicon", tmp);
	g_free(tmp);
		
	xmlNewTextChild(feedNode, NULL, "feedLink", feed_get_html_url(feed));

	if(node->subscription->updateError)
		xmlNewTextChild(feedNode, NULL, "updateError", node->subscription->updateError);
	if(node->subscription->httpError) {
		xmlNewTextChild(feedNode, NULL, "httpError", node->subscription->httpError);

		tmp = g_strdup_printf("%d", node->subscription->httpErrorCode);
		xmlNewTextChild(feedNode, NULL, "httpErrorCode", tmp);
		g_free(tmp);
	}
	if(node->subscription->filterError)
		xmlNewTextChild(feedNode, NULL, "filterError", node->subscription->filterError);
	if(feed->parseErrors && (strlen(feed->parseErrors->str) > 0))
		xmlNewTextChild(feedNode, NULL, "parseError", feed->parseErrors->str);

	metadata_add_xml_nodes(feed->metadata, feedNode);
}

xmlDocPtr feed_to_xml(nodePtr node, xmlNodePtr feedNode) {
	xmlDocPtr	doc = NULL;
	
	if(!feedNode) {
		doc = xmlNewDoc("1.0");
		feedNode = xmlDocGetRootElement(doc);
		feedNode = xmlNewDocNode(doc, NULL, "feed", NULL);
		xmlDocSetRootElement(doc, feedNode);
	}
	feed_add_xml_attributes(node, feedNode);
	
	return doc;
}

guint feed_get_max_item_count(nodePtr node) {
	feedPtr	feed = (feedPtr)node->data;
	
	switch(feed->cacheLimit) {
		case CACHE_DEFAULT:
			return getNumericConfValue(DEFAULT_MAX_ITEMS);
			break;
		case CACHE_DISABLE:
		case CACHE_UNLIMITED:
			return G_MAXUINT;
			break;
		default:
			return feed->cacheLimit;
			break;
	}
}


/* ---------------------------------------------------------------------------- */
/* feed attributes encapsulation						*/
/* ---------------------------------------------------------------------------- */

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

const gchar * feed_get_html_url(feedPtr feed) { return feed->htmlUrl; };
void feed_set_html_url(feedPtr feed, const gchar *base, const gchar *htmlUrl) {
	g_free(feed->htmlUrl);
	feed->htmlUrl = NULL;

	if(htmlUrl) {
		if(strstr(htmlUrl, "://")) {
			/* absolute URI can be used directly */
			feed->htmlUrl = g_strchomp(g_strdup(htmlUrl));
		} else {
			/* relative URI part needs to be expanded */
			gchar *tmp, *source;
			
			source = g_strdup(base);
			tmp = strrchr(source, '/');
			if(tmp)
				*(tmp+1) = '\0';

			feed->htmlUrl = common_build_url(htmlUrl, source);
			g_free(source);
		}
	}
}

const gchar * feed_get_image_url(feedPtr feed) { return feed->imageUrl; };
void feed_set_image_url(feedPtr feed, const gchar *imageUrl) {

	g_free(feed->imageUrl);
	if(imageUrl != NULL)
		feed->imageUrl = g_strchomp(g_strdup(imageUrl));
	else
		feed->imageUrl = NULL;
}

/* method to free a feed structure */
static void feed_free(feedPtr feed) {

	g_string_free(feed->parseErrors, TRUE);
	g_free(feed->htmlUrl);
	g_free(feed->imageUrl);
	g_free(feed->description);
	metadata_list_free(feed->metadata);
	g_free(feed);
}

/* implementation of feed node update request processing callback */

static void feed_process_update_result(struct request *request) {
	feedParserCtxtPtr	ctxt;
	nodePtr			node = (nodePtr)request->user_data;
	feedPtr			feed = (feedPtr)node->data;
	subscriptionPtr		subscription = (subscriptionPtr)node->subscription;
	gchar			*old_title, *old_source;
	gint			old_update_interval;

	debug_enter("feed_process_update_result");
	
	/* no matter what the result of the update is we need to save update
	   status and the last update time to cache */
	node->available = FALSE;
	
	/* note this is to update the subscription URL on permanent redirects */
	if(!strcmp(request->source, subscription_get_source(subscription))) {
		subscription_set_source(subscription, request->source);
		ui_mainwindow_set_status_bar(_("The URL of \"%s\" has changed permanently and was updated"), node_get_title(node));
	}
	
	if(401 == request->httpstatus) { /* unauthorized */
		if(request->flags & FEED_REQ_AUTH_DIALOG)
			ui_auth_dialog_new(node->subscription, request->flags);
	} else if(410 == request->httpstatus) { /* gone */
		subscription->discontinued = TRUE;
		node->available = TRUE;
		ui_mainwindow_set_status_bar(_("\"%s\" is discontinued. Liferea won't updated it anymore!"), node_get_title(node));
	} else if(304 == request->httpstatus) {
		node->available = TRUE;
		ui_mainwindow_set_status_bar(_("\"%s\" has not changed since last update"), node_get_title(node));
	} else if(NULL != request->data) {
		/* we save all properties that should not be overwritten in all cases */
		old_update_interval = subscription_get_update_interval(subscription);
		old_title = g_strdup(node_get_title(node));
		old_source = g_strdup(subscription_get_source(node->subscription));

		/* parse the new downloaded feed into feed and itemSet */
		ctxt = feed_create_parser_ctxt();
		ctxt->feed = feed;
		ctxt->data = request->data;
		ctxt->dataLength = request->size;
		ctxt->subscription = node->subscription;

		/* try to parse the feed */
		if(!feed_parse(ctxt)) {
			/* if it doesn't work and it is a new subscription
			   start feed auto discovery */
			if(request->flags & FEED_REQ_AUTO_DISCOVER)
				feed_auto_discover(ctxt);
		}
		
		if(ctxt->failed) {
			g_string_prepend(feed->parseErrors, _("<p>Could not detect the type of this feed! Please check if the source really points to a resource provided in one of the supported syndication formats!</p>"
			                                      "XML Parser Output:<br /><div class='xmlparseroutput'>"));
			g_string_append(feed->parseErrors, "</div>");
		} else {
			itemSetPtr itemSet = node_get_itemset(node);
			
			/* merge the resulting items into the node's item set */
			itemset_merge_items(itemSet, ctxt->items);
		
			/* restore user defined properties if necessary */
			if(!(request->flags & FEED_REQ_RESET_TITLE)) 
				node_set_title(node, old_title);
				
			if(!(request->flags & FEED_REQ_AUTO_DISCOVER))
				subscription_set_source(subscription, old_source);

			if(request->flags & FEED_REQ_RESET_UPDATE_INT)
				subscription_set_update_interval(subscription, subscription_get_default_update_interval(subscription));
			else
				subscription_set_update_interval(subscription, old_update_interval);

			ui_mainwindow_set_status_bar(_("\"%s\" updated..."), node_get_title(node));

			itemlist_merge_itemset(itemSet);
			itemset_free(itemSet);		
			
			node->available = TRUE;
			
			node_update_counters(node);
			notification_node_has_new_items(node);
			ui_node_update(node->id);
		}
				
		g_free(old_title);
		g_free(old_source);

		feed_free_parser_ctxt(ctxt);
	} else {	
		ui_mainwindow_set_status_bar(_("\"%s\" is not available"), node_get_title(node));
	}
	
	subscription_update_error_status(subscription, request->httpstatus, request->returncode, request->filterErrors);

	node->updateRequest = NULL; 

	if(request->flags & FEED_REQ_DOWNLOAD_FAVICON)
		subscription_update_favicon(node->subscription);

	db_update_state_save (node->id, node->subscription->updateState);
	feedlist_schedule_save ();
	
	script_run_for_hook(SCRIPT_HOOK_FEED_UPDATED);

	debug_exit("feed_process_update_result");
}

/* implementation of the node type interface */

static itemSetPtr feed_load(nodePtr node) {

	return db_itemset_load(node->id);
}

static void feed_save(nodePtr node) {

	/* Nothing to do. Feeds do not have any UI states */
}

static void feed_update_unread_count(nodePtr node) {

	node->itemCount = db_itemset_get_item_count(node->id);
	node->unreadCount = db_itemset_get_unread_count(node->id);
}

static void feed_reset_update_counter(nodePtr node) {

	subscription_reset_update_counter(node->subscription);
}

static void feed_schedule_update(nodePtr node, guint flags) {
	struct request		*request;
	
	debug1(DEBUG_UPDATE, "Scheduling %s to be updated", node_get_title(node));
	
	g_assert (NULL != node->subscription);
	
	/* Retries that might have long timeouts must be 
	   cancelled to immediately execute the user request. */
	if(node->updateRequest)
		update_request_cancel_retry(node->updateRequest);
	
	if(subscription_can_be_updated(node)) {
		ui_mainwindow_set_status_bar(_("Updating \"%s\""), node_get_title(node));
		request = update_request_new(node);
		request->user_data = node;
		request->options = node->subscription->updateOptions;
		request->callback = feed_process_update_result;
		subscription_prepare_request(node->subscription, request, flags);
		node->updateRequest = request;
		update_execute_request(request);
	}
}

static void feed_request_update(nodePtr node, guint flags) {

	feed_schedule_update(node, flags | FEED_REQ_PRIORITY_HIGH);
}

static void feed_request_auto_update(nodePtr node) {
	GTimeVal	now;
	gint		interval;
	guint		flags = 0;

	g_get_current_time(&now);
	interval = subscription_get_update_interval(node->subscription);
	
	if(-2 >= interval)
		return;		/* don't update this feed */
		
	if(-1 == interval)
		interval = getNumericConfValue(DEFAULT_UPDATE_INTERVAL);
	
	if(getBooleanConfValue(ENABLE_FETCH_RETRIES))
		flags |= FEED_REQ_ALLOW_RETRIES;

	if(interval > 0)
		if(node->subscription->updateState->lastPoll.tv_sec + interval*60 <= now.tv_sec)
			feed_schedule_update(node, flags);

	/* And check for favicon updating */
	if (favicon_update_needed (node->id, node->subscription->updateState))
		subscription_update_favicon(node->subscription);
}

static void feed_remove(nodePtr node) {

	favicon_remove_from_cache(node->id);
	notification_node_removed(node);
	ui_node_remove_node(node);
	feed_free(node->data);
}

static void feed_mark_all_read(nodePtr node) {

	itemlist_mark_all_read(node->id);
}

static gchar * feed_render(nodePtr node) {
	renderParamPtr	params;
	gchar		*output = NULL;
	xmlDocPtr	doc;

	doc = feed_to_xml(node, NULL);
	params = render_parameter_new();
	render_parameter_add(params, "pixmapsDir='file://" PACKAGE_DATA_DIR G_DIR_SEPARATOR_S PACKAGE G_DIR_SEPARATOR_S "pixmaps" G_DIR_SEPARATOR_S "'");
	output = render_xml(doc, "feed", params);
	xmlFreeDoc(doc);

	return output;
}

static void feed_add(nodePtr parentNode) {

	ui_subscription_dialog_new(parentNode);
}

static void feed_properties(nodePtr node) {

	ui_subscription_prop_dialog_new(node->subscription);
}

nodeTypePtr feed_get_node_type(void) { 

	static struct nodeType nti = {
		NODE_CAPABILITY_SHOW_UNREAD_COUNT,
		"feed",		/* not used, feed format ids are used instead */
		NULL,
		NODE_TYPE_FEED,
		feed_import,
		feed_export,
		feed_load,
		feed_save,
		feed_update_unread_count,
		feed_reset_update_counter,
		feed_request_update,
		feed_request_auto_update,
		feed_remove,
		feed_mark_all_read,
		feed_render,
		feed_add,
		feed_properties
	};
	nti.icon = icons[ICON_DEFAULT];
	
	return &nti; 
}
