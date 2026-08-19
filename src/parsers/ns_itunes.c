/**
 * @file ns_itunes.c itunes namespace support
 *
 * Copyright (C) 2007-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "ns_itunes.h"

#include "common.h"
#include "metadata.h"
#include "xml.h"

/* a namespace documentation can be found at 
   http://www.apple.com/itunes/store/podcaststechspecs.html
*/

static void
ns_itunes_parse_item_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	if (!xmlStrcmp(cur->name, BAD_CAST"author")) {
		g_autofree gchar *tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);
		ctxt->item->metadata = metadata_list_append (ctxt->item->metadata, "author", tmp);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST"summary")) {
		g_autofree gchar *tmp = xhtml_extract (cur, 0, NULL);
		item_set_description (ctxt->item, tmp);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST"image")) {
		g_autofree gchar *tmp = xml_get_attribute (cur, "href");
		if (tmp)
			metadata_list_set (&(ctxt->item->metadata), "mediathumbnail", tmp);
	}
	else if (!xmlStrcmp(cur->name, BAD_CAST"keywords")) {
		gchar *tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);
		gchar *keyword = tmp;
		gchar *allocated = tmp;
		/* parse comma separated list and strip leading spaces... */
		while (tmp) {
			tmp = strchr (tmp, ',');
			if (tmp) {
				*tmp = 0;
				tmp++;
			}
			while (g_unichar_isspace (*keyword)) {
				keyword = g_utf8_next_char (keyword);
			}
			ctxt->item->metadata = metadata_list_append (ctxt->item->metadata, "category", keyword);
			keyword = tmp;
		}
		g_free (allocated);
	}
}

static void
ns_itunes_parse_channel_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	if (!xmlStrcmp (cur->name, BAD_CAST"summary") || !xmlStrcmp (cur->name, BAD_CAST"subtitle")) {
		g_autofree gchar *tmp = xhtml_extract (cur, 0, NULL);
		const gchar *old = metadata_list_get (ctxt->subscription->metadata, "description");
		if (!old || strlen (old) < strlen (tmp))
			metadata_list_set (&ctxt->subscription->metadata, "description", tmp);
	}
	else if(!xmlStrcmp (cur->name, BAD_CAST"category")) {
		g_autofree gchar *tmp = xml_get_attribute (cur, "text");
		if (tmp)
			ctxt->subscription->metadata = metadata_list_append (ctxt->subscription->metadata, "category", tmp);
	}
	else if (!xmlStrcmp (cur->name, BAD_CAST"image")) {
		g_autofree gchar *tmp = xml_get_attribute (cur, "href");
		if (tmp)
			metadata_list_set (&(ctxt->subscription->metadata), "imageUrl", tmp);
	}
}

void
ns_itunes_register_ns (GHashTable *prefixhash, GHashTable *urihash)
{
	static NsHandler nsh = {
		.parseItemTag = ns_itunes_parse_item_tag,
		.parseChannelTag = ns_itunes_parse_channel_tag,
	};
	g_hash_table_insert (prefixhash, "itunes", &nsh);
	g_hash_table_insert (urihash, "http://www.itunes.com/dtds/podcast-1.0.dtd", &nsh);
}