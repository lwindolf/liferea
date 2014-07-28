/**
 * @file pie_feed.c Atom 0.3 channel parsing
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

#include "pie_feed.h"

#include <sys/time.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>

#include "common.h"
#include "date.h"
#include "feed_parser.h"
#include "feedlist.h"
#include "pie_entry.h"
#include "ns_dc.h"
#include "metadata.h"
#include "xml.h"

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
	gchar	*mode, *type, *ret;

	g_assert(NULL != cur);
	ret = NULL;
	
	/* determine encoding mode */
	mode = xml_get_attribute (cur, "mode");
	type = xml_get_attribute (cur, "type");

	/* Modes are used in older versions of ATOM, including 0.3. It
	   does not exist in the newer IETF drafts.*/
	if(NULL != mode) {
		if(!strcmp(mode, "escaped")) {
			gchar	*tmp;

			tmp = xhtml_extract (cur, 0, NULL);
			if(NULL != tmp)
				ret = tmp;
			
		} else if(!strcmp(mode, "xml")) {
			ret = xhtml_extract (cur, 1,NULL);
			
		} else if(!strcmp(mode, "base64")) {
			g_warning("Base64 encoded <content> in Atom feeds not supported!\n");
			
		} else if(!strcmp(mode, "multipart/alternative")) {
			if(NULL != cur->xmlChildrenNode)
				ret = pie_parse_content_construct(cur->xmlChildrenNode);
		}
		g_free(mode);
	} else {
		/* some feeds don'ts specify a mode but a MIME type in the
		   type attribute... */
		/* not sure what MIME types are necessary... */

		/* This that need to be de-encoded and should not contain sub-tags.*/
		if(NULL == type ||
			!g_ascii_strcasecmp(type, "TEXT") ||
			!strcmp(type, "text/plain")) {
			gchar *tmp;
			tmp = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
			ret = g_markup_printf_escaped("<div xmlns=\"http://www.w3.org/1999/xhtml\"><pre>%s</pre></div>", tmp);
			g_free(tmp);
			/* Next are things that contain subttags */
		} else if(!g_ascii_strcasecmp(type, "HTML") ||
		          !strcmp(type, "text/html")) {
			ret = xhtml_extract (cur, 0,"http://default.base.com/");
		} else if(/* HTML types */
		          !g_ascii_strcasecmp(type, "xhtml") ||
		          !strcmp(type, "application/xhtml+xml")) {
			ret = xhtml_extract (cur, 1,"http://default.base.com/");
		}
	}
	/* If the type was text, everything must be now escaped and
	   wrapped in pre tags.... Also, the atom 0.3 spec says that the
	   default type MUST be considered to be text/plain. The type tag
	   is required in 0.2.... */
	//if (ret != NULL && (type == NULL || !strcmp(type, "text/plain") || !strcmp(type,"TEXT")))) {
	g_free(type);
	
	return ret;
}


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
			tmp = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);

		if (!xmlStrcmp(cur->name, BAD_CAST"email")) {
			tmp2 = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
			tmp3 = g_strdup_printf("%s <a href=\"mailto:%s\">%s</a>", tmp, tmp2, tmp2);
			g_free(tmp);
			g_free(tmp2);
			tmp = tmp3;
		}
					
		if (!xmlStrcmp(cur->name, BAD_CAST"url")) {
			tmp2 = (gchar *)xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1);
			tmp3 = g_strdup_printf("%s (<a href=\"%s\">%s</a>)", tmp, tmp2, _("Website"));
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
static void pie_parse(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	gchar			*tmp2, *tmp = NULL, *tmp3;
	NsHandler		*nsh;
	parseChannelTagFunc	pf;
	
	while(TRUE) {
		if(xmlStrcmp(cur->name, BAD_CAST"feed")) {
			g_string_append(ctxt->feed->parseErrors, "<p>Could not find Atom/Echo/PIE header!</p>");
			break;			
		}

		/* parse feed contents */
		cur = cur->xmlChildrenNode;
		while(cur) {
			if(!cur->name || cur->type != XML_ELEMENT_NODE) {
				cur = cur->next;
				continue;
			}
			
			/* check namespace of this tag */
			if(cur->ns) {
				if((cur->ns->href && (nsh = (NsHandler *)g_hash_table_lookup(ns_pie_ns_uri_table, (gpointer)cur->ns->href))) ||
				   (cur->ns->prefix && (nsh = (NsHandler *)g_hash_table_lookup(pie_nstable, (gpointer)cur->ns->prefix)))) {
					pf = nsh->parseChannelTag;
					if(pf)
						(*pf)(ctxt, cur);
					cur = cur->next;
					continue;
				} else {
					/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
				}
			} /* explicitly no following else !!! */
			
			if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
				tmp = unhtmlize(pie_parse_content_construct(cur));
				if(tmp) {
					if(ctxt->title)
						g_free(ctxt->title);
					ctxt->title = tmp;
				}
			} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
				tmp = xml_get_attribute (cur, "href");
				if(tmp) {				
					/* 0.3 link : rel, type and href attribute */
					tmp2 = xml_get_attribute (cur, "rel");
					if(tmp2 && g_str_equal(tmp2, "alternate"))
						subscription_set_homepage (ctxt->subscription, tmp);
					/* else
						FIXME: Maybe do something with other links? */
					g_free(tmp2);
					g_free(tmp);
				} else {
					/* 0.2 link : element content is the link, or non-alternate link in 0.3 */
					tmp = (gchar *)xmlNodeListGetString(ctxt->doc, cur->xmlChildrenNode, 1);
					if(tmp) {
						subscription_set_homepage (ctxt->subscription, tmp);
						g_free(tmp);
					}
				}
				
			/* parse feed author */
			} else if(!xmlStrcmp(cur->name, BAD_CAST"author")) {
				/* parse feed author */
				tmp = parseAuthor(cur);
				if(tmp) {
					ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "author", tmp);
					g_free(tmp);
				}
			} else if (!xmlStrcmp (cur->name, BAD_CAST"tagline")) {
				tmp = pie_parse_content_construct (cur);
				if (tmp) {
					metadata_list_set (&ctxt->subscription->metadata, "description", tmp);
					g_free (tmp);				
				}
			} else if(!xmlStrcmp(cur->name, BAD_CAST"generator")) {
				tmp = unhtmlize((gchar *)xmlNodeListGetString(ctxt->doc, cur->xmlChildrenNode, 1));
				if(tmp && tmp[0] != '\0') {
					tmp2 = xml_get_attribute (cur, "version");
					if(tmp2) {
						tmp3 = g_strdup_printf("%s %s", tmp, tmp2);
						g_free(tmp);
						g_free(tmp2);
						tmp = tmp3;
					}
					tmp2 = xml_get_attribute (cur, "url");
					if(tmp2) {
						tmp3 = g_strdup_printf("<a href=\"%s\">%s</a>", tmp2, tmp);
						g_free(tmp2);
						g_free(tmp);
						tmp = tmp3;
					}
					ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "feedgenerator", tmp);
				}
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"copyright")) {
				tmp = pie_parse_content_construct(cur);
				if(tmp) {
					ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "copyright", tmp);
					g_free(tmp);
				}				
				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"modified")) { /* Modified was last used in IETF draft 02) */
				tmp = (gchar *)xmlNodeListGetString(ctxt->doc, cur->xmlChildrenNode, 1);
				if(tmp) {
					ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "pubDate", tmp);
					ctxt->feed->time = date_parse_ISO8601 (tmp);
					g_free(tmp);
				}

			} else if(!xmlStrcmp(cur->name, BAD_CAST"updated")) { /* Updated was added in IETF draft 03 */
				tmp = (gchar *)xmlNodeListGetString(ctxt->doc, cur->xmlChildrenNode, 1);
				if(tmp) {
					ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "pubDate", tmp);
					ctxt->feed->time = date_parse_ISO8601 (tmp);
					g_free(tmp);
				}

			} else if(!xmlStrcmp(cur->name, BAD_CAST"contributor")) { 
				tmp = parseAuthor(cur);
				if(tmp) {
					ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "contributor", tmp);
					g_free(tmp);
				}
				
			} else if((!xmlStrcmp(cur->name, BAD_CAST"entry"))) {
				ctxt->item = parseEntry(ctxt, cur);
				if(ctxt->item) {
					if(0 == ctxt->item->time)
						ctxt->item->time = ctxt->feed->time;
					ctxt->items = g_list_append(ctxt->items, ctxt->item);
				}
			}
			
			cur = cur->next;
		}
		
		break;
	}
}

static gboolean pie_format_check(xmlDocPtr doc, xmlNodePtr cur) {

	if(!xmlStrcmp(cur->name, BAD_CAST"feed"))
		return TRUE;
	else
		return FALSE;
}

static void
pie_add_ns_handler (NsHandler *handler)
{
	g_assert (NULL != pie_nstable);
	g_hash_table_insert (pie_nstable, (gpointer)handler->prefix, handler);
	g_assert (handler->registerNs != NULL);
	handler->registerNs (handler, pie_nstable, ns_pie_ns_uri_table);
}

feedHandlerPtr
pie_init_feed_handler (void)
{
	feedHandlerPtr	fhp;
	
	fhp = g_new0 (struct feedHandler, 1);
	
	if (!pie_nstable) {
		pie_nstable = g_hash_table_new (g_str_hash, g_str_equal);
		ns_pie_ns_uri_table = g_hash_table_new (g_str_hash, g_str_equal);
		
		/* register RSS name space handlers */
		pie_add_ns_handler (ns_dc_get_handler ());
	}	


	/* prepare feed handler structure */
	fhp->typeStr = "atom";
	fhp->feedParser	= pie_parse;
	fhp->checkFormat = pie_format_check;

	return fhp;
}

