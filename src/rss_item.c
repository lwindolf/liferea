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
#include "rss_ns.h"
#include "htmlview.h"
#include "metadata.h"

#define RDF_NS	BAD_CAST"http://www.w3.org/1999/02/22-rdf-syntax-ns#"

#define	START_ENCLOSURE	"<div style=\"margin-top:5px;margin-bottom:5px;padding-left:5px;padding-right:5px;border-color:black;border-style:solid;border-width:1px;background-color:#E0E0E0\"> enclosed file: "
#define	END_ENCLOSURE	"</div>"

/* uses the same namespace handler as rss_channel */
extern GSList		*rss_nslist;
extern GHashTable	*rss_nstable;

static gchar *itemTagList[] = {		"title",
					"description",
					"link",
					"author",
					"comments",
					"category",
					"guid",
					NULL
				  };

/* method to parse standard tags for each item element */
itemPtr parseRSSItem(feedPtr fp, xmlNodePtr cur) {
	gchar			*tmp, *link;
	parseItemTagFunc	parseFunc;
	GSList			*hp;
	NsHandler		*nsh;
	RSSItemPtr 		i;
	itemPtr			ip;
	int			j;

	g_assert(NULL != cur);
		
	i = g_new0(struct RSSItem, 1);
	ip = item_new();
	ip->tmpdata = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
	
	/* try to get an item about id */
	tmp =  xmlGetProp(cur, BAD_CAST"about");
	item_set_id(ip, tmp);
	g_free(tmp);
	
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if(NULL == cur->name) {
			g_warning("invalid XML: parser returns NULL value -> tag ignored!");
			cur = cur->next;
			continue;
		}
		
		/* check namespace of this tag */
		if(NULL != cur->ns) {		
			if(NULL != cur->ns->prefix) {
				g_assert(NULL != rss_nslist);
				if(NULL != (hp = (GSList *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix))) {
					nsh = (NsHandler *)hp->data;
					parseFunc = nsh->parseItemTag;
					if(NULL != parseFunc)
						(*parseFunc)(ip, cur);
					cur = cur->next;
					continue;						
				} else {
					/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
				}
			}
		}
		
		if(!xmlStrcmp(cur->name, BAD_CAST"pubDate")) {
 			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp) {
				i->time = parseRFC822Date(tmp);
				g_free(tmp);
			}
		} else 
		if(!xmlStrcmp(cur->name, BAD_CAST"enclosure")) {
			/* RSS 0.93 allows multiple enclosures, so we build
			   a simple string of HTML-links... */
			tmp = utf8_fix(xmlGetNoNsProp(cur, BAD_CAST"url"));
			if(NULL != tmp) {
				link = tmp;
				if(NULL == (tmp = i->enclosure)) {
					i->enclosure = g_strdup_printf("<a href=\"%s\">%s</a>", link, link);					
				} else {
					i->enclosure = g_strdup_printf("%s<a href=\"%s\">%s</a>", tmp, link, link);
					g_free(tmp);
				}
				g_free(link);
			}
		} else 
		if(!xmlStrcmp(cur->name, BAD_CAST"source")) {
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
		} else {
			/* check for RDF tags */
			for(j = 0; j < RSS_ITEM_MAX_TAG; j++) {
				if(!xmlStrcmp(cur->name, BAD_CAST itemTagList[j])) {
 					tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
					if(NULL != tmp) {
						g_free(i->tags[j]);
						i->tags[j] = tmp;
						break;
					}				
				}		
			}
		}
		cur = cur->next;
	}

	/* after parsing we fill the infos into the itemPtr structure */
	item_set_time(ip, i->time);
	item_set_source(ip, i->tags[RSS_ITEM_LINK]);
	item_set_read_status(ip, FALSE);

	if(NULL == item_get_id(ip))
		item_set_id(ip, i->tags[RSS_ITEM_GUID]);

	/* some postprocessing before generating HTML */
	if(NULL != i->tags[RSS_ITEM_TITLE])
		i->tags[RSS_ITEM_TITLE] = unhtmlize(i->tags[RSS_ITEM_TITLE]);
		
	if(NULL != i->tags[RSS_ITEM_DESCRIPTION])
		i->tags[RSS_ITEM_DESCRIPTION] = convertToHTML(i->tags[RSS_ITEM_DESCRIPTION]);

	item_set_title(ip, i->tags[RSS_ITEM_TITLE]);		
	item_set_description(ip, "");	// FIXME: fix this and ensure that ns_content.c works!!!!!!
	g_free(tmp);
	
	/* free RSSItem structure */
	for(j = 0; j < RSS_ITEM_MAX_TAG; j++)
		g_free(i->tags[j]);
	
	g_free(i->enclosure);
	g_hash_table_destroy(ip->tmpdata);
	g_free(i);
	
	return ip;
}
