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
extern GHashTable	*rss_nstable;
extern GHashTable	*ns_rss_ns_uri_table;

/* method to parse standard tags for each item element */
itemPtr parseRSSItem(feedPtr fp, xmlNodePtr cur) {
	gchar			*tmp, *tmp2, *tmp3;
	NsHandler		*nsh;
	parseItemTagFunc	pf;
	itemPtr			ip;
	
	g_assert(NULL != cur);

	ip = item_new();
	ip->tmpdata = g_hash_table_new_full(g_str_hash, g_str_equal, NULL, g_free);
	
	/* try to get an item about id */
	if(NULL != (tmp = xmlGetProp(cur, BAD_CAST"about"))) {
		item_set_id(ip, tmp);
		item_set_source(ip, tmp);
		g_free(tmp);
	}
	
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if(cur->type != XML_ELEMENT_NODE || cur->name == NULL) {
			cur = cur->next;
			continue;
		}
		
		/* check namespace of this tag */
		if(NULL != cur->ns) {
			if(((cur->ns->href != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(ns_rss_ns_uri_table, (gpointer)cur->ns->href))) ||
			   ((cur->ns->prefix != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix)))) {
				if(NULL != (nsh = (NsHandler *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix))) {
					pf = nsh->parseItemTag;
					if(NULL != pf)
						(*pf)(ip, cur);	
					cur = cur->next;
					continue;
				} else {
					/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
				}
			}
		} /* explicitly no following else!!! */
		
		/* check for metadata tags */
		if((tmp2 = g_hash_table_lookup(RssToMetadataMapping, cur->name)) != NULL) {
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
			/* RSS 0.93 allows multiple enclosures */
			tmp = utf8_fix(xmlGetProp(cur, BAD_CAST"url"));
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
					tmp2 = xmlGetProp(cur, "isPermaLink");
					if(tmp2 == NULL || !xmlStrcmp(tmp2, BAD_CAST"true"))
						item_set_source(ip, tmp); /* Per the RSS 2.0 spec. */
					if (tmp2 != NULL)
						xmlFree(tmp2);
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
			tmp = utf8_fix(xmlGetProp(cur, BAD_CAST"url"));
			if(NULL != tmp) {
				item_set_real_source_url(ip, tmp);
				g_free(tmp);
			}
			tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
			if(NULL != tmp) {
				item_set_real_source_title(ip, tmp);
				g_free(tmp);
			}
		}
		
		cur = cur->next;
	}

	item_set_read_status(ip, FALSE);

	g_hash_table_destroy(ip->tmpdata);
	ip->tmpdata = NULL;
	return ip;
}
