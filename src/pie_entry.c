/**
 * @file pie_entry.c Atom/Echo/PIE 0.2/0.3 entry parsing 
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

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "support.h"
#include "common.h"
#include "pie_entry.h"
#include "pie_ns.h"
#include "htmlview.h"

/* structure for the hashtable callback which itself calls the 
   namespace output handler */
#define OUTPUT_PIE_FEED_NS_HEADER	0
#define	OUTPUT_PIE_FEED_NS_FOOTER	1
#define OUTPUT_ITEM_NS_HEADER		2
#define OUTPUT_ITEM_NS_FOOTER		3
typedef struct {
	gint		type;
	gchar		**buffer;	/* pointer to output char buffer pointer */
	gpointer	obj;		/* thats either a PIEFeedPtr or a PIEEntryPtr 
					   depending on the type value */
} outputRequest;

/* uses the same namespace handler as PIE_channel */
extern GHashTable *pie_nslist;

static gchar *entryTagList[] = {	"title",
					"description",
					"copyright",
					"id",
					NULL
				  };
				  
/* we reuse some pie_feed.c function */
extern gchar * parseAuthor(xmlNodePtr cur);
extern void showPIEFeedNSInfo(gpointer key, gpointer value, gpointer userdata);

/* writes item description as HTML into the gtkhtml widget */
static gchar * showPIEEntry(PIEFeedPtr cp, PIEEntryPtr ip) {
	gchar		*tmp, *buffer = NULL;	
	outputRequest	request;

	g_assert(NULL != ip);	
	g_assert(NULL != cp);
	
	if(NULL != ip->source) {
		addToHTMLBuffer(&buffer, ITEM_HEAD_START);
		
		addToHTMLBuffer(&buffer, ITEM_HEAD_CHANNEL);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>", 
			cp->tags[PIE_FEED_LINK],
			cp->tags[PIE_FEED_TITLE]);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		
		addToHTMLBuffer(&buffer, HTML_NEWLINE);
		
		addToHTMLBuffer(&buffer, ITEM_HEAD_ITEM);
		tmp = g_strdup_printf("<a href=\"%s\">%s</a>",
			ip->source,
			ip->tags[PIE_ENTRY_TITLE]);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		
		addToHTMLBuffer(&buffer, ITEM_HEAD_END);	
	}	

	/* process namespace infos */
	request.obj = ip;
	request.buffer = &buffer;
	request.type = OUTPUT_ITEM_NS_HEADER;	
	if(NULL != pie_nslist)
		g_hash_table_foreach(pie_nslist, showPIEFeedNSInfo, (gpointer)&request);

	if(NULL != ip->tags[PIE_ENTRY_DESCRIPTION])
		addToHTMLBuffer(&buffer, ip->tags[PIE_ENTRY_DESCRIPTION]);

	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_START);
	FEED_FOOT_WRITE(buffer, "author",		ip->author);
	FEED_FOOT_WRITE(buffer, "contributors",		ip->contributors);
	FEED_FOOT_WRITE(buffer, "copyright",		ip->tags[PIE_ENTRY_COPYRIGHT]);
	addToHTMLBuffer(&buffer, FEED_FOOT_TABLE_END);

	request.type = OUTPUT_ITEM_NS_FOOTER;
	if(NULL != pie_nslist)
		g_hash_table_foreach(pie_nslist, showPIEFeedNSInfo, (gpointer)&request);
	
	return buffer;
}

/* <content> tag support, FIXME: base64 not supported */
static void parseContent(xmlNodePtr cur, PIEEntryPtr i) {
	gchar	*mode, *type, *tmp;

	g_assert(NULL != cur);
	if((NULL == i->tags[PIE_ENTRY_DESCRIPTION]) || (TRUE == i->summary)) {	
		/* determine encoding mode */
		mode = CONVERT(xmlGetNoNsProp(cur, BAD_CAST"mode"));
		if(NULL != mode) {
			if(!strcmp(mode, "escaped")) {
 				tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					g_free(i->tags[PIE_ENTRY_DESCRIPTION]);
					i->tags[PIE_ENTRY_DESCRIPTION] = tmp;
					i->summary = FALSE;
				}
			} else if(!strcmp(mode, "xml")) {
				g_free(i->tags[PIE_ENTRY_DESCRIPTION]);
				i->tags[PIE_ENTRY_DESCRIPTION] = extractHTMLNode(cur);
				i->summary = FALSE;

			} else if(!strcmp(mode, "base64")) {
				g_warning("Base64 encoded <content> in Atom feeds not supported!\n");

			} else if(!strcmp(mode, "multipart/alternative")) {
				if(NULL != cur->xmlChildrenNode)
					parseContent(cur->xmlChildrenNode, i);
			}
			g_free(mode);
		} else {
			/* some feeds don'ts specify a mode but a MIME 
			   type in the type attribute... */
			type = CONVERT(xmlGetNoNsProp(cur, BAD_CAST"type"));			
			/* not sure what MIME types are necessary... */
			if((NULL == type) ||
			   !strcmp(type, "text/html") ||
			   !strcmp(type, "application/xhtml+xml")) {
				g_free(i->tags[PIE_ENTRY_DESCRIPTION]);
				i->tags[PIE_ENTRY_DESCRIPTION] = extractHTMLNode(cur);
				i->summary = FALSE;
			}
			g_free(type);
		}
	}
}

/* method to parse standard tags for each item element */
itemPtr parseEntry(gpointer cp, xmlNodePtr cur) {
	xmlChar			*xtmp;
	gchar			*tmp2, *tmp;
	parseEntryTagFunc	fp;
	PIENsHandler		*nsh;	
	PIEEntryPtr 		i;
	itemPtr			ip;
	int			j;

	g_assert(NULL != cur);
		
	i = g_new0(struct PIEEntry, 1);
	i->nsinfos = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ip = item_new();
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if(NULL == cur->name) {
			g_warning("invalid XML: parser returns NULL value -> tag ignored!");
			cur = cur->next;
			continue;
		}
		
		/* check namespace of this tag */
		if(NULL != cur->ns) {		
			if (NULL != cur->ns->prefix) {
				g_assert(NULL != pie_nslist);
				if(NULL != (nsh = (PIENsHandler *)g_hash_table_lookup(pie_nslist, (gpointer)cur->ns->prefix))) {
					fp = nsh->parseItemTag;
					if(NULL != fp)
						(*fp)(i, cur);
					cur = cur->next;
					continue;						
				} else {
					g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);
				}
			}
		}

		if(!xmlStrcmp(cur->name, BAD_CAST"issued")) {
			// FIXME: is <modified> or <issued> or <created> the time tag we want to display?
 			tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
 			if(NULL != tmp)
				i->time = parseISO8601Date(tmp);

		} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
			/* 0.2 link : element content is the link
			   0.3 link : rel, type and href attribute */
			if(NULL != (xtmp = xmlGetProp(cur, BAD_CAST"rel"))) {
				if(!xmlStrcmp(xtmp, BAD_CAST"alternate")) {
					g_free(i->source);
					i->source = CONVERT(xmlGetProp(cur, BAD_CAST"href"));
				}
				xmlFree(xtmp);
			} else {
				tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL == tmp) {
					g_free(i->source);
					i->source = tmp;
				}				
			}
		
		} else if(!xmlStrcmp(cur->name, BAD_CAST"author")) {
			/* parse feed author */
			g_free(i->author);
			i->author = parseAuthor(cur);

		} else if(!xmlStrcmp(cur->name, BAD_CAST"contributor")) {
			/* parse feed contributors */
			tmp = parseAuthor(cur);				
			if(NULL != i->contributors) {
				/* add another contributor */
				tmp2 = g_strdup_printf("%s<br>%s", i->contributors, tmp);
				g_free(i->contributors);
				g_free(tmp);
				tmp = tmp2;
			}
			i->contributors = tmp;
		
		/* <content> support */
		} else if(!xmlStrcmp(cur->name, BAD_CAST"content")) {
			parseContent(cur, i);
			
		} else if(!xmlStrcmp(cur->name, BAD_CAST"summary")) {			
			/* <summary> can be used for short text descriptions, if there is no
			   <content> description we show the <summary> content */
			if(NULL == i->tags[PIE_ENTRY_DESCRIPTION]) {
				tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					i->summary = TRUE;
					i->tags[PIE_ENTRY_DESCRIPTION] = tmp;
				}
			}
		} else {				
			/* check for PIE tags */
			for(j = 0; j < PIE_ENTRY_MAX_TAG; j++) {
				if(!xmlStrcmp(cur->name, BAD_CAST entryTagList[j])) {
					tmp = CONVERT(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
					if(NULL != tmp) {
						g_free(i->tags[j]);
						i->tags[j] = tmp;
					}
				}		
			}
		}
		cur = cur->next;
	}

	/* after parsing we fill the infos into the itemPtr structure */
	ip->type = FST_PIE;
	ip->time = i->time;
	ip->source = i->source;
	ip->readStatus = FALSE;
	ip->id = i->tags[PIE_ENTRY_ID];

	/* some postprocessing */
	if(NULL != i->tags[PIE_ENTRY_TITLE])
		i->tags[PIE_ENTRY_TITLE] = unhtmlize(i->tags[PIE_ENTRY_TITLE]);
		
	if(NULL != i->tags[PIE_ENTRY_DESCRIPTION])
		i->tags[PIE_ENTRY_DESCRIPTION] = convertToHTML(i->tags[PIE_ENTRY_DESCRIPTION]);	

	ip->title = g_strdup(i->tags[PIE_ENTRY_TITLE]);
	ip->description = showPIEEntry((PIEFeedPtr)cp, i);

	/* free PIEEntry structure */
	for(j = 0; j < PIE_ENTRY_MAX_TAG; j++)
		g_free(i->tags[j]);
	g_hash_table_destroy(i->nsinfos);
	g_free(i);
	
	return ip;
}
