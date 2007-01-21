/**
 * @file ns_wfw.c Well-Formed Web RSS namespace support
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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

#include "common.h"
#include "metadata.h"
#include "ns_wfw.h"

#define WFW_PREFIX	"wfw"

/* One can find the Well-Formed Web namespace spec at:
   http://wellformedweb.org/news/wfw_namespace_elements
 
   Only the comments feed tag is supported.
*/

static void parse_item_tag(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	gchar	*uri = NULL;

 	if(!xmlStrcmp("commentRss", cur->name) || !xmlStrcmp("commentRSS", cur->name))
		uri = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));

	if(uri) {
		metadata_list_set(&(ctxt->item->metadata), "commentFeedUri", uri);
		g_free(uri);
	}
}

static void ns_wfw_register_ns(NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash) {

	g_hash_table_insert(prefixhash, WFW_PREFIX, nsh);
	g_hash_table_insert(urihash, "http://wellformedweb.org/CommentAPI", nsh);
}

NsHandler *ns_wfw_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->prefix 			= WFW_PREFIX;
	nsh->registerNs			= ns_wfw_register_ns;
	nsh->parseChannelTag		= NULL;
	nsh->parseItemTag		= parse_item_tag;

	return nsh;
}
