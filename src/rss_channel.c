/**
 * @file rss_channel.c some tolerant and generic RSS/RDF channel parsing
 *
 * Note: portions of the original parser code were inspired by
 * the feed reader software Rol which is copyrighted by
 * 
 * Copyright (C) 2002 Jonathan Gordon <eru@unknown-days.com>
 * 
 * The major part of this parsing code written by
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

#include <sys/time.h>
#include <string.h>
#include <stdlib.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "conf.h"
#include "common.h"
#include "rss_channel.h"
#include "callbacks.h"
#include "feed.h"
#include "metadata.h"
#include "ns_dc.h"
#include "ns_fm.h"
#include "ns_content.h"
#include "ns_slash.h"
#include "ns_syn.h"
#include "ns_admin.h"
#include "ns_ag.h"
#include "ns_blogChannel.h"
#include "ns_cC.h"
#include "ns_photo.h"
#include "rss_item.h"

/* HTML output strings */
#define TEXT_INPUT_FORM_START	"<form class=\"rssform\" method=\"GET\" ACTION=\""
#define TEXT_INPUT_TEXT_FIELD	"\"><input class=\"rssformtext\" type=text value=\"\" name=\""
#define TEXT_INPUT_SUBMIT	"\"><input class=\"rssformsubmit\" type=submit value=\""
#define TEXT_INPUT_FORM_END	"\"></form>"

GHashTable *RssToMetadataMapping = NULL;

/* to store the NsHandler structs for all supported RDF namespace handlers */
GHashTable	*rss_nstable = NULL;	/* duplicate storage: for quick finding... */
GHashTable	*ns_rss_ns_uri_table = NULL;

/* This function parses the metadata for the channel. This does not
   parse the items. The items are parsed elsewhere. */
static void parseChannel(feedPtr fp, xmlNodePtr cur) {
	gchar			*tmp, *tmp2, *tmp3;
	NsHandler		*nsh;
	parseChannelTagFunc	pf;
	
	g_assert(NULL != cur);
			
	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if(cur->type != XML_ELEMENT_NODE || cur->name == NULL) {
			cur = cur->next;
			continue;
		}
		
		/* check namespace of this tag */
		if(NULL != cur->ns) {
			if(((cur->ns->href != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(ns_rss_ns_uri_table, (gpointer)cur->ns->href))) ||
			   ((cur->ns->prefix != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(rss_nstable, (gpointer)cur->ns->prefix)))) {
				pf = nsh->parseChannelTag;
				if(NULL != pf)
					(*pf)(fp, cur);
				cur = cur->next;
				continue;
			} else {
				/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
			}
		} /* explicitly no following else !!! */
			
		/* Check for metadata tags */
		if((tmp2 = g_hash_table_lookup(RssToMetadataMapping, cur->name)) != NULL) {
			tmp3 = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE));
			if(tmp3 != NULL) {
				fp->metadata = metadata_list_append(fp->metadata, tmp2, tmp3);
				g_free(tmp3);
			}
		}	
		/* check for specific tags */
		else if(!xmlStrcmp(cur->name, BAD_CAST"pubDate")) {
 			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp) {
				fp->metadata = metadata_list_append(fp->metadata, "pubDate", tmp);
				feed_set_time(fp, parseRFC822Date(tmp));
				g_free(tmp);
			}
		} 
		else if(!xmlStrcmp(cur->name, BAD_CAST"ttl")) {
 			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE));
 			if(NULL != tmp) {
				feed_set_default_update_interval(fp, atoi(tmp));
				feed_set_update_interval(fp, atoi(tmp));
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
 			tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)));
 			if(NULL != tmp) {
				feed_set_title(fp, tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
 			tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)));
 			if(NULL != tmp) {
				feed_set_html_url(fp, tmp);
				g_free(tmp);
			}
		}
		else if(!xmlStrcmp(cur->name, BAD_CAST"description")) {
 			tmp = convertToHTML(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, TRUE)));
 			if(NULL != tmp) {
				feed_set_description(fp, tmp);
				g_free(tmp);
			}
		}
		cur = cur->next;
	}
}

static gchar* parseTextInput(xmlNodePtr cur) {
	gchar	*tmp, *buffer = NULL, *tiLink = NULL, *tiName = NULL, *tiDescription = NULL, *tiTitle = NULL;
	
	g_assert(NULL != cur);

	cur = cur->xmlChildrenNode;
	while(cur != NULL) {
		if (cur->type == XML_ELEMENT_NODE) {
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp) {
				if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
					g_free(tiTitle);
					tiTitle = tmp;
				} else if(!xmlStrcmp(cur->name, BAD_CAST"description")) {
					g_free(tiDescription);
					tiDescription = tmp;
				} else if(!xmlStrcmp(cur->name, BAD_CAST"name")) {
					g_free(tiName);
					tiName = tmp;
				} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
					g_free(tiLink);
					tiLink = tmp;
				} else
					g_free(tmp);
			}
		}
		cur = cur->next;
	}
	
	/* some postprocessing */
	tiTitle = unhtmlize(tiTitle);
	tiDescription = unhtmlize(tiDescription);

	if((NULL != tiLink) && (NULL != tiName) && 
	   (NULL != tiDescription) && (NULL != tiTitle)) {
		addToHTMLBufferFast(&buffer, "<p>");
		addToHTMLBufferFast(&buffer, tiDescription);
		addToHTMLBufferFast(&buffer, TEXT_INPUT_FORM_START);
		addToHTMLBufferFast(&buffer, tiLink);
		addToHTMLBufferFast(&buffer, TEXT_INPUT_TEXT_FIELD);
		addToHTMLBufferFast(&buffer, tiName);
		addToHTMLBufferFast(&buffer, TEXT_INPUT_SUBMIT);
		addToHTMLBufferFast(&buffer, tiTitle);
		addToHTMLBufferFast(&buffer, TEXT_INPUT_FORM_END);
		addToHTMLBufferFast(&buffer, "</p>");
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
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					return tmp;
				}
			}
		}
		cur = cur->next;
	}
	return NULL;
}

/* reads a RSS feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void rss_parse(feedPtr fp, xmlDocPtr doc, xmlNodePtr cur) {
	xmlNodePtr	item;
	itemPtr 	ip;
	GList		*items = NULL;
	gchar		*tmp;
	short 		rdf = 0;
	int 		error = 0;
	
	feed_set_time(fp, time(NULL));

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
		addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not find RDF/RSS header!</p>"));
		xmlFreeDoc(doc);
		error = 1;
	}

	if(!error) {	
		while(cur && xmlIsBlankNode(cur)) {
			cur = cur->next;
		}
		
		while(cur != NULL) {
			if(NULL == cur->name) {
				g_warning("invalid XML: parser returns NULL value -> tag ignored!");
				cur = cur->next;
				continue;
			}

			if((!xmlStrcmp(cur->name, BAD_CAST"channel")) || 
			   (!xmlStrcmp(cur->name, BAD_CAST"Channel"))) {
				parseChannel(fp, cur);
				g_assert(NULL != cur);
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
		while(cur != NULL) {
			if(cur->type != XML_ELEMENT_NODE || NULL == cur->name) {
				cur = cur->next;
				continue;
			}

			/* save link to channel image */
			if((!xmlStrcmp(cur->name, BAD_CAST"image"))) {
				tmp = parseImage(cur);
				feed_set_image_url(fp, tmp);
				g_free(tmp);
				
			} else if((!xmlStrcmp(cur->name, BAD_CAST"textinput")) ||
			          (!xmlStrcmp(cur->name, BAD_CAST"textInput"))) {
				/* no matter if we parse Userland or Netscape, there should be
				   only one text[iI]nput per channel and parsing the rdf:ressource
				   one should not harm */
				if(NULL != (tmp = parseTextInput(cur)))
					fp->metadata = metadata_list_append(fp->metadata, "textInput", tmp);
				g_free(tmp);
				
			} else if((!xmlStrcmp(cur->name, BAD_CAST"items"))) { /* RSS 1.1 */
				item = cur->xmlChildrenNode;
				while(NULL != item) {
					if(NULL != (ip = parseRSSItem(fp, item))) {
						if(0 == item_get_time(ip))
							item_set_time(ip, feed_get_time(fp));
						items = g_list_append(items, ip);
					}
					item = item->next;
				}
			} else if((!xmlStrcmp(cur->name, BAD_CAST"item"))) { /* RSS 1.0, 2.0 */
				/* collect channel items */
				if(NULL != (ip = parseRSSItem(fp, cur))) {
					if(0 == item_get_time(ip))
						item_set_time(ip, feed_get_time(fp));
					items = g_list_append(items, ip);
				}
				
			}
			cur = cur->next;
		}
	}
	/* after parsing we fill in the infos into the feedPtr structure */		
	feed_add_items(fp, items);
	
	if(0 == error) {
		feed_set_available(fp, TRUE);
	} else {
		ui_mainwindow_set_status_bar(_("There were errors while parsing this feed!"));
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

static void rss_add_ns_handler(NsHandler *handler) {

	g_assert(NULL != rss_nstable);
	g_hash_table_insert(rss_nstable, handler->prefix, handler);
	g_assert(handler->registerNs != NULL);
	handler->registerNs(handler, rss_nstable, ns_rss_ns_uri_table);
}

feedHandlerPtr rss_init_feed_handler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);

	/* Note: the tag mapping definitions and namespace registration
	   infos are shared with rss_item.c */
	
	if(RssToMetadataMapping == NULL) {
		RssToMetadataMapping = g_hash_table_new(g_str_hash, g_str_equal);
		g_hash_table_insert(RssToMetadataMapping, "copyright", "copyright");
		g_hash_table_insert(RssToMetadataMapping, "category", "category");
		g_hash_table_insert(RssToMetadataMapping, "webMaster", "webmaster");
		g_hash_table_insert(RssToMetadataMapping, "language", "language");
		g_hash_table_insert(RssToMetadataMapping, "managingEditor", "managingEditor");
		g_hash_table_insert(RssToMetadataMapping, "lastBuildDate", "contentUpdateDate");
		g_hash_table_insert(RssToMetadataMapping, "generator", "feedgenerator");
		g_hash_table_insert(RssToMetadataMapping, "publisher", "webmaster");
		g_hash_table_insert(RssToMetadataMapping, "author", "author");
		g_hash_table_insert(RssToMetadataMapping, "comments", "commentsUri");
	}
	
	if(NULL == rss_nstable) {
		rss_nstable = g_hash_table_new(g_str_hash, g_str_equal);
		ns_rss_ns_uri_table = g_hash_table_new(g_str_hash, g_str_equal);
		
		/* register RSS name space handlers */
		rss_add_ns_handler(ns_bC_getRSSNsHandler());
		rss_add_ns_handler(ns_dc_getRSSNsHandler());
		rss_add_ns_handler(ns_fm_getRSSNsHandler());	
  		rss_add_ns_handler(ns_slash_getRSSNsHandler());
		rss_add_ns_handler(ns_content_getRSSNsHandler());
		rss_add_ns_handler(ns_syn_getRSSNsHandler());
		rss_add_ns_handler(ns_admin_getRSSNsHandler());
		rss_add_ns_handler(ns_ag_getRSSNsHandler());
		rss_add_ns_handler(ns_cC_getRSSNsHandler());
		rss_add_ns_handler(ns_photo_getRSSNsHandler());
		rss_add_ns_handler(ns_pb_getRSSNsHandler());
	}
							
	/* prepare feed handler structure */
	fhp->typeStr = "rss";
	fhp->icon = ICON_AVAILABLE;
	fhp->directory = FALSE;
	fhp->feedParser	= rss_parse;
	fhp->checkFormat = rss_format_check;
	fhp->merge = TRUE;
	
	return fhp;
}
