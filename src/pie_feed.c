/**
 * @file pie_feed.c Atom/Echo/PIE 0.2/0.3 channel parsing
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
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "conf.h"
#include "common.h"
#include "feed.h"
#include "pie_feed.h"
#include "pie_entry.h"
#include "ns_dc.h"
#include "callbacks.h"
#include "htmlview.h"
#include "metadata.h"

/* to store the PIENsHandler structs for all supported RDF namespace handlers */
GHashTable	*pie_nstable = NULL;
GHashTable	*ns_pie_ns_uri_table = NULL;

/* note: the tag order has to correspond with the PIE_FEED_* defines in the header file */
/*
  The follow are not used, but had been recognized:
                                   "language", <---- Not in atom 0.2 or 0.3. We should use xml:lang
							"lastBuildDate", <--- Where is this from?
							"issued", <-- Not in the specs for feeds
							"created",  <---- Not in the specs for feeds
*/

gchar* pie_parse_content_construct(xmlNodePtr cur) {
	gchar	*mode, *type, *tmp, *ret;

	g_assert(NULL != cur);
	ret = NULL;
	
	/* determine encoding mode */
	mode = utf8_fix(xmlGetNoNsProp(cur, BAD_CAST"mode"));
	if(NULL != mode) {
		if(!strcmp(mode, "escaped")) {
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp)
				ret = tmp;
			
		} else if(!strcmp(mode, "xml")) {
			ret = extractHTMLNode(cur);
			
		} else if(!strcmp(mode, "base64")) {
			g_warning("Base64 encoded <content> in Atom feeds not supported!\n");
			
		} else if(!strcmp(mode, "multipart/alternative")) {
			if(NULL != cur->xmlChildrenNode)
				ret = pie_parse_content_construct(cur->xmlChildrenNode);
		}
		g_free(mode);
	} else {
		/* some feeds don'ts specify a mode but a MIME 
		   type in the type attribute... */
		type = utf8_fix(xmlGetNoNsProp(cur, BAD_CAST"type"));			
		/* not sure what MIME types are necessary... */
		if((NULL == type) ||
		   !strcmp(type, "text/html") ||
		   !strcmp(type, "text/plain") ||
		   !strcmp(type, "application/xhtml+xml")) {
			ret = extractHTMLNode(cur->xmlChildrenNode);
		}
		g_free(type);
	}
	
	return ret;
}


/* nonstatic because used by pie_entry.c too */
gchar * parseAuthor(xmlNodePtr cur) {
	gchar	*tmp = NULL;
	gchar	*tmp2, *tmp3;

	g_assert(NULL != cur);
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if(NULL == cur->name) {
			g_warning("invalid XML: parser returns NULL value -> tag ignored!");
			cur = cur->next;
			continue;
		}
		
		if (!xmlStrcmp(cur->name, BAD_CAST"name"))
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));

		if (!xmlStrcmp(cur->name, BAD_CAST"email")) {
			tmp2 = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			tmp3 = g_strdup_printf("%s <a href=\"mailto:%s\">%s</a>", tmp, tmp2, tmp2);
			g_free(tmp);
			g_free(tmp2);
			tmp = tmp3;
		}
					
		if (!xmlStrcmp(cur->name, BAD_CAST"url")) {
			tmp2 = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			tmp3 = g_strdup_printf("%s (<a href=\"%s\">Website</a>)", tmp, tmp2);
			g_free(tmp);
			g_free(tmp2);
			tmp = tmp3;
		}
		cur = cur->next;
	}

	return tmp;
}

/* reads a PIE feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void pie_parse(feedPtr fp, xmlDocPtr doc, xmlNodePtr cur) {
	itemPtr 		ip;
	gchar			*tmp2, *tmp = NULL, *tmp3;
	int 			error = 0;
	NsHandler		*nsh;
	parseChannelTagFunc	pf;
	
	while(TRUE) {
		if(xmlStrcmp(cur->name, BAD_CAST"feed")) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not find Atom/Echo/PIE header!</p>"));
			error = 1;
			break;			
		}

		/* parse feed contents */
		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			if(NULL == cur->name || cur->type != XML_ELEMENT_NODE) {
				cur = cur->next;
				continue;
			}
			
			/* check namespace of this tag */
			if(NULL != cur->ns) {
				if(((cur->ns->href != NULL) &&
				    NULL != (nsh = (NsHandler *)g_hash_table_lookup(ns_pie_ns_uri_table, (gpointer)cur->ns->href))) ||
				   ((cur->ns->prefix != NULL) &&
				    NULL != (nsh = (NsHandler *)g_hash_table_lookup(pie_nstable, (gpointer)cur->ns->prefix)))) {
					pf = nsh->parseChannelTag;
					if(NULL != pf)
						(*pf)(fp, cur);
					cur = cur->next;
					continue;
				} else {
					/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
				}
			} /* explicitly no following else !!! */
			
			if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
				tmp = unhtmlize(utf8_fix(pie_parse_content_construct(cur)));
				if (tmp != NULL)
					feed_set_title(fp, tmp);
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
				if(NULL != (tmp = utf8_fix(xmlGetProp(cur, BAD_CAST"href")))) {
					/* 0.3 link : rel, type and href attribute */
					tmp2 = utf8_fix(xmlGetProp(cur, BAD_CAST"rel"));
					if(tmp2 != NULL && !xmlStrcmp(tmp2, BAD_CAST"alternate"))
						feed_set_html_url(fp, tmp);
					else
						/* FIXME: Maybe do something with other links? */;
					g_free(tmp2);
					g_free(tmp);
				} else {
					/* 0.2 link : element content is the link, or non-alternate link in 0.3 */
					tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
					if(NULL != tmp)
						feed_set_html_url(fp, tmp);
					g_free(tmp);
				}
				
			/* parse feed author */
			} else if(!xmlStrcmp(cur->name, BAD_CAST"author")) {
				/* parse feed author */
				tmp = parseAuthor(cur);
				fp->metadata = metadata_list_append(fp->metadata, "author", tmp);
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"tagline")) {
				tmp = convertToHTML(utf8_fix(pie_parse_content_construct(cur)));
				if (tmp != NULL)
					feed_set_description(fp, tmp);
				g_free(tmp);				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"generator")) {
				tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
				if (tmp != NULL && tmp[0] != '\0') {
					tmp2 = utf8_fix(xmlGetProp(cur, BAD_CAST"version"));
					if (tmp2 != NULL) {
						tmp3 = g_strdup_printf("%s %s", tmp, tmp2);
						g_free(tmp);
						g_free(tmp2);
						tmp = tmp3;
					}
					tmp2 = utf8_fix(xmlGetProp(cur, BAD_CAST"url"));
					if (tmp2 != NULL) {
						tmp3 = g_strdup_printf("<a href=\"%s\">%s</a>", tmp2, tmp);
						g_free(tmp2);
						g_free(tmp);
						tmp = tmp3;
					}
					fp->metadata = metadata_list_append(fp->metadata, "feedgenerator", tmp);
				}
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"copyright")) {
				tmp = utf8_fix(pie_parse_content_construct(cur));
				if(NULL != tmp)
					fp->metadata = metadata_list_append(fp->metadata, "copyright", tmp);
				g_free(tmp);
				
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"modified")) {
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					fp->metadata = metadata_list_append(fp->metadata, "pubDate", tmp);
					feed_set_time(fp, parseISO8601Date(tmp));
					g_free(tmp);
				}

			} else if(!xmlStrcmp(cur->name, BAD_CAST"contributor")) {
				/* parse feed contributors */
				tmp = parseAuthor(cur);
				fp->metadata = metadata_list_append(fp->metadata, "contributor", tmp);
				g_free(tmp);
				
			} else if ((!xmlStrcmp(cur->name, BAD_CAST"entry"))) {
				if(NULL != (ip = parseEntry(fp, cur))) {
					if(0 == item_get_time(ip))
						item_set_time(ip, feed_get_time(fp));
					feed_add_item(fp, ip);
				}
			}
			
			/* collect PIE feed entries */
			cur = cur->next;
		}
		
		
		/* after parsing we fill in the infos into the feedPtr structure */		
		if(0 == error) {
			feed_set_available(fp, TRUE);
		} else {
			ui_mainwindow_set_status_bar(_("There were errors while parsing this feed!"));
		}

		break;
	}
}

static gboolean pie_format_check(xmlDocPtr doc, xmlNodePtr cur) {
	if(!xmlStrcmp(cur->name, BAD_CAST"feed")) {
		return TRUE;
	}
	return FALSE;
}

static void pie_add_ns_handler(NsHandler *handler) {

	g_assert(NULL != pie_nstable);
	if(getNameSpaceStatus(handler->prefix)) {
		g_hash_table_insert(pie_nstable, handler->prefix, handler);
		g_assert(handler->registerNs != NULL);
		handler->registerNs(handler, pie_nstable, ns_pie_ns_uri_table);
	}
}

feedHandlerPtr pie_init_feed_handler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);
	
	if(NULL == pie_nstable) {
		pie_nstable = g_hash_table_new(g_str_hash, g_str_equal);
		ns_pie_ns_uri_table = g_hash_table_new(g_str_hash, g_str_equal);
		
		/* register RSS name space handlers */
		pie_add_ns_handler(ns_dc_getRSSNsHandler());
	}	


	/* prepare feed handler structure */
	fhp->typeStr = "pie";
	fhp->icon = ICON_AVAILABLE;
	fhp->directory = FALSE;
	fhp->feedParser	= pie_parse;
	fhp->checkFormat = pie_format_check;
	fhp->merge = TRUE;

	return fhp;
}

