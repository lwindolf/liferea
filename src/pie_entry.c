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
#include "pie_feed.h"
#include "htmlview.h"
#include "metadata.h"

/* uses the same namespace handler as PIE_channel */
extern GHashTable *pie_nstable;
extern GHashTable *ns_pie_ns_uri_table;

/* we reuse some pie_feed.c function */
extern gchar * parseAuthor(xmlNodePtr cur);

/* <content> tag support, FIXME: base64 not supported */
/* method to parse standard tags for each item element */
itemPtr parseEntry(feedPtr fp, xmlNodePtr cur) {
	xmlChar			*xtmp;
	gchar			*tmp2, *tmp;
	itemPtr			ip;
	GHashTable *nsinfos;
	NsHandler		*nsh;
	parseItemTagFunc	pf;
	
	g_assert(NULL != cur);
		
	nsinfos = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
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
			if(((cur->ns->href != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(ns_pie_ns_uri_table, (gpointer)cur->ns->href))) ||
			   ((cur->ns->prefix != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(pie_nstable, (gpointer)cur->ns->prefix)))) {
				
				pf = nsh->parseItemTag;
				if(NULL != pf)
					(*pf)(ip, cur);
				cur = cur->next;
				continue;
			} else {
				/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
			}
		} /* explicitly no following else !!! */
		
		if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
			tmp = unhtmlize(utf8_fix(pie_parse_content_construct(cur)));
			if (tmp != NULL)
				item_set_title(ip, tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
			if(NULL != (tmp2 = utf8_fix(xmlGetProp(cur, BAD_CAST"href")))) {
				/* 0.3 link : rel, type and href attribute */
				xtmp = xmlGetProp(cur, BAD_CAST"rel");
				if(xtmp != NULL && !xmlStrcmp(xtmp, BAD_CAST"alternate"))
					item_set_source(ip, tmp2);
				else
					/* FIXME: Maybe do something with other links? */;
				xmlFree(xtmp);
				g_free(tmp2);
			} else {
				/* 0.2 link : element content is the link, or non-alternate link in 0.3 */
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp)
					item_set_source(ip, tmp);
				g_free(tmp);
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"author")) {
			/* parse feed author */
			tmp =  parseAuthor(cur);
			ip->metadata = metadata_list_append(ip->metadata, "author", tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"contributor")) {
			/* parse feed contributors */
			tmp = parseAuthor(cur);
			ip->metadata = metadata_list_append(ip->metadata, "contributor", tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"id")) {
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if (tmp != NULL)
				item_set_id(ip, tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"issued")) {
			/* FIXME: is <modified> or <issued> or <created> the time tag we want to display? */
 			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
 			if(NULL != tmp)
				item_set_time(ip, parseRFC822Date(tmp));
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"content")) {
			/* <content> support */
			gchar *tmp = utf8_fix(pie_parse_content_construct(cur));
			if (tmp != NULL)
				item_set_description(ip, tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"summary")) {			
			/* <summary> can be used for short text descriptions, if there is no
			   <content> description we show the <summary> content */
			if (NULL == item_get_description(ip)) {
				tmp = utf8_fix(pie_parse_content_construct(cur));
				if(NULL != tmp)
					item_set_description(ip, tmp);
				g_free(tmp);
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"copyright")) {
 			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
 			if(NULL != tmp)
				ip->metadata = metadata_list_append(ip->metadata, "copyright", tmp);
			g_free(tmp);
		}
		cur = cur->next;
	}
	
	/* after parsing we fill the infos into the itemPtr structure */
	item_set_read_status(ip, FALSE);

	g_hash_table_destroy(nsinfos);
	
	return ip;
}
