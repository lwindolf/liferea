/**
 * @file ns_source.c source namespace support
 *
 * Copyright (C) 2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "ns_source.h"
#include "metadata.h"

/* For namespace docs see: https://source.scripting.com/ */

static void
parse_feed_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	gchar	*tmp = NULL;
	
	if (!xmlStrcmp (BAD_CAST"blogroll", cur->name)) {
		tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);
		if (tmp)
			metadata_list_set (&(ctxt->subscription->metadata), "blogroll", tmp);
	}
}

void
ns_source_register_ns (GHashTable *prefixhash, GHashTable *urihash)
{
	static NsHandler nsh = {
		.parseChannelTag = parse_feed_tag,
	};
	g_hash_table_insert (prefixhash, "source", &nsh);
	g_hash_table_insert (urihash, "http://source.scripting.com/", &nsh);
}