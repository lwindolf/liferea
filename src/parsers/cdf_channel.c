/**
 * @file cdf_channel.c CDF channel parsing
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

/* CDF is evil. There is only one outdated specification of it and its
   example is not even well-formed XML! Also, it seems to rely on
   things being case insensitive. Some people seem to make the tags
   capitalized, while others are like "Channel". */

#include <sys/time.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "common.h"
#include "feed.h"
#include "itemset.h"
#include "cdf_channel.h"
#include "cdf_item.h"
#include "callbacks.h"
#include "metadata.h"

/* note: the tag order has to correspond with the CHANNEL_* defines in the header file */
static GHashTable *channelHash = NULL;

/* method to parse standard tags for the channel element */
static void parseCDFChannel(feedParserCtxtPtr ctxt, xmlNodePtr cur, CDFChannelPtr cp) {
	gchar		*tmp, *tmp2, *tmp3;
	
	cur = cur->xmlChildrenNode;
	while(cur) {
		if(!cur->name || cur->type != XML_ELEMENT_NODE) {
			cur = cur->next;
			continue;
		}

		if((!xmlStrcasecmp(cur->name, BAD_CAST"logo"))) {
			if(tmp = utf8_fix(xmlGetProp(cur, BAD_CAST"HREF"))) 
				tmp = utf8_fix(xmlGetProp(cur, BAD_CAST"href"));
			if(tmp) {
				feed_set_image_url(ctxt->feed, tmp);
				g_free(tmp);
			}

		} else if((!xmlStrcasecmp(cur->name, BAD_CAST"a"))) {
			xmlChar *value = xmlGetProp(cur, BAD_CAST"HREF");
			if(value) {
				feed_set_html_url(ctxt->feed, (gchar *)value);
				xmlFree(value);
			}

		} else if((!xmlStrcasecmp(cur->name, BAD_CAST"item"))) {
			if(ctxt->item = parseCDFItem(ctxt, cur, cp)) {
				if(0 == ctxt->item->time)
					ctxt->item->time = cp->time;
				itemset_append_item(ctxt->itemSet, ctxt->item);
			}

		} else if(!xmlStrcasecmp(cur->name, BAD_CAST "title")) {
			if(tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE))) {
				tmp = unhtmlize(tmp);
				node_set_title(ctxt->node, tmp);
				g_free(tmp);
			}
			
		} else if(!xmlStrcasecmp(cur->name, BAD_CAST "abstract")) {
			if(tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE))) {
				tmp = convertToHTML(tmp);
				feed_set_description(ctxt->feed, tmp);
				xmlFree(tmp);
			}
			
		} else {		
			tmp = g_ascii_strdown((gchar *)cur->name, -1);
			if(tmp2 = g_hash_table_lookup(channelHash, tmp)) {
				if(tmp3 = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE))) {
					ctxt->feed->metadata = metadata_list_append(ctxt->feed->metadata, tmp2, tmp3);
					g_free(tmp3);
				}
			}
			g_free(tmp);
		}
		
		cur = cur->next;
	}
}

/* reads a CDF feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void cdf_parse(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	CDFChannelPtr 	cp;
	
	cp = g_new0(struct CDFChannel, 1);
	
	do {
		/* note: we support only one flavour of CDF channels! We will only
		   support the outer channel of the CDF feed. */
		
		/* find outer channel tag */
		while(cur) {
			if(cur->type == XML_ELEMENT_NODE && (!xmlStrcasecmp(cur->name, BAD_CAST"channel"))) {
				cur = cur->xmlChildrenNode;
				break;
			}
			cur = cur->next;
		}

		time(&(cp->time));
		
		/* find first "real" channel tag */
		while(cur) {
			if((!xmlStrcasecmp(cur->name, BAD_CAST"channel"))) {
				parseCDFChannel(ctxt, cur, cp);
				break;
			}
			cur = cur->next;
		}

		/* after parsing we fill in the infos into the feedPtr structure */		
		feed_set_default_update_interval(ctxt->feed, -1);
		
		g_free(cp);
	} while (FALSE);
}

gboolean cdf_format_check(xmlDocPtr doc, xmlNodePtr cur) {
		
	/* avoid mistaking RSS 1.1 which also uses "Channel" as root tag */
	if((NULL != cur->ns) &&
	   (NULL != cur->ns->href) &&
	   !xmlStrcmp(cur->ns->href, BAD_CAST"http://purl.org/net/rss1.1#"))
	   	return FALSE;
		
	if(!xmlStrcasecmp(cur->name, BAD_CAST"Channel"))
		return TRUE;
		
	return FALSE;
}

/* ---------------------------------------------------------------------------- */
/* initialization		 						*/
/* ---------------------------------------------------------------------------- */

#define CDF_CHANNEL_PUBDATE		4
#define CDF_CHANNEL_WEBMASTER		5
#define CDF_CHANNEL_CATEGORY		6

feedHandlerPtr cdf_init_feed_handler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);
	
	/* there are no name space handlers! */
	if (channelHash == NULL) {
		channelHash = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(channelHash, "copyright", "copyright");
		g_hash_table_insert(channelHash, "publicationdate", "lastBuildDate");
		g_hash_table_insert(channelHash, "publisher", "managingEditor");
		g_hash_table_insert(channelHash, "category", "category");
	}
	
	/* prepare feed handler structure */
	fhp->typeStr = "cdf";
	fhp->icon = ICON_AVAILABLE;
	fhp->directory = FALSE;
	fhp->feedParser	= cdf_parse;
	fhp->checkFormat = cdf_format_check;
	fhp->merge = TRUE;
	return fhp;
}
