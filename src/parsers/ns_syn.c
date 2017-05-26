/**
 * @file ns_syn.c syndication namespace support
 * 
 * Copyright (C) 2003-2007 Lars Windolf <lars.windolf@gmx.de>
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

#include <stdlib.h>
#include "ns_syn.h"

/* you can find the syn module documentation at
   http://web.resource.org/rss/1.0/modules/syndication/

   the tag list
   -------------------------------------------------------
    <syn:updatePeriod>
    <syn:updateFrequency>
    <syn:updateBase>
   -------------------------------------------------------
*/

static void
ns_syn_parse_tag (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	xmlChar	*tmp;
	gint	period;
	gint	frequency = 1;
	
	period = subscription_get_default_update_interval (ctxt->subscription);
	if (!xmlStrcmp (cur->name, BAD_CAST"updatePeriod")) {
		if (NULL != (tmp = xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1))) {

			if (!xmlStrcmp (tmp, BAD_CAST"hourly"))
				period = 60;
			else if (!xmlStrcmp (tmp, BAD_CAST"daily"))
				period = 60*24;
			else if (!xmlStrcmp (tmp, BAD_CAST"weekly"))
				period = 7*24*60;
			else if (!xmlStrcmp (tmp, BAD_CAST"monthly"))
				/* FIXME: not really exact...*/
				period = 31*7*24*60;	
			else if (!xmlStrcmp (tmp, BAD_CAST"yearly"))
				period = 365*24*60;

			xmlFree (tmp);
		}
	} else if (!xmlStrcmp (cur->name, BAD_CAST"updateFrequency")) {
		tmp = xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);
		if (tmp) {
			frequency = atoi ((gchar *)tmp);
			xmlFree (tmp);
		}
	}
	
	/* postprocessing */
	if (0 != frequency)
		period /= frequency;

	subscription_set_default_update_interval (ctxt->subscription, period);
}

static void
ns_syn_register_ns (NsHandler *nsh, GHashTable *prefixhash, GHashTable *urihash)
{
	g_hash_table_insert (prefixhash, "syn", nsh);
	g_hash_table_insert (urihash, "http://purl.org/rss/1.0/modules/syndication/", nsh);
}

NsHandler *
ns_syn_get_handler (void)
{
	NsHandler 	*nsh;
	
	nsh = g_new0 (NsHandler, 1);
	nsh->registerNs		= ns_syn_register_ns;
	nsh->prefix		= "syn";
	nsh->parseChannelTag	= ns_syn_parse_tag;

	return nsh;
}
