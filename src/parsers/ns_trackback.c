/**
 * @file ns_trackback.c trackback namespace support
 *
 * Copyright (C) 2007-2008 Lars Windolf <lars.windolf@gmx.de>
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

#include "ns_trackback.h"
#include "common.h"
#include "xml.h"

/* a namespace documentation can be found at 
   http://madskills.com/public/xml/rss/module/trackback/
*/

static void
parse_item_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	gchar *tmp;
	
	/* We ignore the "ping" tag */

  	if (xmlStrcmp (cur->name, BAD_CAST"about"))
		return;
		
	/* RSS 1.0 */
	tmp = xml_get_attribute (cur, "about");
		
	/* RSS 2.0 */
	if (!tmp)
		tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);

	if (tmp) {
		ctxt->item->metadata = metadata_list_append (ctxt->item->metadata, "related", tmp);
		g_free (tmp);
	}
}

static void
ns_trackback_register_ns (NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash)
{
	g_hash_table_insert (prefixhash, "trackback", nsh);
	g_hash_table_insert (urihash, "http://madskills.com/public/xml/rss/module/trackback/", nsh);
}

NsHandler *
ns_trackback_get_handler (void)
{
	NsHandler 	*nsh;
	
	nsh = g_new0 (NsHandler, 1);
	nsh->prefix		= "trackback";
	nsh->registerNs		= ns_trackback_register_ns;
	nsh->parseItemTag	= parse_item_tag;

	return nsh;
}
