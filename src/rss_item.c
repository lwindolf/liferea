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

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "support.h"
#include "common.h"
#include "rss_item.h"
#include "rss_ns.h"
#include "htmlview.h"

/* structure for the hashtable callback which itself calls the 
   namespace output handler */
#define OUTPUT_RSS_CHANNEL_NS_HEADER	0
#define	OUTPUT_RSS_CHANNEL_NS_FOOTER	1
#define OUTPUT_ITEM_NS_HEADER		2
#define OUTPUT_ITEM_NS_FOOTER		3
typedef struct {
	gint		type;	
	gpointer	obj;	/* thats either a RSSChannelPtr or a RSSItemPtr 
				   depending on the type value */
} outputRequest;

/* uses the same namespace handler as rss_channel */
extern GHashTable *rss_nslist;

static gchar *itemTagList[] = {		"title",
					"description",
					"link",
					"author",
					"comments",
					"enclosure",
					"category",
					NULL
				  };
				  
/* prototypes */
static gchar * getRSSItemTag(RSSItemPtr ip, int tag);
gpointer getRSSItemProp(gpointer ip, gint proptype);
void setRSSItemProp(gpointer ip, gint proptype, gpointer data);
void showRSSItem(gpointer ip);

itemHandlerPtr initRSSItemHandler(void) {
	itemHandlerPtr	ihp;
	
	if(NULL == (ihp = (itemHandlerPtr)g_malloc(sizeof(struct itemHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(ihp, 0, sizeof(struct itemHandler));
	
	/* the RSS/RDF item handling reuses the RSS/RDF channel
	   namespace handler */

	/* prepare item handler structure */
	ihp->getItemProp	= getRSSItemProp;	
	ihp->setItemProp	= setRSSItemProp;
	ihp->showItem		= showRSSItem;
	
	return ihp;
}

/* method to parse standard tags for each item element */
RSSItemPtr parseItem(xmlDocPtr doc, xmlNodePtr cur) {
	gint			bw, br;
	gchar			*tmp = NULL;
	parseItemTagFunc	fp;
	RSSNsHandler		*nsh;	
	RSSItemPtr 		i = NULL;
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

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		/* check namespace of this tag */
		if(NULL != cur->ns) {		
			if (NULL != cur->ns->prefix) {
				if(NULL != (nsh = (RSSNsHandler *)g_hash_table_lookup(rss_nslist, (gpointer)cur->ns->prefix))) {	
					fp = nsh->parseItemTag;
					if(NULL != fp)
						(*fp)(i, doc, cur);
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
			if (!xmlStrcmp(cur->name, (const xmlChar *)itemTagList[j])) {
				tmp = i->tags[j];
				if(NULL == (i->tags[j] = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)))) {
					i->tags[j] = tmp;
				} else {
					g_free(tmp);
				}
			}		
		}		

		cur = cur->next;
	}

	/* some postprocessing */
	if(NULL != i->tags[RSS_ITEM_TITLE])
		i->tags[RSS_ITEM_TITLE] = unhtmlize((gchar *)doc->encoding, i->tags[RSS_ITEM_TITLE]);
		
	if(NULL != i->tags[RSS_ITEM_DESCRIPTION])
		i->tags[RSS_ITEM_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, i->tags[RSS_ITEM_DESCRIPTION]);	
		
	return(i);
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

extern void showRSSFeedNSInfo(gpointer key, gpointer value, gpointer userdata);

/* writes item description as HTML into the gtkhtml widget */
void showRSSItem(gpointer ip) {
	RSSChannelPtr	cp;
	gchar		*itemlink;
	gchar		*feedimage;
	gchar		*tmp;	
	outputRequest	request;

	g_assert(NULL != ip);	
	cp = ((RSSItemPtr)ip)->cp;
	g_assert(NULL != cp);

	startHTMLOutput();
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	if(NULL != (itemlink = getRSSItemTag((RSSItemPtr)ip, RSS_ITEM_LINK))) {
		writeHTML(ITEM_HEAD_START);
		
		writeHTML(ITEM_HEAD_CHANNEL);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", 
			cp->tags[RSS_CHANNEL_LINK],
			getDefaultEntryTitle(cp->key));
		writeHTML(tmp);
		g_free(tmp);
		
		writeHTML(HTML_NEWLINE);
		
		writeHTML(ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", itemlink, getRSSItemTag(ip, RSS_ITEM_TITLE));
		writeHTML(tmp);
		g_free(tmp);
		
		writeHTML(ITEM_HEAD_END);	
	}	

	/* process namespace infos */
	request.obj = ip;
	request.type = OUTPUT_ITEM_NS_HEADER;	
	if(NULL != rss_nslist)
		g_hash_table_foreach(rss_nslist, showRSSFeedNSInfo, (gpointer)&request);

	if(NULL != (feedimage = cp->tags[RSS_CHANNEL_IMAGE])) {
		writeHTML(IMG_START);
		writeHTML(feedimage);
		writeHTML(IMG_END);	
	}

	if(NULL != getRSSItemTag(ip, RSS_ITEM_DESCRIPTION))
		writeHTML(getRSSItemTag(ip, RSS_ITEM_DESCRIPTION));

	request.type = OUTPUT_ITEM_NS_FOOTER;
	if(NULL != rss_nslist)
		g_hash_table_foreach(rss_nslist, showRSSFeedNSInfo, (gpointer)&request);


	finishHTMLOutput();
}

/* ---------------------------------------------------------------------------- */
/* just some encapsulation 							*/
/* ---------------------------------------------------------------------------- */

static gchar * getRSSItemTag(RSSItemPtr ip, int tag) {

	if(NULL == ip)
		return NULL;
	
	g_assert(NULL != ip->cp);
	g_assert(FST_RSS == ((RSSChannelPtr)(ip->cp))->type);
	return ip->tags[tag];
}

void setRSSItemProp(gpointer ip, gint proptype, gpointer data) {
	RSSItemPtr	i = (RSSItemPtr)ip;
	
	if(NULL != i) {
		g_assert(NULL != i->cp);	
		g_assert(FST_RSS == ((RSSChannelPtr)(i->cp))->type);
		switch(proptype) {
			case ITEM_PROP_TITLE:
				g_free(i->tags[RSS_ITEM_TITLE]);
				i->tags[RSS_ITEM_TITLE] = (gchar *)data;
				break;
			case ITEM_PROP_READSTATUS:
				/* no matter what data was given... */
				if(FALSE == i->read) {
					((RSSChannelPtr)(i->cp))->unreadCounter--;
					i->read = TRUE;
				}
				break;
			case ITEM_PROP_DESCRIPTION:
			case ITEM_PROP_TIME:
			case ITEM_PROP_SOURCE:
			case ITEM_PROP_TYPE:
				g_error("please don't do this!");
				break;
			default:
				g_error(_("intenal error! unknow item property type!\n"));
				break;
		}
	}
}

gpointer getRSSItemProp(gpointer ip, gint proptype) {
	RSSItemPtr	i = (RSSItemPtr)ip;
	
	if(NULL != i) {
		g_assert(NULL != i->cp);	
		g_assert(FST_RSS == ((RSSChannelPtr)(i->cp))->type);
		switch(proptype) {
			case ITEM_PROP_TITLE:
				return (gpointer)getRSSItemTag(i, RSS_ITEM_TITLE);
				break;
			case ITEM_PROP_READSTATUS:
				return (gpointer)i->read;
				break;
			case ITEM_PROP_DESCRIPTION:
				return (gpointer)getRSSItemTag(i, RSS_ITEM_DESCRIPTION);
				break;
			case ITEM_PROP_TIME:
				return (gpointer)i->time;
				break;
			case ITEM_PROP_SOURCE:
				return (gpointer)getRSSItemTag(i, RSS_ITEM_LINK);
				break;
			case ITEM_PROP_TYPE:
				return (gpointer)FST_RSS;
				break;
			default:
				g_error(_("intenal error! unknow item property type!\n"));
				break;
		}
	} else {
		return NULL;
	}
}
