/**
 * @file rss_item.c  RSS/RDF item parsing
 *
 * Copyright (C) 2003-2010 Lars Windolf <lars.windolf@gmx.de>
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

#define RDF_NS	BAD_CAST"http://www.w3.org/1999/02/22-rdf-syntax-ns#"

extern GHashTable *RssToMetadataMapping;

/* uses the same namespace handler as rss_channel */
extern GHashTable	*rss_nstable;
extern GHashTable	*ns_rss_ns_uri_table;

/* method to parse standard tags for each item element */
itemPtr
parseRSSItem (feedParserCtxtPtr ctxt, xmlNodePtr cur)
{
	gchar			*tmp, *tmp2, *tmp3;
	NsHandler		*nsh;
	parseItemTagFunc	pf;

	g_assert(NULL != cur);

	ctxt->item = item_new ();
	ctxt->item->tmpdata = g_hash_table_new_full (g_str_hash, g_str_equal, NULL, g_free);

	/* try to get an item about id */
	tmp = xml_get_attribute (cur, "about");
	if (tmp) {
		item_set_id (ctxt->item, tmp);
		item_set_source (ctxt->item, tmp);
		g_free (tmp);
	}

	cur = cur->xmlChildrenNode;
	while (cur) {
		if (cur->type != XML_ELEMENT_NODE || !cur->name) {
			cur = cur->next;
			continue;
		}

		/* check namespace of this tag */
		if (cur->ns) {
			if((cur->ns->href && (nsh = (NsHandler *)g_hash_table_lookup(ns_rss_ns_uri_table, (gpointer)cur->ns->href))) ||
			   (cur->ns->prefix && (nsh = (NsHandler *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix)))) {
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
		tmp2 = g_hash_table_lookup(RssToMetadataMapping, cur->name);
		if (tmp2) {
			tmp3 = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE);
			if (tmp3) {
				ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, tmp2, tmp3);
				g_free(tmp3);
			}
		}
		/* check for specific tags */
		else if(!xmlStrcmp(cur->name, BAD_CAST"pubDate")) {
 			tmp = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
			if (tmp) {
				item_set_time (ctxt->item, date_parse_RFC822 (tmp));
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"enclosure")) {
			/* RSS 0.93 allows multiple enclosures */
			tmp = xml_get_attribute (cur, "url");
			if (tmp) {
				const gchar *feedURL = subscription_get_homepage (ctxt->subscription);

				gchar *type = xml_get_attribute (cur, "type");
				gchar *lengthStr = xml_get_attribute (cur, "length");
				gchar *enclStr = NULL;
				gssize length = 0;
				if (lengthStr)
					length = atol (lengthStr);

				if((strstr(tmp, "://") == NULL) && feedURL && (feedURL[0] != '|') &&
				   (strstr(feedURL, "://") != NULL)) {
					/* add base URL if necessary and possible */
					 tmp2 = g_strdup_printf("%s/%s", feedURL, tmp);
					 g_free(tmp);
					 tmp = tmp2;
				}

				enclStr = enclosure_values_to_string (tmp, type, length, FALSE);
				ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "enclosure", enclStr);
				ctxt->item->hasEnclosure = TRUE;

				g_free (enclStr);
				g_free (tmp);
				g_free (type);
				g_free (lengthStr);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"guid")) {
			if(!item_get_id(ctxt->item)) {
				tmp = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
				if (tmp) {
					if (strlen (tmp) > 0) {
						item_set_id(ctxt->item, tmp);
						ctxt->item->validGuid = TRUE;
						tmp2 = xml_get_attribute (cur, "isPermaLink");
						if(!item_get_source(ctxt->item) && (tmp2 == NULL || g_str_equal (tmp2, "true")))
							item_set_source(ctxt->item, tmp); /* Per the RSS 2.0 spec. */
						if(tmp2)
							xmlFree(tmp2);
					}
					xmlFree(tmp);
				}
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
 			tmp = unhtmlize((gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE));
			if (tmp) {
				item_set_title(ctxt->item, tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
 			tmp = unhtmlize((gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE));
			if (tmp) {
				item_set_source(ctxt->item, tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"description")) {
 			tmp = xhtml_extract (cur, 0, NULL);
			if (tmp) {
				/* don't overwrite content:encoded descriptions... */
				if(!item_get_description(ctxt->item))
					item_set_description(ctxt->item, tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"source")) {
			tmp = xml_get_attribute (cur, "url");
			if (tmp) {
				metadata_list_set (&(ctxt->item->metadata), "realSourceUrl", g_strchomp (tmp));
				g_free (tmp);
			}
			tmp = unhtmlize ((gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1));
			if (tmp) {
				metadata_list_set (&(ctxt->item->metadata), "realSourceTitle", g_strchomp (tmp));
				g_free(tmp);
			}
		}

		cur = cur->next;
	}

	ctxt->item->readStatus = FALSE;

	g_hash_table_destroy(ctxt->item->tmpdata);
	ctxt->item->tmpdata = NULL;

	return ctxt->item;
}
