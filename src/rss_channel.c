/**
 * @file rss_channel.c some tolerant and generic RSS/RDF channel parsing
 *
 * Note: portions of the original parser code were inspired by
 * the feed reader software Rol which is copyrighted by
 * 
 * Copyright (C) 2002 Jonathan Gordon <eru@unknown-days.com>
 * 
 * The major part of this parsing code written by
 * 
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include <sys/time.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "conf.h"
#include "common.h"
#include "rss_channel.h"
#include "callbacks.h"
#include "feed.h"
#include "metadata.h"
#include "rss_ns.h"
#include "ns_dc.h"
#include "ns_fm.h"
#include "ns_content.h"
#include "ns_slash.h"
#include "ns_syn.h"
#include "ns_admin.h"
#include "ns_ag.h"
#include "ns_blogChannel.h"
#include "ns_cC.h"
#include "htmlview.h"

/* HTML output strings */

#define TEXT_INPUT_FORM_START	"<form class=\"rssform\" method=\"GET\" ACTION=\""
#define TEXT_INPUT_TEXT_FIELD	"\"><input class=\"rssformtext\" type=text value=\"\" name=\""
#define TEXT_INPUT_SUBMIT	"\"><input class=\"rssformsubmit\" type=submit value=\""
#define TEXT_INPUT_FORM_END	"\"></form>"

/* structure for the hashtable callback which itself calls the 
   namespace output handler */
#define OUTPUT_RSS_CHANNEL_NS_HEADER	0
#define	OUTPUT_RSS_CHANNEL_NS_FOOTER	1
#define OUTPUT_ITEM_NS_HEADER		2
#define OUTPUT_ITEM_NS_FOOTER		3
typedef struct {
	gint		type;
	gchar		**buffer;	/* pointer to output char buffer pointer */
	gpointer	obj;		/* thats either a RSSChannelPtr or a RSSItemPtr 
					   depending on the type value */
} outputRequest;

GHashTable *channelHash = NULL;

/* to store the RSSNsHandler structs for all supported RDF namespace handlers */
GHashTable	*rss_nstable = NULL;	/* duplicate storage: for quick finding... */
GSList		*rss_nslist = NULL;	/*                    for processing order... */

/* method called by g_slist_foreach for thee HTML
   generation functions to output namespace specific infos 
   not static because its reused by rss_item.c */

void showRSSFeedNSInfo(gpointer value, gpointer userdata) {
	outputRequest	*request = (outputRequest *)userdata;
	RSSNsHandler	*nsh = (RSSNsHandler *)value;
	outputFunc	fp;
	gchar		*tmp;

	switch(request->type) {
		case OUTPUT_RSS_CHANNEL_NS_HEADER:
			fp = nsh->doChannelHeaderOutput;
			break;
		case OUTPUT_RSS_CHANNEL_NS_FOOTER:
			fp = nsh->doChannelFooterOutput;
			break;
		case OUTPUT_ITEM_NS_HEADER:
			fp = nsh->doItemHeaderOutput;
			break;		
		case OUTPUT_ITEM_NS_FOOTER:
			fp = nsh->doItemFooterOutput;
			break;			
		default:	
			g_warning("Internal error! Invalid output request mode for namespace information!");
			return;
			break;	
	}
	
	if(NULL == fp)
		return;
		
	if(NULL == (tmp = (*fp)(request->obj)))
		return;
		
	addToHTMLBuffer(request->buffer, tmp);
	g_free(tmp);
}

/* returns RSS channel description as HTML */
static gchar * showRSSFeedInfo(feedPtr fp, RSSChannelPtr cp) {
	gchar		*buffer = NULL;
	outputRequest	request;

	g_assert(cp != NULL);

	/* process namespace infos */
	request.obj = (gpointer)cp;
	request.type = OUTPUT_RSS_CHANNEL_NS_HEADER;
	request.buffer = &buffer;
	if(NULL != rss_nslist)
		g_slist_foreach(rss_nslist, showRSSFeedNSInfo, (gpointer)&request);

	if(NULL != cp->tags[RSS_CHANNEL_DESCRIPTION])
		addToHTMLBuffer(&buffer, cp->tags[RSS_CHANNEL_DESCRIPTION]);

	/* if available output text[iI]nput formular */
	/* process namespace infos */
	request.type = OUTPUT_RSS_CHANNEL_NS_FOOTER;
	if(NULL != rss_nslist)
		g_slist_foreach(rss_nslist, showRSSFeedNSInfo, (gpointer)&request);

	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(buffer, "language",		cp->tags[RSS_CHANNEL_LANGUAGE]);
	FEED_FOOT_WRITE(buffer, "copyright",		cp->tags[RSS_CHANNEL_COPYRIGHT]);
	FEED_FOOT_WRITE(buffer, "last build date",	cp->tags[RSS_CHANNEL_LASTBUILDDATE]);
	FEED_FOOT_WRITE(buffer, "publication date",	cp->tags[RSS_CHANNEL_PUBDATE]);
	FEED_FOOT_WRITE(buffer, "webmaster",		cp->tags[RSS_CHANNEL_WEBMASTER]);
	FEED_FOOT_WRITE(buffer, "managing editor",	cp->tags[RSS_CHANNEL_MANAGINGEDITOR]);
	FEED_FOOT_WRITE(buffer, "category",		cp->tags[RSS_CHANNEL_CATEGORY]);
	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_END);
		
	return buffer;
}

/* This function parses the metadata for the channel. This does not
   parse the items. The items are parsed elsewhere. */
static void parseChannel(feedPtr fp, RSSChannelPtr cp, xmlNodePtr cur) {
	gchar			*tmp, *tmp2, *tmp3;
	GSList			*hp;
	
	g_assert(NULL != cur);
			
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* check namespace of this tag */
		if (cur->type != XML_ELEMENT_NODE || cur->name == NULL)
			goto next;
		
		if(NULL != cur->ns) {
			if(NULL != cur->ns->prefix) {
				g_assert(NULL != rss_nslist);
				if(NULL != (hp = (GSList *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix))) {
					NsHandler		*nsh = (NsHandler *)hp->data;
					parseChannelTagFunc	pf = nsh->parseChannelTag;
					if(NULL != pf)
						(*pf)(fp, cur);
					goto next;
				} else {
					/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
				}
			}
		}
		/* Check for metadata tags */
		if ((tmp2 = g_hash_table_lookup(channelHash, cur->name)) != NULL) {
			tmp3 = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE));
			if (tmp3 != NULL) {
				fp->metadata = metadata_list_append(fp->metadata, tmp2, tmp3);
				g_free(tmp3);
				goto next;
			}
		}

		
		/* check for specific tags */
		if(!xmlStrcmp(cur->name, BAD_CAST"ttl")) {
 			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE));
 			if(NULL != tmp) {
				feed_set_default_update_interval(fp, atoi(tmp));
				feed_set_update_interval(fp, atoi(tmp));
				g_free(tmp);
				goto next;
			}
		}

		if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
 			tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)));
 			if(NULL != tmp) {
				feed_set_title(fp, tmp);
				g_free(tmp);
				goto next;
			}
		}

		if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
 			tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)));
 			if(NULL != tmp) {
				feed_set_html_uri(fp, tmp);
				g_free(tmp);
				goto next;
			}
		}

		if(!xmlStrcmp(cur->name, BAD_CAST"description")) {
 			tmp = convertToHTML(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)));
 			if(NULL != tmp) {
				feed_set_description(fp, tmp);
				g_free(tmp);
				goto next;
			}
		}
	next:
		cur = cur->next;
	}
}

static gchar* parseTextInput(xmlNodePtr cur) {
	gchar	*tmp, *buffer = NULL, *tiLink = NULL, *tiName = NULL, *tiDescription = NULL, *tiTitle = NULL;
	
	g_assert(NULL != cur);

	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if (cur->type == XML_ELEMENT_NODE) {
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp) {
				if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
					g_free(tiTitle);
					tiTitle = tmp;
				} else if(!xmlStrcmp(cur->name, BAD_CAST"description")) {
					g_free(tiDescription);
					tiDescription = tmp;
				} else if(!xmlStrcmp(cur->name, BAD_CAST"name")) {
					g_free(tiName);
					tiName = tmp;
				} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
					g_free(tiLink);
					tiLink = tmp;
				} else
					g_free(tmp);
			}
		}
		cur = cur->next;
	}
	
	/* some postprocessing */
	tiTitle = unhtmlize(tiTitle);
	tiDescription = unhtmlize(tiDescription);

	if((NULL != tiLink) && (NULL != tiName) && 
	   (NULL != tiDescription) && (NULL != tiTitle)) {
		addToHTMLBufferFast(&buffer, "<p>");
		addToHTMLBufferFast(&buffer, tiDescription);
		addToHTMLBufferFast(&buffer, TEXT_INPUT_FORM_START);
		addToHTMLBufferFast(&buffer, tiLink);
		addToHTMLBufferFast(&buffer, TEXT_INPUT_TEXT_FIELD);
		addToHTMLBufferFast(&buffer, tiName);
		addToHTMLBufferFast(&buffer, TEXT_INPUT_SUBMIT);
		addToHTMLBufferFast(&buffer, tiTitle);
		addToHTMLBufferFast(&buffer, TEXT_INPUT_FORM_END);
		addToHTMLBufferFast(&buffer, "</p>");
	}
	return buffer;
}

static gchar* parseImage(RSSChannelPtr cp, xmlNodePtr cur) {
	gchar	*tmp;
	g_assert(NULL != cur);	
	g_assert(NULL != cp);		

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (cur->type == XML_ELEMENT_NODE) {
			if (!xmlStrcmp(cur->name, BAD_CAST"url")) {
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					return tmp;
				}
			}
		}
		cur = cur->next;
	}
	return NULL;
}

/* reads a RSS feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void rss_parse(feedPtr fp, xmlDocPtr doc, xmlNodePtr cur) {
	itemPtr 		ip;
	RSSChannelPtr 		cp;
	short 			rdf = 0;
	int 			error = 0;
	int			i;
	
	/* initialize channel structure */
	cp = g_new0(struct RSSChannel, 1);
	cp->nsinfos = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);

	if(!xmlStrcmp(cur->name, BAD_CAST"rss")) {
		rdf = 0;
	} else if(!xmlStrcmp(cur->name, BAD_CAST"rdf") || 
			!xmlStrcmp(cur->name, BAD_CAST"RDF")) {
		rdf = 1;
	} else {
		addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not find RDF/RSS header!</p>"));
		xmlFreeDoc(doc);
		error = 1;
		goto end;
	}
	
	cur = cur->xmlChildrenNode;
	while(cur && xmlIsBlankNode(cur)) {
		cur = cur->next;
	}
	
	while(cur != NULL) {
		if(NULL == cur->name) {
			g_warning("invalid XML: parser returns NULL value -> tag ignored!");
			cur = cur->next;
			continue;
		}
		
		if((!xmlStrcmp(cur->name, BAD_CAST"channel"))) {
			parseChannel(fp, cp, cur);
			g_assert(NULL != cur);
			if(0 == rdf)
				cur = cur->xmlChildrenNode;
			break;
		}
		cur = cur->next;
	}
	
	time(&(cp->time));

	/* For RDF (rss 0.9 or 1.0), cur now points to the item after the channel tag. */
	/* For RSS, cur now points to the first item inside of the channel tag */
	/* This ends up being the thing with the items, (and images/textinputs for RDF) */

	/* parse channel contents */
	while(cur != NULL) {
		if (cur->type != XML_ELEMENT_NODE) {
			cur = cur->next;
			continue;
		}
		
		if(NULL == cur->name) {
			g_warning("invalid feed: parser returns NULL cur->name. Offending tag ignored.");
			cur = cur->next;
			continue;
		}

		/* save link to channel image */
		if((!xmlStrcmp(cur->name, BAD_CAST"image"))) {
			gchar *tmp = parseImage(cp, cur);
			fp->metadata = metadata_list_append(fp->metadata, "feedLogoUri", tmp);
			g_free(tmp);
		}
		
		/* no matter if we parse Userland or Netscape, there should be
		   only one text[iI]nput per channel and parsing the rdf:ressource
		   one should not harm */
		if((!xmlStrcmp(cur->name, BAD_CAST"textinput")) ||
		   (!xmlStrcmp(cur->name, BAD_CAST"textInput"))) {
			gchar *tmp = parseTextInput(cur);
			
			if(tmp != NULL)
				fp->metadata = metadata_list_append(fp->metadata, "textInput", tmp);
			g_free(tmp);
		}

		/* collect channel items */
		if((!xmlStrcmp(cur->name, BAD_CAST"item"))) {
			if(NULL != (ip = parseRSSItem(fp, cp, cur))) {
				if(0 == item_get_time(ip))
					item_set_time(ip, cp->time);
				feed_add_item(fp, ip);
			}
		}
		cur = cur->next;
	}
	
 end:
	/* after parsing we fill in the infos into the feedPtr structure */		
	
	if(0 == error) {
		fp->available = TRUE;
		//fp->description = showRSSFeedInfo(fp, cp);
	} else {
		ui_mainwindow_set_status_bar(_("There were errors while parsing this feed!"));
	}
	
	for(i = 0; i < RSS_CHANNEL_MAX_TAG; i++) {
 		g_free(cp->tags[i]);
 	}
	
	g_hash_table_destroy(cp->nsinfos);
	g_free(cp->tiTitle);
 	g_free(cp->tiDescription);
 	g_free(cp->tiName);
 	g_free(cp->tiLink);
	g_free(cp);
}

static gboolean rss_format_check(xmlDocPtr doc, xmlNodePtr cur) {
	if(!xmlStrcmp(cur->name, BAD_CAST"rss") ||
	   !xmlStrcmp(cur->name, BAD_CAST"rdf") || 
	   !xmlStrcmp(cur->name, BAD_CAST"RDF")) {
		return TRUE;
	}
	return FALSE;
}

static void rss_add_ns_handler(NsHandler *handler) {

	g_assert(NULL != rss_nstable);
	if(getNameSpaceStatus(handler->prefix)) {
		rss_nslist = g_slist_append(rss_nslist, handler);
		g_hash_table_insert(rss_nstable, handler->prefix, g_slist_last(rss_nslist));
	}
}

feedHandlerPtr initRSSFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);
	
	if (channelHash == NULL) {
		channelHash = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(channelHash, "copyright", "copyright");
		g_hash_table_insert(channelHash, "category", "category");
		g_hash_table_insert(channelHash, "webMaster", "webmaster");
		g_hash_table_insert(channelHash, "language", "language");
		g_hash_table_insert(channelHash, "managingEditor", "managingEditor");
		g_hash_table_insert(channelHash, "pubDate", "contentUpdateDate");
		g_hash_table_insert(channelHash, "lastBuildDate", "feedUpdateDate");
		g_hash_table_insert(channelHash, "generator", "feedgenerator");
		
		g_hash_table_insert(channelHash, "publisher", "webmaster");
	}
	
	/* because initRSSFeedHandler() is called twice, once for FST_RSS and again for FST_HELPFEED */	
	if(NULL == rss_nstable) {
		rss_nstable = g_hash_table_new(g_str_hash, g_str_equal);
	
		/* register RSS name space handlers */
		//addNameSpaceHandler(ns_bC_getRSSNsHandler());
		//addNameSpaceHandler(ns_dc_getRSSNsHandler());
		//addNameSpaceHandler(ns_fm_getRSSNsHandler());	
  		//addNameSpaceHandler(ns_slash_getRSSNsHandler());
		//addNameSpaceHandler(ns_content_getRSSNsHandler());
		//addNameSpaceHandler(ns_syn_getRSSNsHandler());
		rss_add_ns_handler(ns_admin_getRSSNsHandler());
		rss_add_ns_handler(ns_ag_getRSSNsHandler());
		//addNameSpaceHandler(ns_cC1_getRSSNsHandler());
		//addNameSpaceHandler(ns_cC2_getRSSNsHandler());
	}
							
	/* prepare feed handler structure */
	fhp->typeStr = "rss";
	fhp->icon = ICON_AVAILABLE;
	fhp->directory = FALSE;
	fhp->feedParser	= rss_parse;
	fhp->checkFormat = rss_format_check;
	fhp->merge = TRUE;
	
	return fhp;
}
