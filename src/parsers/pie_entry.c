/**
 * @file pie_entry.c Atom0.3 entry parsing 
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "common.h"
#include "pie_entry.h"
#include "pie_feed.h"
#include "metadata.h"
#include "xml.h"

/* uses the same namespace handler as PIE_channel */
extern GHashTable *pie_nstable;
extern GHashTable *ns_pie_ns_uri_table;

/* we reuse some pie_feed.c function */
extern gchar * parseAuthor(xmlNodePtr cur);

/* <content> tag support, FIXME: base64 not supported */
/* method to parse standard tags for each item element */
itemPtr parseEntry(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	xmlChar			*xtmp;
	gchar			*tmp2, *tmp;
	NsHandler		*nsh;
	parseItemTagFunc	pf;
	
	g_assert(NULL != cur);
		
	ctxt->item = item_new();
	
	cur = cur->xmlChildrenNode;
	while(cur) {
		if(!cur->name) {
			g_warning("invalid XML: parser returns NULL value -> tag ignored!");
			cur = cur->next;
			continue;
		}
		
		
		/* check namespace of this tag */
		if(cur->ns) {
			if((cur->ns->href && (nsh = (NsHandler *)g_hash_table_lookup(ns_pie_ns_uri_table, (gpointer)cur->ns->href))) ||
			   (cur->ns->prefix && (nsh = (NsHandler *)g_hash_table_lookup(pie_nstable, (gpointer)cur->ns->prefix)))) {
				
				if(pf = nsh->parseItemTag)
					(*pf)(ctxt, cur);
				cur = cur->next;
				continue;
			} else {
				/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
			}
		} /* explicitly no following else !!! */
		
		if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
			if(tmp = unhtmlize(pie_parse_content_construct(cur))) {
				item_set_title(ctxt->item, tmp);
				g_free(tmp);
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
			if(tmp2 = common_utf8_fix(xmlGetProp(cur, BAD_CAST"href"))) {
				/* 0.3 link : rel, type and href attribute */
				xtmp = xmlGetProp(cur, BAD_CAST"rel");
				if(xtmp != NULL && !xmlStrcmp(xtmp, BAD_CAST"alternate"))
					item_set_source(ctxt->item, tmp2);
				else
					/* FIXME: Maybe do something with other links? */;
				xmlFree(xtmp);
				g_free(tmp2);
			} else {
				/* 0.2 link : element content is the link, or non-alternate link in 0.3 */
				if(tmp = common_utf8_fix(xmlNodeListGetString(ctxt->doc, cur->xmlChildrenNode, 1))) {
					item_set_source(ctxt->item, tmp);
					g_free(tmp);
				}
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"author")) {
			/* parse feed author */
			tmp =  parseAuthor(cur);
			ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "author", tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"contributor")) {
			/* parse feed contributors */
			tmp = parseAuthor(cur);
			ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "contributor", tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"id")) {
			if(tmp = common_utf8_fix(xmlNodeListGetString(ctxt->doc, cur->xmlChildrenNode, 1))) {
				item_set_id(ctxt->item, tmp);
				g_free(tmp);
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"issued")) {
			/* FIXME: is <modified> or <issued> or <created> the time tag we want to display? */
 			if(tmp = common_utf8_fix(xmlNodeListGetString(ctxt->doc, cur->xmlChildrenNode, 1))) {
				ctxt->item->time = parseISO8601Date(tmp);
				g_free(tmp);
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"content")) {
			/* <content> support */
			if(tmp = common_utf8_fix(pie_parse_content_construct(cur))) {
				item_set_description(ctxt->item, tmp);
				g_free(tmp);
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"summary")) {			
			/* <summary> can be used for short text descriptions, if there is no
			   <content> description we show the <summary> content */
			if(!item_get_description(ctxt->item)) {
				if(tmp = common_utf8_fix(pie_parse_content_construct(cur))) {
					item_set_description(ctxt->item, tmp);
					g_free(tmp);
				}
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"copyright")) {
 			if(tmp = common_utf8_fix(xmlNodeListGetString(ctxt->doc, cur->xmlChildrenNode, 1))) {
				ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "copyright", tmp);
				g_free(tmp);
			}
		}
		cur = cur->next;
	}
	
	/* after parsing we fill the infos into the itemPtr structure */
	ctxt->item->readStatus = FALSE;

	return ctxt->item;
}
