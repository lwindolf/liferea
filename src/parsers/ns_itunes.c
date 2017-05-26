/**
 * @file ns_itunes.c itunes namespace support
 *
 * Copyright (C) 2007 Lars Windolf <lars.windolf@gmx.de>
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
	gchar *tmp;
	
	if (!xmlStrcmp(cur->name, BAD_CAST"author")) {
		tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);
		if (tmp) {
			ctxt->item->metadata = metadata_list_append (ctxt->item->metadata, "author", tmp);
			g_free (tmp);
		}
	}
	
	if (!xmlStrcmp (cur->name, BAD_CAST"summary")) {
		tmp = xhtml_extract (cur, 0, NULL);
		item_set_description (ctxt->item, tmp);
		g_free (tmp);
	}
	
	if (!xmlStrcmp(cur->name, BAD_CAST"keywords")) {
		gchar *keyword = tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);
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
	gchar *tmp;
	const gchar *old;

	if (!xmlStrcmp (cur->name, BAD_CAST"summary") || !xmlStrcmp (cur->name, BAD_CAST"subtitle")) {
		tmp = xhtml_extract (cur, 0, NULL);
		old = metadata_list_get (ctxt->subscription->metadata, "description");
		if (!old || strlen (old) < strlen (tmp))
			metadata_list_set (&ctxt->subscription->metadata, "description", tmp);
		g_free (tmp);
	}
}

static void
ns_itunes_register_ns (NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash)
{
	g_hash_table_insert (prefixhash, "itunes", nsh);
	g_hash_table_insert (urihash, "http://www.itunes.com/dtds/podcast-1.0.dtd", nsh);
}

NsHandler *
ns_itunes_get_handler (void)
{
	NsHandler 	*nsh;
	
	nsh = g_new0 (NsHandler, 1);
	nsh->prefix		= "itunes";
	nsh->registerNs		= ns_itunes_register_ns;
	nsh->parseItemTag	= ns_itunes_parse_item_tag;
	nsh->parseChannelTag	= ns_itunes_parse_channel_tag;

	return nsh;
}
