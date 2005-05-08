/**
 * @file cdf_channel.c CDF channel parsing
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
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
#include "cdf_channel.h"
#include "cdf_item.h"
#include "callbacks.h"
#include "metadata.h"

/* note: the tag order has to correspond with the CHANNEL_* defines in the header file */
static GHashTable *channelHash = NULL;

/* method to parse standard tags for the channel element */
static void parseCDFChannel(feedPtr fp, CDFChannelPtr cp, xmlDocPtr doc, xmlNodePtr cur) {
	gchar		*tmp, *tmp2, *tmp3;
	itemPtr		ip;
	GList		*items = NULL;
	
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if(NULL == cur->name || cur->type != XML_ELEMENT_NODE) {
			cur = cur->next;
			continue;
		}

		if((!xmlStrcasecmp(cur->name, BAD_CAST"logo"))) {
			tmp = utf8_fix(xmlGetProp(cur, BAD_CAST"HREF"));
			if(tmp == NULL)
				tmp = utf8_fix(xmlGetProp(cur, BAD_CAST"href"));
			if(tmp != NULL) {
				feed_set_image_url(fp, tmp);
				g_free(tmp);
			}

		} else if((!xmlStrcasecmp(cur->name, BAD_CAST"a"))) {
			xmlChar *value = xmlGetProp(cur, BAD_CAST"HREF");
			if(value != NULL) {
				feed_set_html_url(fp, (gchar *)value);
				xmlFree(value);
			}

		} else if((!xmlStrcasecmp(cur->name, BAD_CAST"item"))) {
			if(NULL != (ip = parseCDFItem(fp, cp, doc, cur))) {
				if(0 == item_get_time(ip))
					item_set_time(ip, cp->time);
				items = g_list_append(items, ip);
			}

		} else if(!xmlStrcasecmp(cur->name, BAD_CAST "title")) {
			tmp = utf8_fix(xmlNodeListGetString(doc, cur->xmlChildrenNode, TRUE));
			if(NULL != tmp) {
				tmp = unhtmlize(tmp);
				feed_set_title(fp, tmp);
				g_free(tmp);
			}
			
		} else if(!xmlStrcasecmp(cur->name, BAD_CAST "abstract")) {
			tmp = utf8_fix(xmlNodeListGetString(doc, cur->xmlChildrenNode, TRUE));
			if(NULL != tmp) {
				tmp =  convertToHTML(tmp);
				feed_set_description(fp, tmp);
				xmlFree(tmp);
			}
			
		} else {		
			tmp = g_ascii_strdown((gchar *)cur->name, -1);
			if((tmp2 = g_hash_table_lookup(channelHash, tmp)) != NULL) {
				tmp3 = utf8_fix(xmlNodeListGetString(doc, cur->xmlChildrenNode, TRUE));
				if (tmp3 != NULL) {
					fp->metadata = metadata_list_append(fp->metadata, tmp2, tmp3);
					g_free(tmp3);
				}
			}
			g_free(tmp);
		}
		
		cur = cur->next;
	}
	feed_add_items(fp, items);		
}

/* reads a CDF feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void cdf_parse(feedPtr fp, xmlDocPtr doc, xmlNodePtr cur) {
	CDFChannelPtr 	cp;
	
	cp = g_new0(struct CDFChannel, 1);
	cp->nsinfos = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	
	do {
		/* note: we support only one flavour of CDF channels! We will only
		   support the outer channel of the CDF feed. */
		
		/* find outer channel tag */
		while(cur != NULL) {
			if(cur->type == XML_ELEMENT_NODE && (!xmlStrcasecmp(cur->name, BAD_CAST"channel"))) {
				cur = cur->xmlChildrenNode;
				break;
			}
			cur = cur->next;
		}

		time(&(cp->time));
		
		/* find first "real" channel tag */
		while(cur != NULL) {
			if((!xmlStrcasecmp(cur->name, BAD_CAST"channel"))) {
				parseCDFChannel(fp, cp, doc, cur);
				g_assert(NULL != cur);
				break;
			}
			g_assert(NULL != cur);
			cur = cur->next;
		}

		/* after parsing we fill in the infos into the feedPtr structure */		
		feed_set_default_update_interval(fp, -1);

		if (cur != NULL)
			feed_set_available(fp, TRUE);
		
		g_hash_table_destroy(cp->nsinfos);

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
