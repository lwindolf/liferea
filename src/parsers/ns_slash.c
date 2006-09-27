/**
 * @file ns_slash.c slash namespace support
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 * 
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include "ns_slash.h"
#include "common.h"

/* a tag list from http://f3.grp.yahoofs.com/v1/YP40P2oiXvP5CAx4TM6aQw8mDrCtNDwF9_BkMwcvulZHdlhYmCk5cS66_06t9OaIVsubWpwtMUTxYNG7/Modules/Proposed/mod_slash.html

   hmm... maybe you can find a somewhat shorter URL!

-------------------------------------------------------

 <item> Elements:

    * <slash:section> ( #PCDATA )
    * <slash:department> ( #PCDATA )
    * <slash:comments> ( positive integer )
    * <slash:hit_parade> ( comma-separated integers )

-------------------------------------------------------

*/

static void parse_item_tag(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	gchar	*tmp = NULL, *section, *department;
	
	if(!xmlStrcmp(BAD_CAST"section", cur->name)) {
		if(NULL != (tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1))))
 			g_hash_table_insert(ctxt->item->tmpdata, "slash:section", tmp);
			
	} else if(!xmlStrcmp(BAD_CAST"department", cur->name)) {
		if(NULL != (tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1))))
 			g_hash_table_insert(ctxt->item->tmpdata, "slash:department", tmp);
	}
	
	if(tmp) {
		section = g_hash_table_lookup(ctxt->item->tmpdata, "slash:section");
		department = g_hash_table_lookup(ctxt->item->tmpdata, "slash:department");
		tmp = g_strdup_printf("%s,%s", section ? section : "",
		                               department ? department : "" );
		metadata_list_set(&(ctxt->item->metadata), "slash", tmp);
		g_free(tmp);
	}
}

static void ns_slash_register_ns(NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash) {
	g_hash_table_insert(prefixhash, "slash", nsh);
	g_hash_table_insert(urihash, "http://purl.org/rss/1.0/modules/slash/", nsh);
}

NsHandler *ns_slash_getRSSNsHandler(void) {
	NsHandler 	*nsh;
	
	nsh = g_new0(NsHandler, 1);
	nsh->registerNs		= ns_slash_register_ns;
	nsh->prefix		= "slash";
	nsh->parseItemTag	= parse_item_tag;

	return nsh;
}

