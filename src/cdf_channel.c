/**
 * @file cdf_channel.c CDF channel parsing
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
#include "common.h"
#include "cdf_channel.h"
#include "cdf_item.h"
#include "callbacks.h"
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

/* returns CDF channel description as HTML */
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

/* method to parse standard tags for the channel element */
static void parseCDFChannel(feedPtr fp, CDFChannelPtr cp, xmlDocPtr doc, xmlNodePtr cur) {
	gchar		*tmp = NULL;
	itemPtr		ip;
	int		i;
	
	if((NULL == cur) || (NULL == doc)) {
		g_warning("internal error: XML document pointer NULL! This should not happen!\n");
		return;
	}
		
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if(NULL == cur->name) {
			g_warning("invalid XML, parser returns NULL value!");
			cur = cur->next;
			continue;
		}

		/* save first link to a channel image */
		if((!xmlStrcmp(cur->name, BAD_CAST"logo"))) {
			if(NULL != cp->tags[CDF_CHANNEL_IMAGE])				
				cp->tags[CDF_CHANNEL_IMAGE] = CONVERT(xmlGetNoNsProp(cur, BAD_CAST"href"));
			cur = cur->next;			
			continue;
		}
		
		if((!xmlStrcmp(cur->name, BAD_CAST"item"))) {
			if(NULL != (ip = parseCDFItem(fp, cp, doc, cur))) {
				if(0 == ip->time)
					ip->time = cp->time;
				feed_add_item(fp, ip);
			}	
		}

		for(i = 0; i < CDF_CHANNEL_MAX_TAG; i++) {
			if (!xmlStrcmp(cur->name, BAD_CAST CDFChannelTagList[i])) {			
				tmp = CONVERT(xmlNodeListGetString(doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					g_free(cp->tags[i]);
					cp->tags[i] = tmp;
				}
			}		
		}
		cur = cur->next;
	}

	/* some postprocessing */
	if(NULL != cp->tags[CDF_CHANNEL_TITLE])
		cp->tags[CDF_CHANNEL_TITLE] = unhtmlize(cp->tags[CDF_CHANNEL_TITLE]);
		
	if(NULL != cp->tags[CDF_CHANNEL_DESCRIPTION])
		cp->tags[CDF_CHANNEL_DESCRIPTION] = convertToHTML(cp->tags[CDF_CHANNEL_DESCRIPTION]);		
	
}

/* reads a CDF feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void readCDFFeed(feedPtr fp, gchar *data) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur;
	CDFChannelPtr 	cp;
	int 		error = 0;
	
	cp = g_new0(struct CDFChannel, 1);
	cp->nsinfos = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	
	while(1) {
		if(NULL == (doc = parseBuffer(data, &(fp->parseErrors)))) {
			addToHTMLBuffer(&(fp->parseErrors), g_strdup_printf(_("<p>XML error while reading feed! Feed \"%s\" could not be loaded!</p>"), fp->source));
			error = 1;
			break;
		}

		cur = xmlDocGetRootElement(doc);

		if(NULL == cur) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Empty document!</p>"));
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
		while(cur != NULL) {
			if((!xmlStrcmp(cur->name, BAD_CAST"channel"))) {
				cur = cur->xmlChildrenNode;
				break;
			}
			cur = cur->next;
		}

		time(&(cp->time));
		
		/* find first "real" channel tag */
		while(cur != NULL) {
		
			if((!xmlStrcmp(cur->name, BAD_CAST"channel"))) {
				parseCDFChannel(fp, cp, doc, cur);			
				g_assert(NULL != cur);
				break;
			}
			g_assert(NULL != cur);
			cur = cur->next;
		}

		xmlFreeDoc(doc);
		
		/* after parsing we fill in the infos into the feedPtr structure */		
		fp->defaultInterval = fp->updateInterval = -1;
		fp->title = cp->tags[CDF_CHANNEL_TITLE];

		if(0 == error) {
			fp->available = TRUE;
			fp->description = showCDFFeedInfo(cp, fp->source);
		} else {
			ui_mainwindow_set_status_bar(_("There were errors while parsing this feed!"));
		}
		
		g_hash_table_destroy(cp->nsinfos);
		g_free(cp);
		break;
	}
}

/* ---------------------------------------------------------------------------- */
/* initialization		 						*/
/* ---------------------------------------------------------------------------- */

feedHandlerPtr initCDFFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);
	
	/* there are no name space handlers! */

	/* prepare feed handler structure */
	fhp->readFeed		= readCDFFeed;
	fhp->merge		= TRUE;
	
	return fhp;
}
