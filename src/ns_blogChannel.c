/*
   blogChannel namespace support
   
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

#include <string.h> /* For strlen() */
#include "htmlview.h"
#include "netio.h"
#include "ns_blogChannel.h"
#include "common.h"

#define BLOGROLL_START		"<div class=\"blogchanneltitle\"><b>BlogRoll</b></div<br>"
#define BLOGROLL_END		"<br>" 
#define MYSUBSCR_START		"<div class=\"blogchanneltitle\"><b>Authors Subscriptions</b></div<br>"
#define MYSUBSCR_END		"<br>"
#define BLINK_START		"<div class=\"blogchanneltitle\"><b>Promoted Weblog</b></div><br>"
#define BLINK_END		"<br>"

static gchar ns_bC_prefix[] = "blogChannel";

/* the spec at Userland http://backend.userland.com/blogChannelModule

   blogChannel contains of four channel tags
   - blogRoll
   - mySubscriptions
   - blink
   - changes	(ignored)
*/

gchar * ns_bC_getRSSNsPrefix(void) { return ns_bC_prefix; }

/* retruns a HTML string containing the text and attributes of the outline */
static gchar * getOutlineContents(xmlNodePtr cur) {
	gchar		*buffer = NULL;
	gchar		*tmp, *value;

	if(NULL != (value = xmlGetNoNsProp(cur, BAD_CAST"text"))) {
		addToHTMLBuffer(&buffer, CONVERT(value));
		xmlFree(value);
	}
	
	if(NULL != (value = xmlGetNoNsProp(cur, BAD_CAST"url"))) {
		value = CONVERT(value);
		tmp = g_strdup_printf("&nbsp;<a href=\"%s\">%s</a>", value, value);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		xmlFree(value);
	}

	if(NULL != (value = xmlGetNoNsProp(cur, BAD_CAST"htmlUrl"))) {
		value = CONVERT(value);
		tmp = g_strdup_printf("&nbsp;(<a href=\"%s\">HTML</a>)", value);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		xmlFree(value);
	}
			
	if(NULL != (value = xmlGetNoNsProp(cur, BAD_CAST"xmlUrl"))) {
		value = CONVERT(value);
		tmp = g_strdup_printf("&nbsp;(<a href=\"%s\">XML</a>)", value);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		xmlFree(value);
	}		

	return buffer;
}

/* simple function to retrieve an OPML document and 
   parse and output all depth 1 outline tags as
   HTML into a buffer */
static gchar * getOutlineList(gchar *url) {
	struct feed_request	request;
	xmlDocPtr 		doc = NULL;
	xmlNodePtr 		cur;
	gchar			*data, *tmp, *buffer;
	
	request.feedurl = g_strdup(url);
	data = downloadURL(&request);
	g_free(request.feedurl);
	g_free(request.lastmodified);
	if(NULL == data)
		return NULL;

	buffer = NULL;	/* the following code somewhat duplicates opml.c */
	while(1) {
		doc = xmlRecoverMemory(data, strlen(data));
		g_free(data);
		
		if(NULL == doc)
			break;
			
		if(NULL == (cur = xmlDocGetRootElement(doc)))
			break;

		if(!xmlStrcmp(cur->name, BAD_CAST"opml") ||
		   !xmlStrcmp(cur->name, BAD_CAST"oml") ||
		   !xmlStrcmp(cur->name, BAD_CAST"outlineDocument")) {
		   	// nothing
		} else
			break;
	
		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			if(!xmlStrcmp(cur->name, BAD_CAST"body")) {
				/* process all <outline> tags */
				cur = cur->xmlChildrenNode;
				while(cur != NULL) {
					if(!xmlStrcmp(cur->name, BAD_CAST"outline")) {
						addToHTMLBuffer(&buffer, tmp = getOutlineContents(cur));
						addToHTMLBuffer(&buffer, "<br>");
						g_free(tmp);
					}
					cur = cur->next;
				}
				break;
			}
			cur = cur->next;
		}
		break;		
	}

	if(NULL != doc)
		xmlFreeDoc(doc);
		
	return buffer;
}

static void ns_bC_parseChannelTag(RSSChannelPtr cp, xmlNodePtr cur) {
	xmlChar		*string;
	gchar		*buffer = NULL;
	gchar		*key, *output, *tmp;
	
	string = xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
	  
	if(!xmlStrcmp("blogRoll", cur->name)) {	
		if(NULL != (output = getOutlineList(string))) {
			addToHTMLBuffer(&buffer, BLOGROLL_START);
			addToHTMLBuffer(&buffer, output);
			addToHTMLBuffer(&buffer, BLOGROLL_END);
			g_free(output);
		}
	} else if(!xmlStrcmp("mySubscriptions", cur->name)) {
		if(NULL != (output = getOutlineList(string))) {
			addToHTMLBuffer(&buffer, BLOGROLL_START);
			addToHTMLBuffer(&buffer, output);
			addToHTMLBuffer(&buffer, BLOGROLL_END);
			g_free(output);
		}
	} else if(!xmlStrcmp("blink", cur->name)) {
		tmp = CONVERT(string);
		output = g_strdup_printf("<p><a href=\"%s\">%s</a></p>", tmp, tmp);
		g_free(tmp);
		addToHTMLBuffer(&buffer, BLINK_START);
		addToHTMLBuffer(&buffer, output);
		addToHTMLBuffer(&buffer, BLINK_END);
		g_free(output);
	}

	if(NULL != string)
		xmlFree(string);	

	if(NULL != buffer) {
		key = g_strdup_printf("cC:%s", cur->name);
		if(NULL != g_hash_table_lookup(cp->nsinfos, key)) 
			g_hash_table_remove(cp->nsinfos, key);	
		g_hash_table_insert(cp->nsinfos, key, buffer);
	}
}

static void ns_bC_doOutput(GHashTable *nsinfos, gchar **buffer, gchar *tagname) {
	gchar		*output;
	gchar		*key;
	
	g_assert(NULL != nsinfos);
	key = g_strdup_printf("cC:%s", tagname);
	
	if(NULL != (output = g_hash_table_lookup(nsinfos, key))) {
		addToHTMLBuffer(buffer, output);
		g_hash_table_remove(nsinfos, key);
	}
	g_free(key);
}

static gchar * ns_bC_doChannelOutput(gpointer obj) {
	gchar	*buffer = NULL;
	
	if(NULL != obj) {
		ns_bC_doOutput(((RSSChannelPtr)obj)->nsinfos, &buffer, "blink");
		ns_bC_doOutput(((RSSChannelPtr)obj)->nsinfos, &buffer, "blogRoll");
		ns_bC_doOutput(((RSSChannelPtr)obj)->nsinfos, &buffer, "mySubscriptions");
	}
	return buffer;
}

RSSNsHandler *ns_bC_getRSSNsHandler(void) {
	RSSNsHandler 	*nsh;
	
	if(NULL != (nsh = (RSSNsHandler *)g_malloc(sizeof(RSSNsHandler)))) {
		nsh->parseChannelTag		= ns_bC_parseChannelTag;
		nsh->parseItemTag		= NULL;
		nsh->doChannelHeaderOutput	= NULL;
		nsh->doChannelFooterOutput	= ns_bC_doChannelOutput;
		nsh->doItemHeaderOutput		= NULL;
		nsh->doItemFooterOutput		= NULL;
	}
	return nsh;
}
