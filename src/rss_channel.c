/*
   some tolerant and generic RSS/RDF channel parsing
      
   Note: portions of the original parser code were inspired by
   the feed reader software Rol which is copyrighted by
   
   Copyright (C) 2002 Jonathan Gordon <eru@unknown-days.com>
   
   The major part of this backend/parsing/storing code written by
   
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

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

#include <sys/time.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "conf.h"
#include "support.h"
#include "common.h"
#include "rss_channel.h"
#include "callbacks.h"

#include "rss_ns.h"
#include "ns_dc.h"
#include "ns_fm.h"
#include "ns_content.h"
#include "ns_slash.h"
#include "ns_syn.h"
#include "ns_admin.h"
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

/* to store the RSSNsHandler structs for all supported RDF namespace handlers */
GHashTable	*rss_nstable = NULL;	/* duplicate storage: for quick finding... */
GSList		*rss_nslist = NULL;	/*                    for processing order... */

/* note: the tag order has to correspond with the RSS_CHANNEL_* defines in the header file */
static gchar *channelTagList[] = {	"title",
					"description",
					"link",
					"image",
					"copyright",
					"language",
					"lastBuildDate",
					"pubDate",
					"webMaster",
					"managingEditor",
					"category",
					NULL
				  };

/* ---------------------------------------------------------------------------- */
/* HTML output 		 							*/
/* ---------------------------------------------------------------------------- */

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
			g_warning(_("Internal error! Invalid output request mode for namespace information!"));
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

/* writes RSS channel description as HTML into the gtkhtml widget */
static gchar * showRSSFeedInfo(RSSChannelPtr cp, gchar *url) {
	gchar		*buffer = NULL;
	gchar		*tmp;
	outputRequest	request;

	g_assert(cp != NULL);	

	addToHTMLBuffer(&buffer, FEED_HEAD_START);
	addToHTMLBuffer(&buffer, FEED_HEAD_CHANNEL);
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>",
		cp->tags[RSS_CHANNEL_LINK],
		cp->tags[RSS_CHANNEL_TITLE]);
	addToHTMLBuffer(&buffer, tmp);
	g_free(tmp);
	
	addToHTMLBuffer(&buffer, HTML_NEWLINE);	

	addToHTMLBuffer(&buffer, FEED_HEAD_SOURCE);
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>", url, url);
	addToHTMLBuffer(&buffer, tmp);
	g_free(tmp);

	addToHTMLBuffer(&buffer, FEED_HEAD_END);	
		
	/* process namespace infos */
	request.obj = (gpointer)cp;
	request.type = OUTPUT_RSS_CHANNEL_NS_HEADER;
	request.buffer = &buffer;
	if(NULL != rss_nslist)
		g_slist_foreach(rss_nslist, showRSSFeedNSInfo, (gpointer)&request);

	if(NULL != cp->tags[RSS_CHANNEL_IMAGE]) {
		addToHTMLBuffer(&buffer, IMG_START);
		addToHTMLBuffer(&buffer, cp->tags[RSS_CHANNEL_IMAGE] );
		addToHTMLBuffer(&buffer, IMG_END);	
	}

	if(NULL != cp->tags[RSS_CHANNEL_DESCRIPTION])
		addToHTMLBuffer(&buffer, cp->tags[RSS_CHANNEL_DESCRIPTION]);

	/* if available output text[iI]nput formular */
	if((NULL != cp->tiLink) && (NULL != cp->tiName) && 
	   (NULL != cp->tiDescription) && (NULL != cp->tiTitle)) {
	   
		addToHTMLBuffer(&buffer, "<br><br>");
		addToHTMLBuffer(&buffer, cp->tiDescription);
		addToHTMLBuffer(&buffer, TEXT_INPUT_FORM_START);
		addToHTMLBuffer(&buffer, cp->tiLink);
		addToHTMLBuffer(&buffer, TEXT_INPUT_TEXT_FIELD);
		addToHTMLBuffer(&buffer, cp->tiName);
		addToHTMLBuffer(&buffer, TEXT_INPUT_SUBMIT);
		addToHTMLBuffer(&buffer, cp->tiTitle);
		addToHTMLBuffer(&buffer, TEXT_INPUT_FORM_END);
	}

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

/* ---------------------------------------------------------------------------- */
/* RSS parsing		 							*/
/* ---------------------------------------------------------------------------- */

/* method to parse standard tags for the channel element */
static void parseChannel(RSSChannelPtr cp, xmlNodePtr cur) {
	gchar			*tmp;
	parseChannelTagFunc	fp;
	GSList			*hp;
	RSSNsHandler		*nsh;
	int			i;
	
	g_assert(NULL != cur);
			
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if(!xmlStrcmp(cur->name, BAD_CAST"ttl")) {
 			tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
 			if(NULL != tmp) {
				cp->updateInterval = atoi(tmp);
				g_free(tmp);
			}
		}
	
		/* check namespace of this tag */
		if(NULL != cur->ns) {
			if(NULL != cur->ns->prefix) {
				g_assert(NULL != rss_nslist);
				if(NULL != (hp = (GSList *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix))) {
					nsh = (RSSNsHandler *)hp->data;
					fp = nsh->parseChannelTag;
					if(NULL != fp)
						(*fp)(cp, cur);
					cur = cur->next;
					continue;
				} else {
					//g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);
				}
			}
		}
		/* check for RDF tags */
		for(i = 0; i < RSS_CHANNEL_MAX_TAG; i++) {
			if(!xmlStrcmp(cur->name, BAD_CAST channelTagList[i])) {
				tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					g_free(cp->tags[i]);
					cp->tags[i] = tmp;
					break;
				}
			}		
		}
		cur = cur->next;
	}

	/* some postprocessing */
	if(NULL != cp->tags[RSS_CHANNEL_TITLE])
		cp->tags[RSS_CHANNEL_TITLE] = unhtmlize(cp->tags[RSS_CHANNEL_TITLE]);

	if(NULL != cp->tags[RSS_CHANNEL_DESCRIPTION])
		cp->tags[RSS_CHANNEL_DESCRIPTION] = convertToHTML(cp->tags[RSS_CHANNEL_DESCRIPTION]);
}

static void parseTextInput(RSSChannelPtr cp, xmlNodePtr cur) {
	gchar	*tmp;
	
	g_assert(NULL != cur);
	g_assert(NULL != cp);
	
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
 		tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		if(NULL != tmp) {
			if(!xmlStrcmp(cur->name, BAD_CAST"title"))
				cp->tiTitle = tmp;
			else if(!xmlStrcmp(cur->name, BAD_CAST"description"))
				cp->tiDescription = tmp;
			else if(!xmlStrcmp(cur->name, BAD_CAST"name"))
				cp->tiName = tmp;
			else if(!xmlStrcmp(cur->name, BAD_CAST"link"))
				cp->tiLink = tmp;
			else
				g_free(tmp);
		}
		cur = cur->next;
	}
	
	/* some postprocessing */
	if(NULL != cp->tiTitle)
		cp->tiTitle = unhtmlize(cp->tiTitle);
		
	if(NULL != cp->tiDescription)
		cp->tiDescription = unhtmlize(cp->tiDescription);
}

static void parseImage(RSSChannelPtr cp, xmlNodePtr cur) {
	gchar	*tmp;
	
	g_assert(NULL != cur);	
	g_assert(NULL != cp);		

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
 		if (!xmlStrcmp(cur->name, BAD_CAST"url")) {
 			tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp) {
				g_free(cp->tags[RSS_CHANNEL_IMAGE]);
				cp->tags[RSS_CHANNEL_IMAGE] = tmp;
 			}
 		}		
		cur = cur->next;
	}
}

/* reads a RSS feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void readRSSFeed(feedPtr fp, gchar *data) {
	xmlDocPtr 		doc;
	xmlNodePtr 		cur;
	itemPtr 		ip;
	RSSChannelPtr 		cp;
	short 			rdf = 0;
	int 			error = 0;
	int			i;
	
	/* initialize channel structure */
	cp = g_new0(struct RSSChannel, 1);
	cp->nsinfos = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	cp->updateInterval = -1;

	while(1) {
		if(NULL == (doc = parseBuffer(data, &(fp->parseErrors)))) {
			addToHTMLBuffer(&(fp->parseErrors), g_strdup_printf(_("<p>XML error while reading feed! Feed \"%s\" could not be loaded!</p>"), fp->source));
			error = 1;
			break;
		}

		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Empty document!</p>"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		if(!xmlStrcmp(cur->name, BAD_CAST"rss")) {
			rdf = 0;
		} else if(!xmlStrcmp(cur->name, BAD_CAST"rdf") || 
                	  !xmlStrcmp(cur->name, BAD_CAST"RDF")) {
			rdf = 1;
		} else {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not find RDF/RSS header!</p>"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		cur = cur->xmlChildrenNode;
		while(cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
	
		while (cur != NULL) {
			if ((!xmlStrcmp(cur->name, BAD_CAST"channel"))) {
				parseChannel(cp, cur);
				g_assert(NULL != cur);
				if(0 == rdf)
					cur = cur->xmlChildrenNode;
				break;
			}
			g_assert(NULL != cur);
			cur = cur->next;
		}

		time(&(cp->time));

		/* parse channel contents */
		while(cur != NULL) {
			/* save link to channel image */
			if((!xmlStrcmp(cur->name, BAD_CAST"image"))) {
				parseImage(cp, cur);
			}

			/* no matter if we parse Userland or Netscape, there should be
			   only one text[iI]nput per channel and parsing the rdf:ressource
			   one should not harm */
			if((!xmlStrcmp(cur->name, BAD_CAST"textinput")) ||
			   (!xmlStrcmp(cur->name, BAD_CAST"textInput"))) {
				parseTextInput(cp, cur);
			}
			
			/* collect channel items */
			if((!xmlStrcmp(cur->name, BAD_CAST"item"))) {
				if(NULL != (ip = parseRSSItem(fp, cp, cur))) {
					if(0 == ip->time)
						ip->time = cp->time;
					feed_add_item(fp, ip);
				}
			}
			cur = cur->next;
		}
		xmlFreeDoc(doc);
		break;
	}

	/* after parsing we fill in the infos into the feedPtr structure */		
	fp->defaultInterval = cp->updateInterval;
	feed_set_update_interval(fp,cp->updateInterval);
	fp->title = g_strdup(cp->tags[RSS_CHANNEL_TITLE]);

	if(0 == error) {
		fp->available = TRUE;
		fp->description = showRSSFeedInfo(cp, fp->source);
	} else {
		ui_mainwindow_set_status_bar(_("There were errors while parsing this feed!"));
	}

	for(i = 0; i < RSS_CHANNEL_MAX_TAG; i++) {
 		g_free(cp->tags[i]);
 	}

	g_free(cp->tiTitle);
 	g_free(cp->tiDescription);
 	g_free(cp->tiName);
 	g_free(cp->tiLink);
	g_free(cp);
}

/* ---------------------------------------------------------------------------- */
/* initialization		 						*/
/* ---------------------------------------------------------------------------- */

static void addNameSpaceHandler(gchar *prefix, gpointer handler) {

	g_assert(NULL != rss_nstable);
	if(getNameSpaceStatus(prefix)) {
		rss_nslist = g_slist_append(rss_nslist, handler);
		g_hash_table_insert(rss_nstable, (gpointer)prefix, g_slist_last(rss_nslist));
	}
}

feedHandlerPtr initRSSFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);

	/* because initRSSFeedHandler() is called twice, once for FST_RSS and again for FST_HELPFEED */	
	if(NULL == rss_nstable) {
		rss_nstable = g_hash_table_new(g_str_hash, g_str_equal);
	
		/* register RSS name space handlers */
		addNameSpaceHandler(ns_bC_getRSSNsPrefix(), (gpointer)ns_bC_getRSSNsHandler());
		addNameSpaceHandler(ns_dc_getRSSNsPrefix(), (gpointer)ns_dc_getRSSNsHandler());
		addNameSpaceHandler(ns_fm_getRSSNsPrefix(), (gpointer)ns_fm_getRSSNsHandler());	
  		addNameSpaceHandler(ns_slash_getRSSNsPrefix(), (gpointer)ns_slash_getRSSNsHandler());
		addNameSpaceHandler(ns_content_getRSSNsPrefix(), (gpointer)ns_content_getRSSNsHandler());
		addNameSpaceHandler(ns_syn_getRSSNsPrefix(), (gpointer)ns_syn_getRSSNsHandler());
		addNameSpaceHandler(ns_admin_getRSSNsPrefix(), (gpointer)ns_admin_getRSSNsHandler());
		addNameSpaceHandler(ns_cC_getRSSNsPrefix(), (gpointer)ns_cC_getRSSNsHandler());
	}
							
	/* prepare feed handler structure */
	fhp->readFeed		= readRSSFeed;
	fhp->merge		= TRUE;
	
	return fhp;
}
