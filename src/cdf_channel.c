/*
   CDF channel parsing
      
   Note: portions of the original parser code were inspired by
   the feed reader software Rol which is copyrighted by
   
   Copyright (C) 2002 Jonathan Gordon <eru@unknown-days.com>
   
   The major part of this backend/parsing/storing code written by
   
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

#include <sys/time.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "conf.h"
#include "support.h"
#include "common.h"
#include "cdf_channel.h"
#include "cdf_item.h"

#include "netio.h"
#include "htmlview.h"

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
feedPtr		readCDFFeed(gchar *url);
gchar *		showCDFFeedInfo(CDFChannelPtr cp, gchar *url);

feedHandlerPtr initCDFFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	if(NULL == (fhp = (feedHandlerPtr)g_malloc(sizeof(struct feedHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(fhp, 0, sizeof(struct feedHandler));
	
	/* there are no name space handlers! */

	/* prepare feed handler structure */
	fhp->readFeed		= readCDFFeed;
	
	return fhp;
}

/* method to parse standard tags for the channel element */
static void parseCDFChannel(feedPtr fp, CDFChannelPtr cp, xmlDocPtr doc, xmlNodePtr cur) {
	gchar		*tmp = NULL;
	gchar		*encoding;
	gchar		*value;
	itemPtr		ip;
	int		i;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning(_("internal error: XML document pointer NULL! This should not happen!\n"));
		return;
	}
		
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {

		/* save first link to a channel image */
		if((!xmlStrcmp(cur->name, (const xmlChar *) "logo"))) {
			if(NULL != cp->tags[CDF_CHANNEL_IMAGE]) {
				value = xmlGetNoNsProp(cur, (const xmlChar *)"href");
				if(NULL != value)
					cp->tags[CDF_CHANNEL_IMAGE] = g_strdup(value);
				g_free(value);
			}
			cur = cur->next;			
			continue;
		}
		
		if((!xmlStrcmp(cur->name, (const xmlChar *) "item"))) {
			if(NULL != (ip = parseCDFItem(fp, cp, doc, cur))) {
				if(0 == ip->time)
					ip->time = cp->time;
				addItem(fp, ip);
			}	
		}

		for(i = 0; i < CDF_CHANNEL_MAX_TAG; i++) {
			g_assert(NULL != cur->name);
			if (!xmlStrcmp(cur->name, (const xmlChar *)CDFChannelTagList[i])) {
				tmp = cp->tags[i];
				if(NULL == (cp->tags[i] = g_strdup(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1)))) {
					cp->tags[i] = tmp;
				} else {
					g_free(tmp);
				}
			}		
		}
		cur = cur->next;
	}

	/* some postprocessing */
	if(NULL != cp->tags[CDF_CHANNEL_TITLE])
		cp->tags[CDF_CHANNEL_TITLE] = unhtmlize((gchar *)doc->encoding, cp->tags[CDF_CHANNEL_TITLE]);
		
	if(NULL != cp->tags[CDF_CHANNEL_DESCRIPTION])
		cp->tags[CDF_CHANNEL_DESCRIPTION] = convertToHTML((gchar *)doc->encoding, cp->tags[CDF_CHANNEL_DESCRIPTION]);		
	
}

/* reads a CDF feed URL and returns a new channel structure (even if
   the feed could not be read) */
feedPtr readCDFFeed(gchar *url) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	CDFChannelPtr 	cp;
	feedPtr		fp;
	gchar		*encoding;
	char		*data;
	int 		error = 0;
	
	/* initialize channel structure */
	if(NULL == (cp = (CDFChannelPtr) malloc(sizeof(struct CDFChannel)))) {
		g_error("not enough memory!\n");
		return NULL;
	}
	memset(cp, 0, sizeof(struct CDFChannel));
	cp->nsinfos = g_hash_table_new(g_str_hash, g_str_equal);		
	fp = getNewFeedStruct();
	
	while(1) {
		if(NULL == (data = downloadURL(url))) {
			error = 1;
			break;
		}

		doc = xmlRecoverMemory(data, strlen(data));
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

		time(&(cp->time));
		
		/* find first "real" channel tag */
		while (cur != NULL) {
		
			if ((!xmlStrcmp(cur->name, (const xmlChar *) "channel"))) {
				parseCDFChannel(fp, cp, doc, cur);			
				g_assert(NULL != cur);
				break;
			}
			g_assert(NULL != cur);
			cur = cur->next;
		}

		xmlFreeDoc(doc);
		
		/* after parsing we fill in the infos into the feedPtr structure */		
		fp->type = FST_RSS;
		fp->defaultInterval = fp->updateInterval = -1;
		fp->title = cp->tags[CDF_CHANNEL_TITLE];

		if(0 == error) {
			fp->available = TRUE;
			fp->description = showCDFFeedInfo(cp, url);
		}
		
		g_free(cp->nsinfos);
		g_free(cp);
		break;
	}

	return fp;
}

/* ---------------------------------------------------------------------------- */
/* HTML output stuff	 							*/
/* ---------------------------------------------------------------------------- */

/* writes CDF channel description as HTML into the gtkhtml widget */
gchar * showCDFFeedInfo(CDFChannelPtr cp, gchar *url) {
	gchar		*buffer = NULL;
	gchar		*tmp;

	g_assert(cp != NULL);

	addToHTMLBuffer(&buffer, FEED_HEAD_START);
	addToHTMLBuffer(&buffer, FEED_HEAD_CHANNEL);
	tmp = g_strdup_printf("<a href=\"%s\">%s</a>", 
		url, 
		cp->tags[CDF_CHANNEL_TITLE]);
	addToHTMLBuffer(&buffer, tmp);
	g_free(tmp);
	
	addToHTMLBuffer(&buffer, FEED_HEAD_END);	

	if(NULL != cp->tags[CDF_CHANNEL_IMAGE]) {
		addToHTMLBuffer(&buffer, IMG_START);
		addToHTMLBuffer(&buffer, cp->tags[CDF_CHANNEL_IMAGE]);
		addToHTMLBuffer(&buffer, IMG_END);	
	}

	if(NULL != cp->tags[CDF_CHANNEL_DESCRIPTION])
		addToHTMLBuffer(&buffer, cp->tags[CDF_CHANNEL_DESCRIPTION]);

	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(buffer, "copyright",		cp->tags[CDF_CHANNEL_COPYRIGHT]);
	FEED_FOOT_WRITE(buffer, "publication date",	cp->tags[CDF_CHANNEL_PUBDATE]);
	FEED_FOOT_WRITE(buffer, "webmaster",		cp->tags[CDF_CHANNEL_WEBMASTER]);
	FEED_FOOT_WRITE(buffer, "category",		cp->tags[CDF_CHANNEL_CATEGORY]);
	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_END);

	return buffer;
}
