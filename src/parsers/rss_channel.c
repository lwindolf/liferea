/**
 * @file rss_channel.c  some tolerant and generic RSS/RDF channel parsing
 *
 * Copyright (C) 2003-2022 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2005-2006 Nathan Conrad <t98502@users.sourceforge.net>
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
#include "rss_channel.h"

#include <string.h>
#include <stdlib.h>

#include "common.h"
#include "date.h"
#include "feed_parser.h"
#include "feedlist.h"
#include "metadata.h"
#include "ns_admin.h"
#include "ns_ag.h"
#include "ns_cC.h"
#include "ns_content.h"
#include "ns_dc.h"
#include "ns_georss.h"
#include "ns_itunes.h"
#include "ns_media.h"
#include "ns_slash.h"
#include "ns_syn.h"
#include "ns_trackback.h"
#include "ns_wfw.h"
#include "rss_item.h"
#include "xml.h"

/* HTML output strings */
#define TEXT_INPUT_FORM_START	"<form class=\"rssform\" method=\"GET\" action=\""
#define TEXT_INPUT_TEXT_FIELD	"\"><input class=\"rssformtext\" type=\"text\" value=\"\" name=\""
#define TEXT_INPUT_SUBMIT	"\" /><input class=\"rssformsubmit\" type=\"submit\" value=\""
#define TEXT_INPUT_FORM_END	"\" /></form>"

GHashTable *RssToMetadataMapping = NULL;

/* to store the NsHandler structs for all supported RDF namespace handlers */
GHashTable	*rss_nstable = NULL;	/* duplicate storage: for quick finding... */
GHashTable	*ns_rss_ns_uri_table = NULL;

/* This function parses the metadata for the channel. This does not
   parse the items. The items are parsed elsewhere. */
static void parseChannel(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	gchar			*tmp, *tmp2, *tmp3;
	NsHandler		*nsh;
	parseChannelTagFunc	pf;

	g_assert(NULL != cur);

	cur = cur->xmlChildrenNode;
	while(cur) {
		if(cur->type != XML_ELEMENT_NODE || cur->name == NULL) {
			cur = cur->next;
			continue;
		}

		/* check namespace of this tag */
		if(cur->ns) {
			if((cur->ns->href && (nsh = (NsHandler *)g_hash_table_lookup(ns_rss_ns_uri_table, (gpointer)cur->ns->href))) ||
			   (cur->ns->prefix && (nsh = (NsHandler *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix)))) {
				if(NULL != (pf = nsh->parseChannelTag))
					(*pf)(ctxt, cur);
				cur = cur->next;
				continue;
			} else {
				/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
			}
		} /* explicitly no following else !!! */

		/* Check for metadata tags */
		if(NULL != (tmp2 = g_hash_table_lookup(RssToMetadataMapping, cur->name))) {
			if(NULL != (tmp3 = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE))) {
				ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, tmp2, tmp3);
				g_free(tmp3);
			}
		}
		/* check for specific tags */
		else if(!xmlStrcmp(cur->name, BAD_CAST"pubDate")) {
 			if(NULL != (tmp = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1))) {
				ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "pubDate", tmp);
				ctxt->feed->time = date_parse_RFC822 (tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"ttl")) {
 			if(NULL != (tmp = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE))) {
				ctxt->subscription->updateState->timeToLive = atoi (tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
 			if(NULL != (tmp = unhtmlize((gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)))) {
				if(ctxt->title)
					g_free(ctxt->title);
				ctxt->title = tmp;
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
 			if(NULL != (tmp = unhtmlize((gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)))) {
				subscription_set_homepage (ctxt->subscription, tmp);
				g_free(tmp);
			}
		}
		else if (!xmlStrcmp (cur->name, BAD_CAST"description")) {
 			tmp = xhtml_extract (cur, 0, NULL);
			if (tmp) {
				metadata_list_set (&ctxt->subscription->metadata, "description", tmp);
				g_free (tmp);
			}
		}

		cur = cur->next;
	}
}

static gchar* parseTextInput(xmlNodePtr cur) {
	gchar	*buffer = NULL, *tiLink = NULL, *tiName = NULL, *tiDescription = NULL, *tiTitle = NULL;

	g_assert(NULL != cur);

	cur = cur->xmlChildrenNode;
	while(cur) {
		if(cur->type == XML_ELEMENT_NODE) {
			if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
				g_free(tiTitle);
				tiTitle = xhtml_extract (cur, 0, NULL);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"description")) {
				g_free(tiDescription);
				tiDescription = xhtml_extract (cur, 0, NULL);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"name")) {
				g_free(tiName);
				tiName = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
				g_free(tiLink);
				tiLink = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
			}
		}
		cur = cur->next;
	}

	/* some postprocessing */
	tiTitle = unhtmlize(tiTitle);
	tiDescription = unhtmlize(tiDescription);

	if(tiLink && tiName && tiDescription && tiTitle) {
		buffer = g_strdup_printf("<p>%s%s%s%s%s%s%s%s</p>",
		                         tiDescription,
					 TEXT_INPUT_FORM_START,
					 tiLink,
					 TEXT_INPUT_TEXT_FIELD,
					 tiName,
					 TEXT_INPUT_SUBMIT,
					 tiTitle,
					 TEXT_INPUT_FORM_END);
	}
	g_free(tiTitle);
	g_free(tiDescription);
	g_free(tiName);
	g_free(tiLink);
	return buffer;
}

static gchar* parseImage(xmlNodePtr cur) {
	gchar	*tmp;

	g_assert(NULL != cur);

	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if (cur->type == XML_ELEMENT_NODE) {
			if (!xmlStrcmp(cur->name, BAD_CAST"url")) {
				tmp = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
				if(NULL != tmp) {
					return tmp;
				}
			}
		}
		cur = cur->next;
	}
	return NULL;
}

/**
 * Parses given data as an RSS/RDF channel
 *
 * @param ctxt		the feed parser context
 * @param cur		the root node of the XML document
 */
static void rss_parse(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	gchar		*tmp;
	short 		rdf = 0;
	int 		error = 0;

	ctxt->feed->time = time(NULL);

	if(!xmlStrcmp(cur->name, BAD_CAST"rss")) {
		cur = cur->xmlChildrenNode;
		rdf = 0;
	} else if(!xmlStrcmp(cur->name, BAD_CAST"rdf") ||
	          !xmlStrcmp(cur->name, BAD_CAST"RDF")) {
		cur = cur->xmlChildrenNode;
		rdf = 1;
	} else if(!xmlStrcmp(cur->name, BAD_CAST"Channel")) {
		/* explicitly no "cur = cur->xmlChildrenNode;" ! */
		rdf = 0;
	} else {
		g_string_append(ctxt->feed->parseErrors, "<p>Could not find RDF/RSS header!</p>");
		error = 1;
	}

	if(!error) {
		while(cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}

		while(cur) {
			if(!cur->name) {
				g_warning("invalid XML: parser returns NULL value -> tag ignored!");
				cur = cur->next;
				continue;
			}

			if((!xmlStrcmp(cur->name, BAD_CAST"channel")) ||
			   (!xmlStrcmp(cur->name, BAD_CAST"Channel"))) {
				parseChannel(ctxt, cur);
				if(0 == rdf)
					cur = cur->xmlChildrenNode;
				break;
			}
			cur = cur->next;
		}

		/* For RDF (rss 0.9 or 1.0), cur now points to the item after the channel tag. */
		/* For RSS, cur now points to the first item inside of the channel tag */
		/* This ends up being the thing with the items, (and images/textinputs for RDF) */

		/* parse channel contents */
		while(cur) {
			if(cur->type != XML_ELEMENT_NODE || NULL == cur->name) {
				cur = cur->next;
				continue;
			}

			/* save link to channel image */
			if((!xmlStrcmp(cur->name, BAD_CAST"image"))) {
				if(NULL != (tmp = parseImage(cur))) {
					metadata_list_set (&ctxt->subscription->metadata, "imageUrl", tmp);
					g_free(tmp);
				}

			} else if((!xmlStrcmp(cur->name, BAD_CAST"textinput")) ||
			          (!xmlStrcmp(cur->name, BAD_CAST"textInput"))) {
				/* no matter if we parse Userland or Netscape, there should be
				   only one text[iI]nput per channel and parsing the rdf:ressource
				   one should not harm */
				if(NULL != (tmp = parseTextInput(cur))) {
					ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "textInput", tmp);
					g_free(tmp);
				}

			} else if((!xmlStrcmp(cur->name, BAD_CAST"items"))) { /* RSS 1.1 */
				xmlNodePtr itemNode = cur->xmlChildrenNode;
				while(itemNode) {
					if ((!xmlStrcmp(itemNode->name, BAD_CAST"item"))) {
						if(NULL != (ctxt->item = parseRSSItem(ctxt, itemNode))) {
							if(0 == ctxt->item->time)
								ctxt->item->time = ctxt->feed->time;
							ctxt->items = g_list_append(ctxt->items, ctxt->item);
						}
					}
					itemNode = itemNode->next;
				}
			} else if((!xmlStrcmp(cur->name, BAD_CAST"item"))) { /* RSS 1.0, 2.0 */
				/* collect channel items */
				if(NULL != (ctxt->item = parseRSSItem(ctxt, cur))) {
					if(0 == ctxt->item->time)
						ctxt->item->time = ctxt->feed->time;
					ctxt->items = g_list_append(ctxt->items, ctxt->item);
				}

			}
			cur = cur->next;
		}
	}
}

static gboolean rss_format_check(xmlDocPtr doc, xmlNodePtr cur) {

	if(!xmlStrcmp(cur->name, BAD_CAST"rss") ||
	   !xmlStrcmp(cur->name, BAD_CAST"rdf") ||
	   !xmlStrcmp(cur->name, BAD_CAST"RDF")) {
		return TRUE;
	}

	/* RSS 1.1 */
	if((NULL != cur->ns) &&
	   (NULL != cur->ns->href) &&
	   !xmlStrcmp(cur->name, BAD_CAST"Channel") &&
	   !xmlStrcmp(cur->ns->href, BAD_CAST"http://purl.org/net/rss1.1#"))
	   	return TRUE;

	return FALSE;
}

static void
rss_add_ns_handler (NsHandler *handler)
{
	g_assert (NULL != rss_nstable);
	g_hash_table_insert (rss_nstable, (gpointer)handler->prefix, handler);
	g_assert (handler->registerNs != NULL);
	handler->registerNs (handler, rss_nstable, ns_rss_ns_uri_table);
}

feedHandlerPtr
rss_init_feed_handler (void)
{
	feedHandlerPtr	fhp;

	fhp = g_new0 (struct feedHandler, 1);

	/* Note: the tag mapping definitions and namespace registration
	   infos are shared with rss_item.c */

	if (!RssToMetadataMapping) {
		RssToMetadataMapping = g_hash_table_new (g_str_hash, g_str_equal);
		g_hash_table_insert (RssToMetadataMapping, "copyright", "copyright");
		g_hash_table_insert (RssToMetadataMapping, "category", "category");
		g_hash_table_insert (RssToMetadataMapping, "webMaster", "webmaster");
		g_hash_table_insert (RssToMetadataMapping, "language", "language");
		g_hash_table_insert (RssToMetadataMapping, "managingEditor", "managingEditor");
		g_hash_table_insert (RssToMetadataMapping, "lastBuildDate", "contentUpdateDate");
		g_hash_table_insert (RssToMetadataMapping, "generator", "feedgenerator");
		g_hash_table_insert (RssToMetadataMapping, "publisher", "webmaster");
		g_hash_table_insert (RssToMetadataMapping, "author", "author");
		g_hash_table_insert (RssToMetadataMapping, "comments", "commentsUri");
	}

	if (!rss_nstable) {
		rss_nstable = g_hash_table_new (g_str_hash, g_str_equal);
		ns_rss_ns_uri_table = g_hash_table_new (g_str_hash, g_str_equal);

		/* register name space handlers */
    rss_add_ns_handler (ns_dc_get_handler ());
    rss_add_ns_handler (ns_slash_get_handler ());
		rss_add_ns_handler (ns_content_get_handler ());
		rss_add_ns_handler (ns_syn_get_handler ());
		rss_add_ns_handler (ns_admin_get_handler ());
		rss_add_ns_handler (ns_ag_get_handler ());
		rss_add_ns_handler (ns_cC_get_handler ());
		rss_add_ns_handler (ns_wfw_get_handler ());
		rss_add_ns_handler (ns_media_get_handler ());
		rss_add_ns_handler (ns_itunes_get_handler ());
		rss_add_ns_handler (ns_trackback_get_handler ());
		rss_add_ns_handler (ns_georss_get_handler ());
	}

	/* prepare feed handler structure */
	fhp->typeStr = "rss";
	fhp->feedParser	= rss_parse;
	fhp->checkFormat = rss_format_check;

	return fhp;
}
