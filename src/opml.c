/*
   generic OPML 1.0 support
   
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

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "conf.h"
#include "common.h"
#include "feed.h"
#include "item.h"
#include "opml.h"

#include "netio.h"
#include "htmlview.h"

/* you can find the OPML specification at Userland:

   http://www.opml.org/
   
 */
 
/* this is a generic subtag list for directory, channel and format description tags */
#define OPML_TITLE		0
#define OPML_CREATED		1
#define OPML_MODIFIED		2
#define OPML_OWNERNAME		3
#define OPML_OWNEREMAIL		4
#define OPML_MAX_TAG		5

/* note: the tag order has to correspond with the OCS_* defines in the header file */
static gchar *opmlTagList[] = {	"title",
				"dateCreated",
				"dateModified",
				"ownerName",
				"ownerEmail",
				NULL
 			      };

/* ---------------------------------------------------------------------------- */
/* OPML parsing and HTML output	 						*/
/* ---------------------------------------------------------------------------- */
			      
/* retruns a HTML string containing the text and attributes of the outline */
static gchar * getOutlineContents(xmlNodePtr cur) {
	gchar		*buffer = NULL;
	gchar		*tmp, *value;
	gboolean	link = FALSE;
	xmlAttrPtr	attr;

	attr = cur->properties;
	while(NULL != attr) {
		/* get prop value */
		value = xmlGetNoNsProp(cur, attr->name);

		if(!xmlStrcmp(attr->name, BAD_CAST"text")) {		
			tmp = g_strdup_printf("<p>%s</p>", value);
			addToHTMLBuffer(&buffer, tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(attr->name, BAD_CAST"isComment")) {
			/* don't output anything */
			
		} else if(!xmlStrcmp(attr->name, BAD_CAST"type")) {
			/* don't output anything */
			
		} else if(!xmlStrcmp(attr->name, BAD_CAST"url")) {		
			tmp = g_strdup_printf("<a href=\"%s\">%s</a>", value, value);
			addToHTMLBuffer(&buffer, tmp);
			g_free(tmp);
			
		} else {		
			tmp = g_strdup_printf("<p>%s : %s\n</p>", (gchar *)attr->name, value);
			addToHTMLBuffer(&buffer, tmp);
			g_free(tmp);
		}
		
		xmlFree(value);
		attr = attr->next;
	}
	addToHTMLBuffer(&buffer, "<br><br>");
			
	/* check for <outline> subtags */
	if(NULL != cur->xmlChildrenNode) {
		addToHTMLBuffer(&buffer, "<ul>");
		cur = cur->xmlChildrenNode;
		while(NULL != cur) {
			if(!xmlStrcmp(cur->name, BAD_CAST"outline")) {
				tmp = g_strdup_printf("<li style=\"padding-left:15px;\">%s</li>", getOutlineContents(cur));
				addToHTMLBuffer(&buffer, tmp);
				g_free(tmp);
			}

			cur = cur->next;
		}
		addToHTMLBuffer(&buffer, "</ul>");
	}

	return buffer;
}

static void readOPML(feedPtr fp) {
	xmlDocPtr 	doc;
	xmlNodePtr 	cur, child;
	itemPtr		ip;
	gchar		*encoding;
	gchar		*buffer, *tmp;
	gchar		*headTags[OPML_MAX_TAG];
	int 		i, error = 0;

	while(1) {
		doc = xmlRecoverMemory(fp->data, strlen(fp->data));
		
		if(NULL == doc) {
			print_status(g_strdup_printf(_("XML error wile reading feed! Feed \"%s\" could not be loaded!"), fp->source));
			error = 1;
			break;
		}

		if(NULL == (cur = xmlDocGetRootElement(doc))) {
			print_status(_("Empty document!"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		if(!xmlStrcmp(cur->name, BAD_CAST"opml") ||
		   !xmlStrcmp(cur->name, BAD_CAST"oml") ||
		   !xmlStrcmp(cur->name, BAD_CAST"outlineDocument")) {
		   	// nothing
		} else {
			print_status(_("Could not find OPML header!!"));
			xmlFreeDoc(doc);
			error = 1;
			break;			
		}

		cur = cur->xmlChildrenNode;
		while (cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}

		memset(headTags, 0, sizeof(gchar *)*OPML_MAX_TAG);		
		while (cur != NULL) {
			if(!xmlStrcmp(cur->name, BAD_CAST"head")) {
				/* check for <head> tags */
				child = cur->xmlChildrenNode;
				while(child != NULL) {
					for(i = 0; i < OPML_MAX_TAG; i++) {
						if (!xmlStrcmp(child->name, (const xmlChar *)opmlTagList[i])) {
							tmp = headTags[i];
							if(NULL == (headTags[i] = g_strdup(xmlNodeListGetString(doc, child->xmlChildrenNode, 1)))) {
								headTags[i] = tmp;
							} else {
								g_free(tmp);
							}
						}		
					}
					child = child->next;
				}
			}
			
			if(!xmlStrcmp(cur->name, BAD_CAST"body")) {
				/* process all <outline> tags */
				child = cur->xmlChildrenNode;
				while(child != NULL) {
					if(!xmlStrcmp(child->name, BAD_CAST"outline")) {
						buffer = NULL;
						addToHTMLBuffer(&buffer, tmp = getOutlineContents(child));
						g_free(tmp);
						
						ip = getNewItemStruct();
						ip->title = xmlGetNoNsProp(child, BAD_CAST"text");
						ip->description = buffer;
						ip->readStatus = TRUE;
						addItem(fp, ip);
					}
					child = child->next;
				}
			}
			
			cur = cur->next;
		}
		xmlFreeDoc(doc);

		/* after parsing we fill in the infos into the feedPtr structure */		
		fp->type = FST_OPML;
		fp->updateInterval = fp->updateCounter = -1;
		fp->title = headTags[OPML_TITLE];
		
		if(0 == error) {
			/* prepare HTML output */
			buffer = NULL;
			addToHTMLBuffer(&buffer, FEED_HEAD_START);	
			addToHTMLBuffer(&buffer, FEED_HEAD_CHANNEL);
			addToHTMLBuffer(&buffer, headTags[OPML_TITLE]);
			addToHTMLBuffer(&buffer, HTML_NEWLINE);	
			addToHTMLBuffer(&buffer, FEED_HEAD_SOURCE);	
			if(NULL != fp->source) {
				tmp = g_strdup_printf("<a href=\"%s\">%s</a>", fp->source, fp->source);
				addToHTMLBuffer(&buffer, tmp);
				g_free(tmp);
			}

			addToHTMLBuffer(&buffer, FEED_HEAD_END);	

			addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_START);
			FEED_FOOT_WRITE(buffer, "title",		headTags[OPML_TITLE]);
			FEED_FOOT_WRITE(buffer, "creation date",	headTags[OPML_CREATED]);
			FEED_FOOT_WRITE(buffer, "last modified",	headTags[OPML_MODIFIED]);
			FEED_FOOT_WRITE(buffer, "owner name",		headTags[OPML_OWNERNAME]);
			FEED_FOOT_WRITE(buffer, "owner email",		headTags[OPML_OWNEREMAIL]);
			addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_END);
			
			fp->description = buffer;
			fp->available = TRUE;
		} else
			fp->title = g_strdup(fp->source);
		break;
	}
}

/* ---------------------------------------------------------------------------- */
/* initialization								*/
/* ---------------------------------------------------------------------------- */

feedHandlerPtr initOPMLFeedHandler(void) {
	feedHandlerPtr	fhp;
	
	if(NULL == (fhp = (feedHandlerPtr)g_malloc(sizeof(struct feedHandler)))) {
		g_error(_("not enough memory!"));
	}
	memset(fhp, 0, sizeof(struct feedHandler));
	
	/* prepare feed handler structure */
	fhp->readFeed		= readOPML;
	fhp->merge		= FALSE;
	
	return fhp;
}
