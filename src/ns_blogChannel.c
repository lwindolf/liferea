/**
 * @file ns_blogChannel.c blogChannel namespace support
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

#include "htmlview.h"
#include "ns_blogChannel.h"
#include "common.h"
#include "update.h"
#include <string.h>

#define BLOGROLL_START		"<p><div class=\"blogchanneltitle\"><b>BlogRoll</b></div></p>"
#define BLOGROLL_END		"" 
#define MYSUBSCR_START		"<p><div class=\"blogchanneltitle\"><b>Authors Subscriptions</b></div></p>"
#define MYSUBSCR_END		""
#define BLINK_START		"<p><div class=\"blogchanneltitle\"><b>Promoted Weblog</b></div></p>"
#define BLINK_END		""

/* the spec at Userland http://backend.userland.com/blogChannelModule

   blogChannel contains of four channel tags
   - blogRoll
   - mySubscriptions
   - blink
   - changes	(ignored)
*/

/* returns a HTML string containing the text and attributes of the outline */
static gchar * getOutlineContents(xmlNodePtr cur) {
	gchar		*buffer = NULL;
	gchar		*tmp, *value;

	if(NULL != (value = utf8_fix(xmlGetNoNsProp(cur, BAD_CAST"text")))) {
		addToHTMLBuffer(&buffer, value);
		g_free(value);
	}
	
	if(NULL != (value = utf8_fix(xmlGetNoNsProp(cur, BAD_CAST"url")))) {
		tmp = g_strdup_printf("&nbsp;<a href=\"%s\">%s</a>", value, value);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		g_free(value);
	}

	if(NULL != (value = utf8_fix(xmlGetNoNsProp(cur, BAD_CAST"htmlUrl")))) {
		tmp = g_strdup_printf("&nbsp;(<a href=\"%s\">HTML</a>)", value);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		g_free(value);
	}
			
	if(NULL != (value = utf8_fix(xmlGetNoNsProp(cur, BAD_CAST"xmlUrl")))) {
		tmp = g_strdup_printf("&nbsp;(<a href=\"%s\">XML</a>)", value);
		addToHTMLBuffer(&buffer, tmp);
		g_free(tmp);
		g_free(value);
	}		

	return buffer;
}

/* simple function to retrieve an OPML document and 
   parse and output all depth 1 outline tags as
   HTML into a buffer */
static gchar * getOutlineList(gchar *url) {
	struct request	*request;
	xmlDocPtr 		doc = NULL;
	xmlNodePtr 		cur;
	gchar			*tmp, *buffer;
	
	request = download_request_new();
	request->source = g_strdup(url);
	download_process(request);

	if (request->data == NULL) {
		download_request_free(request);
		return NULL;
	}

	buffer = NULL;	/* the following code somewhat duplicates opml.c */
	while(1) {
		doc = xmlRecoverMemory(request->data, request->size);
		
		if(NULL == doc)
			break;
			
		if(NULL == (cur = xmlDocGetRootElement(doc)))
			break;

		if(!xmlStrcmp(cur->name, BAD_CAST"opml") ||
		   !xmlStrcmp(cur->name, BAD_CAST"oml") ||
		   !xmlStrcmp(cur->name, BAD_CAST"outlineDocument")) {
		   	/* nothing */
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

	download_request_free(request);		
	return buffer;
}

static void parse_channel_tag(feedPtr fp, xmlNodePtr cur) {
	xmlChar		*string;
	gchar		*buffer = NULL;
	gchar		*output, *tmp;
	
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
			addToHTMLBuffer(&buffer, MYSUBSCR_START);
			addToHTMLBuffer(&buffer, output);
			addToHTMLBuffer(&buffer, MYSUBSCR_END);
			g_free(output);
		}
	} else if(!xmlStrcmp("blink", cur->name)) {
		tmp = utf8_fix(string);
		string = NULL;
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
		g_hash_table_insert(fp->tmpdata, g_strdup_printf("bC:%s", cur->name), buffer);
		buffer = NULL;
		addToHTMLBuffer(&buffer, g_hash_table_lookup(fp->tmpdata, "bC:blink"));
		addToHTMLBuffer(&buffer, g_hash_table_lookup(fp->tmpdata, "bC:blogRoll"));
		addToHTMLBuffer(&buffer, g_hash_table_lookup(fp->tmpdata, "bC:mySubscriptions"));
		metadata_list_set(&(fp->metadata), "blogChannel", buffer);
		g_free(buffer);
	}
}

static void ns_blogChannel_insert_ns_uris(NsHandler *nsh, GHashTable *hash) {
	g_hash_table_insert(hash, "http://backend.userland.com/blogChannelModule", nsh);
}

NsHandler *ns_bC_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->insertNsUris = ns_blogChannel_insert_ns_uris;
	nsh->prefix			= "blogChannel";
	nsh->parseChannelTag		= parse_channel_tag;

	return nsh;
}
