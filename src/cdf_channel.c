/*
   CDF channel parsing
      
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
#include "cdf_channel.h"
#include "cdf_item.h"
#include "htmlview.h"

extern GMutex * entries_lock;	// FIXME
extern GHashTable *entries;	// FIXME

/* note: the tag order has to correspond with the CHANNEL_* defines in the header file */
static gchar *CDFChannelTagList[] = {	"title",
					"abstract",
					"logo",
					"copyright",
					"publicationdate",
					"publisher",
					"category",
					NULL
				  };

/* prototypes */
gpointer 	loadCDFFeed(gchar *keyprefix, gchar *key);
gpointer	readCDFFeed(gchar *url);
gpointer	mergeCDFFeed(gpointer old_fp, gpointer new_fp);
gchar * 	getCDFFeedTag(gpointer cp, int tag);
gpointer 	getCDFFeedProp(gpointer fp, gint proptype);
void 		setCDFFeedProp(gpointer fp, gint proptype, gpointer data);
void		showCDFFeedInfo(gpointer fp);

/* display an items description in the HTML widget */
void	showCDFItem(gpointer ip, gpointer cp);

/* display a feed info in the HTML widget */
void	showCDFFeedInfo(gpointer cp);

feedHandlerPtr initCDFFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	if(NULL == (fhp = (feedHandlerPtr)g_malloc(sizeof(struct feedHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(fhp, 0, sizeof(struct feedHandler));
	
	/* there are no name space handlers! */

	/* prepare feed handler structure */
	fhp->loadFeed		= loadCDFFeed;	
	fhp->readFeed		= readCDFFeed;
	fhp->mergeFeed		= mergeCDFFeed;
	fhp->removeFeed		= NULL;	// FIXME
	fhp->getFeedProp	= getCDFFeedProp;
	fhp->setFeedProp	= setCDFFeedProp;	
	fhp->showFeedInfo	= showCDFFeedInfo;
	
	return fhp;
}

/* method to parse standard tags for the channel element */
static void parseCDFChannel(CDFChannelPtr c, xmlDocPtr doc, xmlNodePtr cur) {
	CDFItemPtr	ip, lastip = NULL;
	gchar		*tmp = NULL;
	gchar		*encoding;
	gchar		*value;
	int		i;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}
		
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {

		/* save first link to a channel image */
		if((!xmlStrcmp(cur->name, (const xmlChar *) "logo"))) {
			if(NULL != c->tags[CDF_CHANNEL_IMAGE]) {
				value = xmlGetNoNsProp(cur, (const xmlChar *)"href");
				if(NULL != value)
					c->tags[CDF_CHANNEL_IMAGE] = g_strdup(value);
				g_free(value);
			}
			cur = cur->next;			
			continue;
		}
		
		if((!xmlStrcmp(cur->name, (const xmlChar *) "item"))) {
			if(NULL != (ip = (CDFItemPtr) parseCDFItem(doc, cur))) {
				c->unreadCounter++;
				ip->cp = c;
				if(NULL == ip->time)
					ip->time = c->time;

				c->items = g_slist_append(c->items, ip);
			}	
		}

		for(i = 0; i < CDF_CHANNEL_MAX_TAG; i++) {
			g_assert(NULL != cur->name);
			if (!xmlStrcmp(cur->name, (const xmlChar *)CDFChannelTagList[i])) {
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
	if(NULL != c->tags[CDF_CHANNEL_TITLE])
		c->tags[CDF_CHANNEL_TITLE] = unhtmlize((gchar *)doc->encoding, c->tags[CDF_CHANNEL_TITLE]);
		
	if(NULL != c->tags[CDF_CHANNEL_DESCRIPTION])
		c->tags[CDF_CHANNEL_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, c->tags[CDF_CHANNEL_DESCRIPTION]);		
	
}

/* loads a saved CDF feed from disk */
gpointer loadCDFFeed(gchar *keyprefix, gchar *key) {
	CDFChannelPtr	new_cp = NULL;

	// workaround as long loading is not implemented
	if(NULL == (new_cp = (CDFChannelPtr) malloc(sizeof(struct CDFChannel)))) {
		g_error("not enough memory!\n");
		return NULL;
	}

	memset(new_cp, 0, sizeof(struct CDFChannel));
	new_cp->updateInterval = -1;
	new_cp->updateCounter = 0;	/* to enforce immediate reload */
	new_cp->type = FST_RSS;
	
	return (gpointer)new_cp;
}

/* reads a CDF feed URL and returns a new channel structure (even if
   the feed could not be read) */
gpointer readCDFFeed(gchar *url) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	CDFItemPtr 	ip, lastip = NULL;
	CDFChannelPtr 	cp;
	gchar		*encoding;
	int 		error = 0;
	
	/* initialize channel structure */
	if(NULL == (cp = (CDFChannelPtr) malloc(sizeof(struct CDFChannel)))) {
		g_error("not enough memory!\n");
		return NULL;
	}
	memset(cp, 0, sizeof(struct CDFChannel));
	cp->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);		
	
	cp->updateInterval = -1;
	cp->updateCounter = -1;
	cp->key = NULL;	
	cp->items = NULL;
	cp->available = FALSE;
	cp->source = g_strdup(url);
	cp->type = FST_CDF;
	
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

		while(cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
	
		/* note: we support only one flavour of CDF channels! We will only
		   support the first channel of the CDF feed. */
	
		/* find outer channel tag */
		while (cur != NULL) {
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "channel"))) {
				cur = cur->xmlChildrenNode;
				break;
			}
			cur = cur->next;
		}

		cp->time = getActualTime();
		cp->encoding = g_strdup(doc->encoding);
		cp->available = TRUE;		
		
		/* find first "real" channel tag */
		while (cur != NULL) {
		
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "channel"))) {
				parseCDFChannel(cp, doc, cur);			
				g_assert(NULL != cur);
				break;
			}
			g_assert(NULL != cur);
			cur = cur->next;
		}

		xmlFreeDoc(doc);
		break;
	}

	return (gpointer)cp;
}


/* used to merge two CDFChannelPtr structures after while
   updating a feed, returns a CDFChannelPtr to the merged
   structure and frees (FIXME) all unneeded memory */
gpointer mergeCDFFeed(gpointer old_fp, gpointer new_fp) {
	CDFChannelPtr	new = (CDFChannelPtr) new_fp;
	CDFChannelPtr	old = (CDFChannelPtr) old_fp;
		
	// FIXME: compare items, merge appropriate
	// actually this function does almost nothing
	
	new->updateInterval = old->updateInterval;
	new->updateCounter = old->updateInterval;	/* resetting the counter */
	new->usertitle = old->usertitle;
	new->key = old->key;
	new->source = old->source;
	new->type = old->type;
	new->keyprefix = old->keyprefix;
	
	// FIXME: free old_cp memory
		
	return new_fp;
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* writes CDF channel description as HTML into the gtkhtml widget */
void showCDFFeedInfo(gpointer fp) {
	CDFChannelPtr	cp = (CDFChannelPtr)fp;
	gchar		*feedimage;
	gchar		*feeddescription;
	gchar		*source;
	gchar		*tmp;

	g_assert(cp != NULL);
	
	startHTMLOutput();
	writeHTML(HTML_HEAD_START);

	writeHTML(META_ENCODING1);
	writeHTML("UTF-8");
	writeHTML(META_ENCODING2);

	writeHTML(HTML_HEAD_END);

	writeHTML(FEED_HEAD_START);
	
	writeHTML(FEED_HEAD_CHANNEL);
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>", 
		cp->source, 
		getDefaultEntryTitle(cp->key));
	writeHTML(tmp);
	g_free(tmp);
	
	writeHTML(FEED_HEAD_END);	

	if(NULL != (feedimage = getCDFFeedTag(cp, CDF_CHANNEL_IMAGE))) {
		writeHTML(IMG_START);
		writeHTML(feedimage);
		writeHTML(IMG_END);	
	}

	if(NULL != (feeddescription = getCDFFeedTag(cp, CDF_CHANNEL_DESCRIPTION)))
		writeHTML(feeddescription);

	writeHTML(FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(doc, "copyright",		getCDFFeedTag(cp, CDF_CHANNEL_COPYRIGHT));
	FEED_FOOT_WRITE(doc, "publication date",	getCDFFeedTag(cp, CDF_CHANNEL_PUBDATE));
	FEED_FOOT_WRITE(doc, "webmaster",		getCDFFeedTag(cp, CDF_CHANNEL_WEBMASTER));
	FEED_FOOT_WRITE(doc, "category",		getCDFFeedTag(cp, CDF_CHANNEL_CATEGORY));
	writeHTML(FEED_FOOT_TABLE_END);

	finishHTMLOutput();
}

/* ---------------------------------------------------------------------------- */
/* just some encapsulation 							*/
/* ---------------------------------------------------------------------------- */

gchar * getCDFFeedTag(gpointer cp, int tag) { 
	
	if(NULL == cp)
		return NULL;

	g_assert(FST_CDF == ((CDFChannelPtr)cp)->type);
	return ((CDFChannelPtr)cp)->tags[tag]; 
}

void setCDFFeedProp(gpointer fp, gint proptype, gpointer data) {
	CDFChannelPtr	c = (CDFChannelPtr)fp;
	
	if(NULL != c) {
		g_assert(FST_CDF == c->type);
		switch(proptype) {
			case FEED_PROP_TITLE:
				g_free(c->tags[CDF_CHANNEL_TITLE]);
				c->tags[CDF_CHANNEL_TITLE] = (gchar *)data;
				break;
			case FEED_PROP_USERTITLE:
				g_free(c->usertitle);
				c->usertitle = (gchar *)data;
				break;
			case FEED_PROP_SOURCE:
				g_free(c->source);
				c->source = (gchar *)data;
				break;
			case FEED_PROP_DFLTUPDINTERVAL:			
				// FIXME: implement <schedule> support
			case FEED_PROP_UPDATEINTERVAL:
				c->updateInterval = (gint)data;
				break;
			case FEED_PROP_UPDATECOUNTER:
				c->updateCounter = (gint)data;
				break;
			case FEED_PROP_UNREADCOUNT:
				c->unreadCounter = (gint)data;
				break;
			case FEED_PROP_AVAILABLE:
				c->available = (gboolean)data;
				break;
			case FEED_PROP_ITEMLIST:
				g_error("please don't do this!");
				break;
			default:
				g_error(_("internal error! unknown feed property type!\n"));
				break;
		}
	} else {
		return;
	}
}

gpointer getCDFFeedProp(gpointer fp, gint proptype) {
	CDFChannelPtr	c = (CDFChannelPtr)fp;
	
	if(NULL != c) {
		g_assert(FST_CDF == c->type);
		switch(proptype) {
			case FEED_PROP_TITLE:
				return (gpointer)getCDFFeedTag(c, CDF_CHANNEL_TITLE);
				break;
			case FEED_PROP_USERTITLE:
				return (gpointer)c->usertitle;
				break;
			case FEED_PROP_SOURCE:
				return (gpointer)c->source;
				break;
			case FEED_PROP_DFLTUPDINTERVAL:
				// FIXME: implement <schedule> support
				return (gpointer)-1;
				break;
			case FEED_PROP_UPDATEINTERVAL:
				return (gpointer)c->updateInterval;
				break;
			case FEED_PROP_UPDATECOUNTER:
				return (gpointer)c->updateCounter;
				break;				
			case FEED_PROP_UNREADCOUNT:
				return (gpointer)c->unreadCounter;
				break;
			case FEED_PROP_AVAILABLE:
				return (gpointer)c->available;
				break;
			case FEED_PROP_ITEMLIST:
				return (gpointer)c->items;
				break;
			default:
				g_error(_("internal error! unknow feed property type!\n"));
				break;
		}
	} else {
		return NULL;
	}
}
