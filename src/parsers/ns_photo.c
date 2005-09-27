/**
 * @file ns_photo.c photo blog namespace support
 *
 * Copyright (C) 2004 Lars Lindner <lars.lindner@gmx.net>
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

#include "support.h"
#include "common.h"
#include "ns_photo.h"
#include "ui/ui_htmlview.h"

void ns_photo_render(gpointer data, struct displayset *displayset, gpointer user_data) {
	gchar	*thumbnail, *imgsrc;

	thumbnail = g_strdup((gchar *)data);
	if(NULL != (imgsrc = strchr(thumbnail, ','))) {
		*imgsrc = 0;
		imgsrc++;
		
		addToHTMLBuffer(&(displayset->body), "<div class=photoheader>");
		addToHTMLBuffer(&(displayset->body), _("included photo"));
		addToHTMLBuffer(&(displayset->body), "</div>");
		if(*imgsrc != 0) {
			addToHTMLBuffer(&(displayset->body), "<a class=photolink href=\"");
			addToHTMLBuffer(&(displayset->body), imgsrc);
			addToHTMLBuffer(&(displayset->body), "\">");
		}
		
		addToHTMLBuffer(&(displayset->body), "<img class=photoimg src=\"");
		addToHTMLBuffer(&(displayset->body), thumbnail);
		addToHTMLBuffer(&(displayset->body), "\">");
		
		if(*imgsrc != 0) {
			addToHTMLBuffer(&(displayset->body), "</a>");
		}
	}
	g_free(thumbnail);
}

static void parse_item_tag(itemPtr ip, xmlNodePtr cur) {
	gchar	*tmp, *thumbnail, *imgsrc;
	
	if(!xmlStrcmp("thumbnail", cur->name) || !xmlStrcmp("thumb", cur->name)) {
 		tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		if(NULL != tmp) {
			if(g_utf8_strlen(tmp, -1) > 0)
	 			g_hash_table_insert(ip->tmpdata, "photo:thumbnail", tmp);
			else
				g_free(tmp);
		}
	} else if(!xmlStrcmp("imgsrc", cur->name)) {
 		tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		if(NULL != tmp) {
			if(g_utf8_strlen(tmp, -1) > 0)
	 			g_hash_table_insert(ip->tmpdata, "photo:imgsrc", tmp);				
			else
				g_free(tmp);
		}
	}
	
	thumbnail = g_hash_table_lookup(ip->tmpdata, "photo:thumbnail");
	imgsrc = g_hash_table_lookup(ip->tmpdata, "photo:imgsrc");
	if(NULL == thumbnail) {
		/* we do nothing */
	} else {
		tmp = g_strdup_printf("%s,%s", thumbnail, (NULL != imgsrc)?imgsrc:"");
		metadata_list_set(&(ip->metadata), "photo", tmp);
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
