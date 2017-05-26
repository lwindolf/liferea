/**
 * @file ns_ag.c mod_aggregation support
 *
 * Copyright (C) 2003-2009 Lars Windolf <lars.windolf@gmx.de>
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

#include "ns_ag.h"

#include "date.h"
#include "common.h"
#include "metadata.h"

/* you can find an aggregation namespace spec at:
   http://web.resource.org/rss/1.0/modules/aggregation/
 
  taglist for mod_aggregation:
  --------------------------------
     source
     sourceURL
     timestamp
  --------------------------------
  
  both tags usually contains URIs which we simply display in the
  feed info view footer
*/

static void
parse_item_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	gchar		*date, *source, *sourceURL, *tmp;
	gboolean	sourceTag = FALSE;
	
	if (!xmlStrcmp (BAD_CAST "source", cur->name)) {
		sourceTag = TRUE;
		g_hash_table_insert (ctxt->item->tmpdata, g_strdup ("ag:source"), (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		
	} else if (!xmlStrcmp (BAD_CAST "sourceURL", cur->name)) {  
		sourceTag = TRUE;
		g_hash_table_insert (ctxt->item->tmpdata, g_strdup ("ag:sourceURL"), (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));	
	}
	
	if (sourceTag) {
		source = g_hash_table_lookup (ctxt->item->tmpdata, "ag:source");
		sourceURL = g_hash_table_lookup (ctxt->item->tmpdata, "ag:sourceURL");
		
		if (source && sourceURL)
			tmp = g_strdup_printf ("<a href=\"%s\">%s</a>", sourceURL, source);
		else if (!source)
			tmp = g_strdup_printf ("<a href=\"%s\">%s</a>", sourceURL, sourceURL);
		else
			tmp = g_strdup (source);
	
		metadata_list_set (&(ctxt->item->metadata), "agSource", tmp);
	} else if (!xmlStrcmp (BAD_CAST "timestamp", cur->name)) {
		if (NULL != (tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1))) {
			date = date_format (date_parse_ISO8601 (tmp), _("%b %d %H:%M"));
			metadata_list_set (&(ctxt->item->metadata), "agTimestamp", date);
			g_free (date);
			g_free (tmp);
		}
	}
}

static void
ns_ag_register_ns (NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash)
{
	g_hash_table_insert (prefixhash, "ag", nsh);
	g_hash_table_insert (urihash, "http://purl.org/rss/1.0/modules/aggregation/", nsh);
}

NsHandler *
ns_ag_get_handler (void)
{
	NsHandler 	*nsh;
	
	nsh = g_new0 (NsHandler, 1);
	nsh->registerNs		= ns_ag_register_ns;
	nsh->prefix		= "ag";
	nsh->parseItemTag	= parse_item_tag;

	return nsh;
}
