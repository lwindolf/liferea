/*
   RSS channel parsing
      
   Note: portions of the original parser code were inspired by
   the feed reader software Rol which is copyrighted by
   
   Copyright (C) 2002 Jonathan Gordon <eru@unknown-days.com>
   
   The major part of this backend/parsing/storing code written by
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <sys/time.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "conf.h"
#include "support.h"
#include "common.h"
#include "rss_channel.h"
#include "rss_ns.h"
// FIXME
#include "backend.h"	

#define	TIMESTRLEN	256


extern GMutex * entries_lock;	// FIXME
extern GHashTable *entries;	// FIXME

/* to store the RSSNsHandler structs for all supported RDF namespace handlers */
GHashTable	*rss_nslist = NULL;

/* note: the tag order has to correspond with the CHANNEL_* defines in the header file */
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

/* method to parse standard tags for the channel element */
static void parseChannel(channelPtr c, xmlDocPtr doc, xmlNodePtr cur) {
	gchar			*tmp = NULL;
	gchar			*encoding;
	parseChannelTagFunc	fp;
	RSSNsHandler		*nsh;
	int			i;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}
		
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
	
		/* check namespace of this tag */
		if(NULL != cur->ns) {
			if (NULL != cur->ns->prefix) {
				if(NULL != (nsh = (RSSNsHandler *)g_hash_table_lookup(rss_nslist, (gpointer)cur->ns->prefix))) {
					fp = nsh->parseChannelTag;
					if(NULL != fp)
						(*fp)(c, doc, cur);
					cur = cur->next;
					continue;
				} else {
//					g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);
				}
			}
		}

		/* check for RDF tags */
		for(i = 0; i < CHANNEL_MAX_TAG; i++) {
			g_assert(NULL != cur->name);
			if (!xmlStrcmp(cur->name, (const xmlChar *)channelTagList[i])) {
				tmp = c->tags[i];
				if(NULL == (c->tags[i] = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)))) {
					c->tags[i] = tmp;
				} else {
					g_free(tmp);
				}
			}		
		}
		cur = cur->next;
	}

	/* some postprocessing */
	if(NULL != c->tags[CHANNEL_TITLE])
		c->tags[CHANNEL_TITLE] = unhtmlize((gchar *)doc->encoding, c->tags[CHANNEL_TITLE]);
		
	if(NULL != c->tags[CHANNEL_DESCRIPTION])
		c->tags[CHANNEL_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, c->tags[CHANNEL_DESCRIPTION]);		
	
}

static void parseImage(xmlDocPtr doc, xmlNodePtr cur, channelPtr cp) {

	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}
	
	if(NULL == cp) {
		g_warning(_("internal error: parseImage without a channel! Skipping image!\n"));
		return;
	}
		
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (!xmlStrcmp(cur->name, (const xmlChar *)"url"))
			cp->tags[CHANNEL_IMAGE] = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
			
		cur = cur->next;
	}
}

/* reads a feed URL and returns a new channel structure (even if
   the feed could not be read) */
channelPtr readRSSFeed(gchar *url) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	time_t		t;
	itemPtr 	ip, lastip = NULL;
	channelPtr 	cp;
	gchar		*encoding;
	gchar		*timestr;
	gchar		*timeformat;
	short 		rdf = 0;
	int 		error = 0;
	
	/* initialize channel structure */
	if(NULL == (cp = (channelPtr) malloc(sizeof(struct channel)))) {
		g_error("not enough memory!\n");
		return NULL;
	}
	memset(cp, 0, sizeof(struct channel));
	cp->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);		
	
	cp->updateInterval = -1;
	cp->updateCounter = -1;
	cp->key = NULL;	
	cp->items = NULL;
	cp->available = FALSE;
	cp->source = g_strdup(url);
	
	while(1) {
		print_status(g_strdup_printf(_("reading from %s"),url));
		
		doc = xmlParseFile(url);

		if(NULL == doc) {
			print_status(g_strdup_printf(_("XML error wile reading feed! Feed \"%s\" could not be loaded!"), url));
			error = 1;
			break;
		}

		cur = xmlDocGetRootElement(doc);

		if(NULL == cur) {
			print_status(_("Empty document! Feed was not added!"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		if (!xmlStrcmp(cur->name, (const xmlChar *)"rss")) {
			rdf = 0;
		} else if (!xmlStrcmp(cur->name, (const xmlChar *)"rdf") || 
                	   !xmlStrcmp(cur->name, (const xmlChar *)"RDF")) {
			rdf = 1;
		} else {
			print_status(_("Could not find RDF/RSS header! Feed was not added!"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		cur = cur->xmlChildrenNode;
		while(cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
	
		while (cur != NULL) {
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "channel"))) {
				parseChannel(cp, doc, cur);
				g_assert(NULL != cur);
				if(0 == rdf)
					cur = cur->xmlChildrenNode;
				break;
			}
			g_assert(NULL != cur);
			cur = cur->next;
		}

		/* get receive time */
		if((time_t)-1 != time(&t)) {
			if(NULL != (timestr = (gchar *)g_malloc(TIMESTRLEN+1))) {
				if(NULL != (timeformat = getStringConfValue(TIME_FORMAT))) {
					strftime(timestr, TIMESTRLEN, (char *)timeformat, gmtime(&t));
					g_free(timeformat);
				}
			}
		}

		cp->time = timestr;
		cp->encoding = g_strdup(doc->encoding);
		cp->available = TRUE;

		/* parse channel contents */
		while (cur != NULL) {
			/* save link to channel image */
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "image"))) {
				parseImage(doc, cur, cp);
			}

			/* collect channel items */
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "item"))) {
				if(NULL != (ip = (itemPtr) parseItem(doc, cur))) {
					cp->unreadCounter++;
					ip->cp = cp;
					if(NULL == ip->time)
						ip->time = cp->time;
					ip->next = NULL;
					if(NULL == lastip)
						cp->items = ip;
					else
						lastip->next = ip;
					lastip = ip;				
				}
			}
			cur = cur->next;
		}

		xmlFreeDoc(doc);
		break;
	}

	return cp;
}


/* used to merge two channelPtr structures after while
   updating a feed, returns a channelPtr to the merged
   structure and frees (FIXME) all unneeded memory */
channelPtr mergeRSSFeed(channelPtr old_cp, channelPtr new_cp) {

	// FIXME: compare items, merge appropriate
	// actually this function does almost nothing
	
	new_cp->updateInterval = old_cp->updateInterval;
	new_cp->updateCounter = old_cp->updateInterval;	/* resetting the counter */
	new_cp->usertitle = old_cp->usertitle;
	new_cp->key = old_cp->key;
	new_cp->source = old_cp->source;
	new_cp->type = old_cp->type;
	new_cp->keyprefix = old_cp->keyprefix;
	
	// FIXME: free old_cp memory
		
	return new_cp;
}

/* just some encapsulation */

gint	getRSSFeedUnreadCount(gchar *feedkey) { 
	channelPtr	c;
	
	g_mutex_lock(entries_lock);
	c = (channelPtr)g_hash_table_lookup(entries, (gpointer)feedkey);
	g_mutex_unlock(entries_lock);
	
	if(NULL != c) {
		g_assert(IS_FEED(c->type));
		return c->unreadCounter;
	} else {
		return 0;
	}
}

GHashTable * getFeedNsHandler(gpointer cp) { return rss_nslist; }

gchar * getFeedTag(gpointer cp, int tag) { return ((channelPtr)cp)->tags[tag]; }
