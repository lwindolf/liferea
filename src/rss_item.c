/*
   RSS/RDF item parsing 
      
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

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "support.h"
#include "common.h"
#include "rss_item.h"
#include "rss_ns.h"
#include "htmlview.h"

#define RDF_NS	BAD_CAST"http://www.w3.org/1999/02/22-rdf-syntax-ns#"

#define	START_ENCLOSURE	"<div style=\"margin-top:5px;margin-bottom:5px;padding-left:5px;padding-right:5px;border-color:black;border-style:solid;border-width:1px;background-color:#E0E0E0\"> enclosed file: "
#define	END_ENCLOSURE	"</div>"

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

/* uses the same namespace handler as rss_channel */
extern GSList *rss_nslist;
extern GHashTable *rss_nstable;

static gchar *itemTagList[] = {		"title",
					"description",
					"link",
					"author",
					"comments",
					"category",
					"guid",
					NULL
				  };
				  
/* prototypes */
static gchar * showRSSItem(feedPtr fp, RSSChannelPtr cp, RSSItemPtr ip);

/* method to parse standard tags for each item element */
itemPtr parseRSSItem(feedPtr fp, RSSChannelPtr cp, xmlDocPtr doc, xmlNodePtr cur) {
	gchar			*tmp, *link;
	parseItemTagFunc	parseFunc;
	GSList			*hp;
	RSSNsHandler		*nsh;
	RSSItemPtr 		i;
	itemPtr			ip;
	int			j;

	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return NULL;
	}
		
	if(NULL == (i = (RSSItemPtr) malloc(sizeof(struct RSSItem)))) {
		g_error("not enough memory!\n");
		return(NULL);
	}
	memset(i, 0, sizeof(struct RSSItem));
	i->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);
	ip = getNewItemStruct();
	
	/* try to get an item about id */
	ip->id = xmlGetProp(cur, BAD_CAST"about");

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* check namespace of this tag */
		if(NULL != cur->ns) {		
			if (NULL != cur->ns->prefix) {
				g_assert(NULL != rss_nslist);
				if(NULL != (hp = (GSList *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix))) {
					nsh = (RSSNsHandler *)hp->data;
					parseFunc = nsh->parseItemTag;
					if(NULL != parseFunc)
						(*parseFunc)(i, doc, cur);
					cur = cur->next;
					continue;						
				} else {
					//g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);
				}
			}
		}
		
		/* check for RDF tags */
		for(j = 0; j < RSS_ITEM_MAX_TAG; j++) {
			g_assert(NULL != cur->name);
			if (!xmlStrcmp(cur->name, BAD_CAST itemTagList[j])) {
				tmp = i->tags[j];
				if(NULL == (i->tags[j] = CONVERT(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)))) {
					i->tags[j] = tmp;
				} else {
					g_free(tmp);
				}
			}		
		}
		
		if(!xmlStrcmp(cur->name, BAD_CAST"enclosure")) {
			/* RSS 0.93 allows multiple enclosures, so we build
			   a simple string of HTML-links... */
			link = CONVERT(xmlGetNoNsProp(cur, BAD_CAST"url"));
			
			if(NULL == (tmp = i->enclosure))
				tmp = g_strdup("");
			else
				tmp = g_strdup_printf("%s,", tmp);
				
			i->enclosure = g_strdup_printf("%s<a href=\"%s\">%s</a>", tmp, link, link);
		}

		cur = cur->next;
	}

	/* after parsing we fill the infos into the itemPtr structure */
	ip->type = FST_RSS;
	ip->time = i->time;
	ip->source = g_strdup(i->tags[RSS_ITEM_LINK]);
	ip->readStatus = FALSE;

	if(NULL == ip->id)
		ip->id = g_strdup(i->tags[RSS_ITEM_GUID]);

	/* some postprocessing before generating HTML */
/*	if(NULL != i->tags[RSS_ITEM_TITLE])
		i->tags[RSS_ITEM_TITLE] = unhtmlize("UTF-8", i->tags[RSS_ITEM_TITLE]);*/
		
	if(NULL != i->tags[RSS_ITEM_DESCRIPTION])
		i->tags[RSS_ITEM_DESCRIPTION] = convertToHTML("UTF-8", i->tags[RSS_ITEM_DESCRIPTION]);

	ip->title = g_strdup(i->tags[RSS_ITEM_TITLE]);		
	ip->description = showRSSItem(fp, cp, i);

	/* free RSSItem structure */
	for(j = 0; j < RSS_ITEM_MAX_TAG; j++)
		g_free(i->tags[j]);
	g_free(i->enclosure);
	g_free(i->nsinfos);
	g_free(i);
	return ip;
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

extern void showRSSFeedNSInfo(gpointer value, gpointer userdata);

/* writes item description as HTML into a buffer and returns
  a pointer to it */
static gchar * showRSSItem(feedPtr fp, RSSChannelPtr cp, RSSItemPtr ip) {
	gchar		*buffer = NULL;
	gchar		*tmp;	
	outputRequest	request;

	g_assert(NULL != ip);
	g_assert(NULL != cp);
	g_assert(NULL != fp);
	

	addToHTMLBuffer(&buffer, ITEM_HEAD_START);		
	addToHTMLBuffer(&buffer, ITEM_HEAD_CHANNEL);
	if(NULL != cp->tags[RSS_CHANNEL_LINK]) {
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", 
			cp->tags[RSS_CHANNEL_LINK],
			cp->tags[RSS_CHANNEL_TITLE]);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
	} else {
		addToHTMLBuffer(&buffer, cp->tags[RSS_CHANNEL_TITLE]);
	
	}

	addToHTMLBuffer(&buffer, HTML_NEWLINE);		
	addToHTMLBuffer(&buffer, ITEM_HEAD_ITEM);
	if(NULL != ip->tags[RSS_ITEM_LINK]) {
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", ip->tags[RSS_ITEM_LINK], 
					(NULL != ip->tags[RSS_ITEM_TITLE])?ip->tags[RSS_ITEM_TITLE]:ip->tags[RSS_ITEM_LINK]);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
	} else {
		addToHTMLBuffer(&buffer, (NULL != ip->tags[RSS_ITEM_TITLE])?ip->tags[RSS_ITEM_TITLE]:ip->tags[RSS_ITEM_LINK]);
	}
	addToHTMLBuffer(&buffer, ITEM_HEAD_END);

	/* process namespace infos */
	request.obj = ip;
	request.buffer = &buffer;
	request.type = OUTPUT_ITEM_NS_HEADER;	
	if(NULL != rss_nslist)
		g_slist_foreach(rss_nslist, showRSSFeedNSInfo, (gpointer)&request);

	if(NULL != cp->tags[RSS_CHANNEL_IMAGE]) {
		addToHTMLBuffer(&buffer, IMG_START);
		addToHTMLBuffer(&buffer, cp->tags[RSS_CHANNEL_IMAGE]);
		addToHTMLBuffer(&buffer, IMG_END);	
	}

	if(NULL != ip->tags[RSS_ITEM_DESCRIPTION])
		addToHTMLBuffer(&buffer, ip->tags[RSS_ITEM_DESCRIPTION]);

	if(NULL != ip->tags[RSS_ITEM_COMMENTS]) {
		tmp = g_strdup_printf("<div style=\"margin-top:5px;margin-bottom:5px;\">(<a href=\"%s\">%s</a>)</div>", 
				ip->tags[RSS_ITEM_COMMENTS],_("comments"));
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
	}
		
	if(NULL != ip->enclosure) {
		addToHTMLBuffer(&buffer, START_ENCLOSURE);
		addToHTMLBuffer(&buffer, ip->enclosure);
		addToHTMLBuffer(&buffer, END_ENCLOSURE);
	}

	if(NULL != ip->tags[RSS_ITEM_AUTHOR]) {
		addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_START);
		FEED_FOOT_WRITE(buffer, "author", ip->tags[RSS_ITEM_AUTHOR]);
		addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_END);
	}
	
	request.type = OUTPUT_ITEM_NS_FOOTER;
	if(NULL != rss_nslist)
		g_slist_foreach(rss_nslist, showRSSFeedNSInfo, (gpointer)&request);

	return buffer;
}
