/**
 * @file cdf_item.c CDF item parsing 
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

#include "common.h"
#include "cdf_channel.h"
#include "cdf_item.h"
#include "metadata.h"
#include "xml.h"

extern GHashTable *cdf_nslist;

static GHashTable *CDFToMetadataMapping = NULL;

/* FIXME: The 'link' tag used to be used, but I coundn't find its
   use... The spec says to use 'A' instead. */

/* method to parse standard tags for each item element */
itemPtr parseCDFItem(feedParserCtxtPtr ctxt, xmlNodePtr cur, CDFChannelPtr cp) {
	gchar		*tmp = NULL, *tmp2, *tmp3;

	if(CDFToMetadataMapping == NULL) {
		CDFToMetadataMapping = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(CDFToMetadataMapping, "author", "author");
		g_hash_table_insert(CDFToMetadataMapping, "category", "category");
	}
		
	ctxt->item = item_new();
	
	/* save the item link */
	if(!(tmp = common_utf8_fix(xmlGetProp(cur, BAD_CAST"href"))))
		tmp = common_utf8_fix(xmlGetProp(cur, BAD_CAST"HREF"));
	if(tmp) {
		item_set_source(ctxt->item, tmp);
		g_free(tmp);
	}
	
	cur = cur->xmlChildrenNode;
	while(cur) {

		if(!cur->name || cur->type != XML_ELEMENT_NODE) {
			cur = cur->next;
			continue;
		}
		
		/* save first link to a channel image */
		if(tmp = g_ascii_strdown(cur->name, -1)) {
			if(tmp2 = g_hash_table_lookup(CDFToMetadataMapping, tmp)) {
				if(tmp3 = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE))) {
					ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, tmp2, tmp3);
					g_free(tmp3);
				}
			}
			g_free(tmp);
		}
		
		if((!xmlStrcasecmp(cur->name, BAD_CAST"logo"))) {
			
			if(!(tmp = common_utf8_fix(xmlGetProp(cur, BAD_CAST"href"))))
				tmp = common_utf8_fix(xmlGetProp(cur, BAD_CAST"HREF"));
			if(tmp) {
				ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "imageUrl", tmp);
				g_free(tmp);
			}
			
		} else if((!xmlStrcasecmp(cur->name, BAD_CAST"title"))) {
			if(tmp = unhtmlize(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1))) {
				item_set_title(ctxt->item, tmp);
				g_free(tmp);
			}
			
		} else if((!xmlStrcasecmp(cur->name, BAD_CAST"abstract"))) {
			if(tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1))) {
				item_set_description(ctxt->item, tmp);
				g_free(tmp);
			}
			
		} else if((!xmlStrcasecmp(cur->name, BAD_CAST"a"))) {
			if(!(tmp = common_utf8_fix(xmlGetProp(cur, BAD_CAST"href"))))
				tmp = common_utf8_fix(xmlGetProp(cur, BAD_CAST"HREF"));
			if(tmp) {
				item_set_source(ctxt->item, tmp);
				g_free(tmp);
			}
		}
		
		cur = cur->next;
	}

	ctxt->item->readStatus = FALSE;
	
	return ctxt->item;
}
