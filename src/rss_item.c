/**
 * @file rss_item.c RSS/RDF item parsing 
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
#include "rss_item.h"
#include "htmlview.h"
#include "metadata.h"

#define RDF_NS	BAD_CAST"http://www.w3.org/1999/02/22-rdf-syntax-ns#"

extern GHashTable *RssToMetadataMapping;

/* uses the same namespace handler as rss_channel */
extern GSList		*rss_nslist;
extern GHashTable	*rss_nstable;

/* method to parse standard tags for each item element */
itemPtr parseRSSItem(feedPtr fp, xmlNodePtr cur) {
	gchar			*tmp, *tmp2, *tmp3, *link;
	GSList			*hp;
	NsHandler		*nsh;
	parseItemTagFunc	pf;
	itemPtr			ip;

	g_assert(NULL != cur);
		
	ip = item_new();
	ip->tmpdata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	
	/* try to get an item about id */
	tmp = xmlGetProp(cur, BAD_CAST"about");
	item_set_id(ip, tmp);
	g_free(tmp);
	
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if(cur->type != XML_ELEMENT_NODE || cur->name == NULL)
			;
		/* check namespace of this tag */
		else if(NULL != cur->ns) {		
			if(NULL != cur->ns->prefix) {
				g_assert(NULL != rss_nslist);
				if(NULL != (hp = (GSList *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix))) {
					nsh = (NsHandler *)hp->data;
					pf = nsh->parseItemTag;
					if(NULL != pf)
						(*pf)(ip, cur);				
				} else {
					/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
				}
			}
		}
		/* check for metadata tags */
		else if((tmp2 = g_hash_table_lookup(RssToMetadataMapping, cur->name)) != NULL) {
			tmp3 = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE));
			if(tmp3 != NULL) {
				ip->metadata = metadata_list_append(ip->metadata, tmp2, tmp3);
				g_free(tmp3);
			}
		}
		/* check for specific tags */
		else if(!xmlStrcmp(cur->name, BAD_CAST"pubDate")) {
 			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp) {
				item_set_time(ip, parseRFC822Date(tmp));
				g_free(tmp);
			}
		} 
		else if(!xmlStrcmp(cur->name, BAD_CAST"enclosure")) {
			/* RSS 0.93 allows multiple enclosures, so we build
			   a simple string of HTML-links... */
			tmp = utf8_fix(xmlGetNoNsProp(cur, BAD_CAST"url"));
			if(NULL != tmp) {
				ip->metadata = metadata_list_append(ip->metadata, "enclosure", tmp);
				g_free(tmp);
			}
		} 
		else if(!xmlStrcmp(cur->name, BAD_CAST"guid")) {
			if(NULL == item_get_id(ip)) {
				tmp = xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
				if(NULL != tmp) {
					item_set_id(ip, tmp);
					xmlFree(tmp);
				}
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
 			tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)));
 			if(NULL != tmp) {
				item_set_title(ip, tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
 			tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)));
 			if(NULL != tmp) {
				item_set_source(ip, tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"description")) {
 			tmp = convertToHTML(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)));
 			if(NULL != tmp) {
				item_set_description(ip, tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"source")) {
			gchar *source_url = utf8_fix(xmlGetNoNsProp(cur, BAD_CAST"url"));
			if(NULL != source_url) {
				gchar *source_title = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
				gchar *tmp = g_strdup_printf("<a href=\"%s\">%s</a>", source_url, 
									    (NULL != source_title)?source_title:source_url);
				ip->metadata = metadata_list_append(ip->metadata, "itemSource", tmp);
				g_free(source_url);
				g_free(source_title);
				g_free(tmp);
			}
		}
		cur = cur->next;
	}

	item_set_read_status(ip, FALSE);
	g_hash_table_destroy(ip->tmpdata);
	return ip;
}
