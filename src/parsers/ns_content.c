/**
 * @file ns_content.c content namespace support
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

#include "ns_content.h"
#include "common.h"
#include "xml.h"

/* a namespace documentation can be found at 
   http://web.resource.org/rss/1.0/modules/content/
   
   This namespace handler is (for now) only used to handle
   <content:encoding> tags. If such a tag appears the originial
   description will be replaced by the encoded content.
   
*/

static void parse_item_tag(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	gchar *tmp;

  	if(!xmlStrcmp(cur->name, "encoded")) {
		if(tmp = common_utf8_fix(xhtml_extract (cur, 0, NULL))) {
			item_set_description(ctxt->item, tmp);
			g_free(tmp);
		}
	}
}

static void ns_content_register_ns(NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash) {
	g_hash_table_insert(prefixhash, "content", nsh);
	g_hash_table_insert(urihash, "http://purl.org/rss/1.0/modules/content/", nsh);
}

NsHandler *ns_content_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->prefix		= g_strdup("content");
	nsh->registerNs		= ns_content_register_ns;
	nsh->parseItemTag	= parse_item_tag;

	return nsh;
}
