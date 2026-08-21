/**
 * @file rss_item.c  RSS/RDF item parsing
 *
 * Copyright (C) 2003-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "rss_item.h"

#include <string.h>

#include "common.h"
#include "date.h"
#include "enclosure.h"
#include "metadata.h"
#include "xml.h"

extern GHashTable *RssToMetadataMapping;

/* uses the same namespace handler as rss_channel */
extern GHashTable	*rss_nstable;
extern GHashTable	*ns_rss_ns_uri_table;

/* method to parse standard tags for each item element */
itemPtr
parseRSSItem (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	NsHandler		*nsh;
	parseItemTagFunc	pf;

	g_assert (NULL != cur);

	ctxt->item = item_new ();

	/* try to get an item about id */
	g_autofree gchar *source = xml_get_attribute (cur, "about");
	if (source) {
		item_set_id (ctxt->item, source);
		item_set_source (ctxt->item, source);
	}

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (cur->type != XML_ELEMENT_NODE || !cur->name) {
			cur = cur->next;
			continue;
		}

		/* check namespace of this tag */
		if (cur->ns) {
			if ((cur->ns->href && (nsh = (NsHandler *)g_hash_table_lookup (ns_rss_ns_uri_table, (gpointer)cur->ns->href))) ||
			    (cur->ns->prefix && (nsh = (NsHandler *)g_hash_table_lookup (rss_nstable, (gpointer)cur->ns->prefix)))) {
				pf = nsh->parseItemTag;
				if (pf)
					(*pf)(ctxt, cur);
				cur = cur->next;
				continue;
			} else {
				/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
			}
		} /* explicitly no following else!!! */

		/* check for metadata tags */
		gchar *metadataMapping = g_hash_table_lookup(RssToMetadataMapping, cur->name);
		if (metadataMapping) {
			g_autofree gchar *tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, TRUE);
			if (tmp)
				metadata_list_set (&ctxt->item->metadata, metadataMapping, tmp);
		}
		/* check for specific tags */
		else if (!xmlStrcmp (cur->name, BAD_CAST"pubDate")) {
			g_autofree gchar *tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);
			if (tmp)
				item_set_time (ctxt->item, date_parse_RFC822 (tmp));
		}
		else if(!xmlStrcmp (cur->name, BAD_CAST"enclosure")) {
			g_autofree gchar *tmp = xml_get_attribute (cur, "url");
			g_autofree gchar *type = xml_get_attribute (cur, "type");
			g_autofree gchar *lengthStr = xml_get_attribute (cur, "length");
			if (tmp) {
				gssize length = 0;
				if (lengthStr)
					length = atol (lengthStr);

				item_add_enclosure (ctxt->item, enclosure_new (tmp, type, length, -1, -1));
			}
		}
		else if (!xmlStrcmp (cur->name, BAD_CAST"guid")) {
			if (!item_get_id (ctxt->item)) {
				g_autofree gchar *tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);
				if (tmp) {
					if (strlen (tmp) > 0) {
						item_set_id (ctxt->item, tmp);
						ctxt->item->validGuid = TRUE;
						g_autofree gchar *tmp2 = xml_get_attribute (cur, "isPermaLink");
						if (!item_get_source (ctxt->item) && (tmp2 == NULL || g_str_equal (tmp2, "true")))
							item_set_source (ctxt->item, tmp); /* Per the RSS 2.0 spec. */
					}
				}
			}
		}
		else if (!xmlStrcmp (cur->name, BAD_CAST"title")) {
			g_autofree gchar *tmp = unhtmlize ((gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, TRUE));
			if (tmp)
				item_set_title (ctxt->item, tmp);
		}
		else if (!xmlStrcmp (cur->name, BAD_CAST"link")) {
			g_autofree gchar *tmp = unhtmlize ((gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, TRUE));
			if (tmp)
				item_set_source (ctxt->item, tmp);
		}
		else if (!xmlStrcmp (cur->name, BAD_CAST"description")) {
			g_autofree gchar *tmp = xhtml_extract (cur, 0, NULL);
			if (tmp) {
				/* don't overwrite content:encoded descriptions... */
				if (!item_get_description (ctxt->item))
					item_set_description (ctxt->item, tmp);
			}
		}
		else if(!xmlStrcmp (cur->name, BAD_CAST"source")) {
			g_autofree gchar *url = xml_get_attribute (cur, "url");
			if (url)
				metadata_list_set (&(ctxt->item->metadata), "realSourceUrl", g_strstrip (url));
			
			g_autofree gchar *title = unhtmlize ((gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1));
			if (title)
				metadata_list_set (&(ctxt->item->metadata), "realSourceTitle", g_strstrip (title));
		}

		cur = cur->next;
	}

	ctxt->item->readStatus = FALSE;

	return ctxt->item;
}
