/**
 * @file georss.c  GeoRSS namespace support
 *
 * Copyright (C) 2009 Mikel Olasagasti <mikel@olasagasti.info>
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

#include "common.h"
#include "metadata.h"
#include "ns_georss.h"

#define GEORSS_PREFIX	"georss"

/* One can find the GeoRSS namespace spec at:
   http://georss.org/model

   Just georss:point at the moment
*/

static void
ns_georss_parse_item_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	gchar	*point = NULL;

 	if (!xmlStrcmp (BAD_CAST"point", cur->name))
		point = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);

	if (point) {
		metadata_list_set (&(ctxt->item->metadata), "point", point);
		g_free (point);
	}
}

static void
ns_georss_register_ns (NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash)
{
	g_hash_table_insert (prefixhash, GEORSS_PREFIX, nsh);
	g_hash_table_insert (urihash, "http://www.georss.org/georss", nsh);
}

NsHandler *
ns_georss_get_handler (void)
{
	NsHandler 	*nsh;

	nsh = g_new0 (NsHandler, 1);
	nsh->prefix 			= GEORSS_PREFIX;
	nsh->registerNs			= ns_georss_register_ns;
	nsh->parseChannelTag		= NULL;
	nsh->parseItemTag		= ns_georss_parse_item_tag;

	return nsh;
}
