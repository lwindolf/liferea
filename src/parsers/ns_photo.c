/**
 * @file ns_photo.c photo blog namespace support
 *
 * Copyright (C) 2004-2007 Lars Lindner <lars.lindner@gmail.com>
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
#include "ns_photo.h"

static void parse_item_tag(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	gchar	*tmp, *thumbnail, *imgsrc;
	
	if(!xmlStrcmp("thumbnail", cur->name) || !xmlStrcmp("thumb", cur->name)) {
 		if(NULL != (tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
			if(g_utf8_strlen(tmp, -1) > 0)
	 			g_hash_table_insert(ctxt->item->tmpdata, "photo:thumbnail", tmp);
			else
				g_free(tmp);
		}
	} else if(!xmlStrcmp("imgsrc", cur->name)) {
 		if(NULL != (tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)))) {
			if(g_utf8_strlen(tmp, -1) > 0)
	 			g_hash_table_insert(ctxt->item->tmpdata, "photo:imgsrc", tmp);				
			else
				g_free(tmp);
		}
	}
	
	thumbnail = g_hash_table_lookup(ctxt->item->tmpdata, "photo:thumbnail");
	imgsrc = g_hash_table_lookup(ctxt->item->tmpdata, "photo:imgsrc");
	if(!thumbnail) {
		/* we do nothing */
	} else {
		tmp = g_strdup_printf("%s,%s", thumbnail, imgsrc?imgsrc:"");
		metadata_list_set(&(ctxt->item->metadata), "photo", tmp);
		g_free(tmp);
	}
}

static void ns_pb_register_ns(NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash) {
	g_hash_table_insert(prefixhash, "pb", nsh);
	g_hash_table_insert(urihash, "http://snaplog.com/backend/PhotoBlog.html", nsh);
}

NsHandler *ns_pb_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->registerNs			= ns_pb_register_ns;
	nsh->prefix			= "pb";
	nsh->parseItemTag		= parse_item_tag;

	return nsh;
}

static void ns_photo_register_ns(NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash) {
	g_hash_table_insert(prefixhash, "photo", nsh);
	g_hash_table_insert(urihash, "http://www.pheed.com/pheed/", nsh);
}

NsHandler *ns_photo_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->registerNs			= ns_photo_register_ns;
	nsh->prefix			= "photo";
	nsh->parseItemTag		= parse_item_tag;

	return nsh;
}
