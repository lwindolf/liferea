/**
 * @file atom10.c Atom 1.0 Parser
 * 
 * Copyright (C) 2005 Nathan Conrad <t98502@users.sourceforge.net>
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

#include <sys/time.h>
#include <string.h>
#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include "support.h"
#include "conf.h"
#include "common.h"
#include "feed.h"
#include "ns_dc.h"
#include "callbacks.h"
#include "metadata.h"
#include "atom10.h"

#define ATOM10_NS BAD_CAST"http://www.w3.org/2005/Atom"

/* to store the ATOMNsHandler structs for all supported RDF namespace handlers */
GHashTable	*atom10_nstable = NULL;
GHashTable	*ns_atom10_ns_uri_table = NULL;

/* note: the tag order has to correspond with the ATOM10_FEED_* defines in the header file */
/*
  The follow are not used, but had been recognized:
                                   "language", <---- Not in atom 0.2 or 0.3. We should use xml:lang
							"lastBuildDate", <--- Where is this from?
							"issued", <-- Not in the specs for feeds
							"created",  <---- Not in the specs for feeds
*/

/**
 * This parses an Atom content construct.
 *
 * @param cur the parent node of the elements to be parsed.
 * @returns g_strduped string which must be freed by the caller.
 */

static gchar* atom10_parse_content_construct(xmlNodePtr cur) {
	gchar	*mode, *type, *tmp, *ret;

	g_assert(NULL != cur);
	ret = NULL;
	
	/* determine encoding mode */
	mode = utf8_fix(xmlGetProp(cur, BAD_CAST"mode"));
	type = utf8_fix(xmlGetProp(cur, BAD_CAST"type"));

	/* Modes are used in older versions of ATOM, including 0.3. It
	   does not exist in the newer IETF drafts.*/
	if(NULL != mode) {
		if(!strcmp(mode, "escaped")) {
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp)
				ret = tmp;
			
		} else if(!strcmp(mode, "xml")) {
			ret = extractHTMLNode(cur, TRUE);
			
		} else if(!strcmp(mode, "base64")) {
			g_warning("Base64 encoded <content> in Atom feeds not supported!\n");
			
		} else if(!strcmp(mode, "multipart/alternative")) {
			if(NULL != cur->xmlChildrenNode)
				ret = atom10_parse_content_construct(cur->xmlChildrenNode);
		}
		g_free(mode);
	} else {
		/* some feeds don'ts specify a mode but a MIME type in the
		   type attribute... */
		/* not sure what MIME types are necessary... */

		/* This that need to be de-encoded and should not contain sub-tags.*/
		if (NULL == type || (
						 !strcmp(type, "TEXT") ||
						 !strcmp(type, "text/plain") ||
						 !strcmp(type, "HTML") ||
						 !strcmp(type, "text/html"))) {
			ret = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			/* Next are things that contain subttags */
		} else if((NULL == type) ||
				/* HTML types */
				!strcmp(type, "XHTML") ||
				!strcmp(type, "application/xhtml+xml")) {
			/* Text types */
			ret = extractHTMLNode(cur, TRUE);
		}
	}
	/* If the type was text, everything must be now escaped and
	   wrapped in pre tags.... Also, the atom 0.3 spec says that the
	   default type MUST be considered to be text/plain. The type tag
	   is required in 0.2.... */
	//if (ret != NULL && (type == NULL || !strcmp(type, "text/plain") || !strcmp(type,"TEXT")))) {
	if((ret != NULL) && (type != NULL) && (!strcmp(type, "text/plain") || !strcmp(type,"TEXT"))) {
		gchar *tmp = g_markup_printf_escaped("<pre>%s</pre>", ret);
		g_free(ret);
		ret = tmp;
	}
	g_free(type);
	
	return ret;
}

/* This returns a escaped version of a text construct. If htmlified is
   set to 1, then HTML is returned. When set to 0, All HTML tags are
   removed.*/
static gchar* atom10_parse_text_construct(xmlNodePtr cur, gboolean htmlified) {
	gchar	*type, *tmp, *ret;

	g_assert(NULL != cur);
	ret = NULL;
	
	/* determine encoding mode */
	type = utf8_fix(xmlGetNsProp(cur, BAD_CAST"type", ATOM10_NS));

	/* not sure what MIME types are necessary... */
	
	/* This that need to be de-encoded and should not contain sub-tags.*/
	if (NULL == type || (!strcmp(type, "text") ||
					 !strcmp(type, "html"))) {
		ret = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		if (htmlified)
			tmp = g_markup_printf_escaped("<pre>%s</pre>", ret);
		else
			tmp = g_markup_printf_escaped("%s", ret);
		g_free(ret);
		ret = tmp;
	} else if(!strcmp(type, "xhtml")) {
		ret = utf8_fix(extractHTMLNode(cur, TRUE));
		if (!htmlified) {
			ret = unhtmlize(ret);
		}
			tmp = g_markup_printf_escaped("%s", ret);
		g_free(ret);
		ret = tmp;
		
	} else {
		/* Invalid ATOM feed */
		ret = g_strdup("This attribute was invalidly specified in this ATOM feed.");
	}
	
	g_free(type);
	
	return ret;
}


/* nonstatic because used by atom10_entry.c too */
static gchar * atom10_parse_person_construct(xmlNodePtr cur) {
	gchar	*tmp = NULL;
	gchar	*name = NULL, *uri = NULL, *email = NULL;
	gboolean invalid = FALSE;
	
	g_assert(NULL != cur);
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if(NULL == cur->name || cur->type != XML_ELEMENT_NODE || cur->ns == NULL) {
			cur = cur->next;
			continue;
		}
		
		if (!xmlStrcmp(cur->ns->href, ATOM10_NS)) {
			if (!xmlStrcmp(cur->name, BAD_CAST"name")) {
				g_free(name);
				name = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			}
			
			if (!xmlStrcmp(cur->name, BAD_CAST"email")) {
				if (email != NULL)
					invalid = TRUE;
				g_free(email);
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				email = g_strdup_printf(" - <a href=\"mailto:%s\">%s</a>", tmp, tmp);
				g_free(tmp);
			}
			
			if (!xmlStrcmp(cur->name, BAD_CAST"uri")) {
				if (uri == NULL)
					invalid = TRUE;
				g_free(uri);
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				uri = g_strdup_printf(" (<a href=\"%s\">Website</a>)", tmp);
				g_free(tmp);
			}
		} else {
			/* FIXME: handle extension elements here */
		}
		cur = cur->next;
	}
	if (name == NULL) {
		invalid = TRUE;
		name = g_strdup(_("Invalid ATOM FEED: unknown author"));
	}
	tmp = g_strdup_printf("%s%s%s", name, uri == NULL ? "" : uri, email == NULL ? "" : email);
	g_free(uri);
	g_free(email);
	g_free(name);
	return tmp;
}

/* <content> tag support, FIXME: base64 not supported */
/* method to parse standard tags for each item element */
static itemPtr atom10_parse_entry(feedPtr fp, xmlNodePtr cur) {
	gchar			*tmp2, *tmp;
	itemPtr			ip;
	GHashTable *nsinfos;
	NsHandler		*nsh;
	parseItemTagFunc	pf;
	
	g_assert(NULL != cur);
	
	nsinfos = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
	ip = item_new();
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		
		if (cur->type != XML_ELEMENT_NODE || cur->name == NULL) {
			cur = cur->next;
			continue;
		}
		
		/* check namespace of this tag */
		if(NULL == cur->ns) {
			/* This is an invalid feed... no idea what to do with the current element */
			printf("element with no namespace found!\n");
			cur = cur->next;
			continue;
		}
		
		if(NULL != cur->ns) {
			if(((cur->ns->href != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(ns_atom10_ns_uri_table, (gpointer)cur->ns->href))) ||
			   ((cur->ns->prefix != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(atom10_nstable, (gpointer)cur->ns->prefix)))) {
				
				pf = nsh->parseItemTag;
				if(NULL != pf)
					(*pf)(ip, cur);
				cur = cur->next;
				continue;
			} else {
				/*g_print("unsupported namespace \"%s\"\n", cur->ns->prefix);*/
			}
		} /* explicitly no following else !!! */
		
		if (cur->ns->href == NULL || xmlStrcmp(cur->ns->href, ATOM10_NS) != 0) {
			printf("unknown namespace found in feed\n");
			cur = cur->next;
			continue;
		}
		/* At this point, the namespace must be the ATOM 1.0 namespace */
		
		if(!xmlStrcmp(cur->name, BAD_CAST"author")) {
			/* parse feed author */
			tmp = atom10_parse_person_construct(cur);
			ip->metadata = metadata_list_append(ip->metadata, "author", tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"category")) { 
			tmp = NULL;
			if (xmlHasNsProp(cur, BAD_CAST"label", ATOM10_NS)) {
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			} else if (xmlHasNsProp(cur, BAD_CAST"term", ATOM10_NS)) {
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			}
			if (tmp != NULL) {
				tmp2 = g_markup_escape_text(tmp, -1);
				ip->metadata = metadata_list_append(ip->metadata, "category", tmp2);
				g_free(tmp2);
				xmlFree(tmp);
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"content")) {
			/* <content> support */
			gchar *tmp = utf8_fix(atom10_parse_content_construct(cur));
			if (tmp != NULL)
				item_set_description(ip, tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"contributor")) {
			/* FIXME: parse feed contributors */
			tmp = atom10_parse_person_construct(cur);
			ip->metadata = metadata_list_append(ip->metadata, "contributor", tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"id")) {
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if (tmp != NULL)
				item_set_id(ip, tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
			if(NULL != (tmp = utf8_fix(xmlGetNsProp(cur, BAD_CAST"href", ATOM10_NS)))) {
				tmp2 = utf8_fix(xmlGetNsProp(cur, BAD_CAST"rel", ATOM10_NS));
				if(!xmlHasNsProp(cur, BAD_CAST"rel", ATOM10_NS) || tmp2 == NULL || !xmlStrcmp(tmp2, BAD_CAST"alternate"))
					feed_set_html_url(fp, tmp);
				else if (!xmlStrcmp(tmp2, BAD_CAST"enclosure")) {
					/* FIXME: Display the human readable title from the property "title" */
					/* FIXME: Use xml:base, see "xmlChar *xmlNodeGetBase(xmlDocPtr doc, xmlNodePtr cur)" */
					/* This current code was copied from the RSS parser.*/
					if((strstr(tmp, "://") == NULL) &&
					   (fp->htmlUrl != NULL) &&
					   (fp->htmlUrl[0] != '|') &&
					   (strstr(fp->htmlUrl, "://") != NULL)) {
						/* add base URL if necessary and possible */
						gchar *tmp3 = g_strdup_printf("%s/%s", fp->htmlUrl, tmp);
						g_free(tmp);
						tmp = tmp3;
					}
					/* FIXME: Verify URL is not evil... */
					ip->metadata = metadata_list_append(ip->metadata, "enclosure", tmp);
				} else
					/* FIXME: Maybe do something with other links such as "related" and add metadata for "via"? */;
				g_free(tmp2);
				g_free(tmp);
			} 
		} else if(!xmlStrcmp(cur->name, BAD_CAST"published")) {
 			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
 			if(NULL != tmp) {
				item_set_time(ip, parseISO8601Date(tmp));
				ip->metadata = metadata_list_append(ip->metadata, "pubDate", tmp);
				g_free(tmp);
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"rights")) {
			tmp = utf8_fix(atom10_parse_text_construct(cur, FALSE));
			if(NULL != tmp)
				ip->metadata = metadata_list_append(ip->metadata, "copyright", tmp);
			g_free(tmp);
			/* FIXME: Parse "source" */
		} else if(!xmlStrcmp(cur->name, BAD_CAST"summary")) {			
			/* <summary> can be used for short text descriptions, if there is no
			   <content> description we show the <summary> content */
			if (NULL == item_get_description(ip)) {
				tmp = utf8_fix(atom10_parse_text_construct(cur, TRUE));
				if(NULL != tmp)
					item_set_description(ip, tmp);
				g_free(tmp);
			}
		} else if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
			tmp = unhtmlize(utf8_fix(atom10_parse_text_construct(cur, FALSE)));
			if (tmp != NULL)
				item_set_title(ip, tmp);
			g_free(tmp);
		} else if(!xmlStrcmp(cur->name, BAD_CAST"updated")) {
			tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if(NULL != tmp) {
				item_set_time(ip, parseISO8601Date(tmp));
				ip->metadata = metadata_list_append(ip->metadata, "contentUpdateDate", tmp);
			}
			g_free(tmp);
		}
		cur = cur->next;
	}
	
	/* after parsing we fill the infos into the itemPtr structure */
	ip->readStatus = FALSE;
	
	g_hash_table_destroy(nsinfos);
	
	if(0 == item_get_time(ip))
		item_set_time(ip, feed_get_time(fp));
	
	return ip;
}

/* reads a Atom feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void atom10_parse_feed(feedPtr fp, xmlDocPtr doc, xmlNodePtr cur) {
	itemPtr 		ip;
	GList			*items = NULL;
	gchar			*tmp2, *tmp = NULL, *tmp3;
	int 			error = 0;
	NsHandler		*nsh;
	parseChannelTagFunc	pf;

	while(TRUE) {
		if(xmlStrcmp(cur->name, BAD_CAST"feed")) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not find Atom 1.0 header!</p>"));
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
			if(NULL == cur->ns) {
				/* This is an invalid feed... no idea what to do with the current element */
				printf("element with no namespace found!\n");
				cur = cur->next;
				continue;
			}
			
			if(((cur->ns->href != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(ns_atom10_ns_uri_table, (gpointer)cur->ns->href))) ||
			   ((cur->ns->prefix != NULL) &&
			    NULL != (nsh = (NsHandler *)g_hash_table_lookup(atom10_nstable, (gpointer)cur->ns->prefix)))) {
				pf = nsh->parseChannelTag;
				if(NULL != pf)
					(*pf)(fp, cur);
				cur = cur->next;
				continue;
			}
			
			if (xmlStrcmp(cur->ns->href, ATOM10_NS) != 0) {
				printf("unknown namespace found in feed\n");
				cur = cur->next;
				continue;
			}
			/* At this point, the namespace must be the ATOM 1.0 namespace */
			
			if(!xmlStrcmp(cur->name, BAD_CAST"author")) {
				/* parse feed author */
				tmp = atom10_parse_person_construct(cur);
				fp->metadata = metadata_list_append(fp->metadata, "author", tmp);
				g_free(tmp);
				/* FIXME: make item parsing use this author if not specified elsewhere */
			} else if(!xmlStrcmp(cur->name, BAD_CAST"category")) { 
				tmp = NULL;
				if (xmlHasNsProp(cur, BAD_CAST"label", ATOM10_NS)) {
					tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				} else if (xmlHasNsProp(cur, BAD_CAST"term", ATOM10_NS)) {
					tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				}
				if (tmp != NULL) {
					tmp2 = g_markup_escape_text(tmp, -1);
					fp->metadata = metadata_list_append(fp->metadata, "category", tmp2);
					g_free(tmp2);
					xmlFree(tmp);
				}
			} else if(!xmlStrcmp(cur->name, BAD_CAST"contributor")) { 
				/* parse feed contributors */
				tmp = atom10_parse_person_construct(cur);
				fp->metadata = metadata_list_append(fp->metadata, "contributor", tmp);
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"generator")) {
				tmp = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
				if (tmp != NULL && tmp[0] != '\0') {
					tmp2 = utf8_fix(xmlGetNsProp(cur, BAD_CAST"version", ATOM10_NS));
					if (tmp2 != NULL) {
						tmp3 = g_strdup_printf("%s %s", tmp, tmp2);
						g_free(tmp);
						g_free(tmp2);
						tmp = tmp3;
					}
					tmp2 = utf8_fix(xmlGetNsProp(cur, BAD_CAST"uri", ATOM10_NS));
					if (tmp2 != NULL) {
						tmp3 = g_strdup_printf("<a href=\"%s\">%s</a>", tmp2, tmp);
						g_free(tmp2);
						g_free(tmp);
						tmp = tmp3;
					}
					fp->metadata = metadata_list_append(fp->metadata, "feedgenerator", tmp);
				}
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"icon")) {
				/* FIXME: Parse icon and use as a favicon? */
			} else if(!xmlStrcmp(cur->name, BAD_CAST"id")) {
				/* FIXME: Parse ID, but I'm not sure where Liferea would use it */
			} else if(!xmlStrcmp(cur->name, BAD_CAST"link")) {
				if(NULL != (tmp = utf8_fix(xmlGetNsProp(cur, BAD_CAST"href", ATOM10_NS)))) {
					tmp2 = utf8_fix(xmlGetNsProp(cur, BAD_CAST"rel", ATOM10_NS));
					if(!xmlHasNsProp(cur, BAD_CAST"rel", ATOM10_NS) || tmp2 == NULL || !xmlStrcmp(tmp2, BAD_CAST"alternate"))
						feed_set_html_url(fp, tmp);
					else if (!xmlStrcmp(tmp2, BAD_CAST"enclosure")) {
						/* FIXME: Display the human readable title from the property "title" */
						/* FIXME: Use xml:base, see "xmlChar *xmlNodeGetBase(xmlDocPtr doc, xmlNodePtr cur)" */
						/* This current code was copied from the RSS parser.*/
						if((strstr(tmp, "://") == NULL) &&
						   (fp->htmlUrl != NULL) &&
						   (fp->htmlUrl[0] != '|') &&
						   (strstr(fp->htmlUrl, "://") != NULL)) {
							/* add base URL if necessary and possible */
							tmp3 = g_strdup_printf("%s/%s", fp->htmlUrl, tmp);
							g_free(tmp);
							tmp = tmp3;
						}
						/* FIXME: Verify URL is not evil... */
						fp->metadata = metadata_list_append(fp->metadata, "enclosure", tmp);
					} else
						/* FIXME: Maybe do something with other links such as "related" and add metadata for "via"? */;
					g_free(tmp2);
					g_free(tmp);
				} 
			} else if(!xmlStrcmp(cur->name, BAD_CAST"logo")) {
				tmp = utf8_fix(atom10_parse_text_construct(cur, FALSE));
				/* FIXME: Verify URL is not evil... */
				feed_set_image_url(fp, tmp);
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"rights")) {
				tmp = utf8_fix(atom10_parse_text_construct(cur, FALSE));
				if(NULL != tmp)
					fp->metadata = metadata_list_append(fp->metadata, "copyright", tmp);
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"subtitle")) {
				tmp = convertToHTML(utf8_fix(atom10_parse_text_construct(cur, TRUE)));
				if (tmp != NULL)
					feed_set_description(fp, tmp);
				g_free(tmp);				
			} else if(!xmlStrcmp(cur->name, BAD_CAST"title")) {
				tmp = unhtmlize(utf8_fix(atom10_parse_text_construct(cur, FALSE)));
				if (tmp != NULL)
					feed_set_title(fp, tmp);
				g_free(tmp);
			} else if(!xmlStrcmp(cur->name, BAD_CAST"updated")) { /* Updated was added in IETF draft 03 */
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				if(NULL != tmp) {
					fp->metadata = metadata_list_append(fp->metadata, "contentUpdateDate", tmp);
					feed_set_time(fp, parseISO8601Date(tmp));
					g_free(tmp);
				}
			} else if((!xmlStrcmp(cur->name, BAD_CAST"entry"))) {
				if(NULL != (ip = atom10_parse_entry(fp, cur))) {
					items = g_list_append(items, ip);
				}
			}
			cur = cur->next;
		}
		
		feed_add_items(fp, items);
		/* FIXME: Maybe check to see that the required information was actually provided (persuant to the RFC). */
		/* after parsing we fill in the infos into the feedPtr structure */		
		if(0 == error) {
			feed_set_available(fp, TRUE);
		} else {
			ui_mainwindow_set_status_bar(_("There were errors while parsing this feed!"));
		}
		
		break;
	}
}

static gboolean atom10_format_check(xmlDocPtr doc, xmlNodePtr cur) {
	if(!xmlStrcmp(cur->name, BAD_CAST"feed") && !xmlStrcmp(cur->ns->href, ATOM10_NS)) {
		return TRUE;
	}
	return FALSE;
}

static void atom10_add_ns_handler(NsHandler *handler) {

	g_assert(NULL != atom10_nstable);
	g_hash_table_insert(atom10_nstable, handler->prefix, handler);
	g_assert(handler->registerNs != NULL);
	handler->registerNs(handler, atom10_nstable, ns_atom10_ns_uri_table);
}

feedHandlerPtr atom10_init_feed_handler(void) {
	feedHandlerPtr	fhp;
	
	fhp = g_new0(struct feedHandler, 1);
	
	if(NULL == atom10_nstable) {
		atom10_nstable = g_hash_table_new(g_str_hash, g_str_equal);
		ns_atom10_ns_uri_table = g_hash_table_new(g_str_hash, g_str_equal);
		
		/* register RSS name space handlers */
		atom10_add_ns_handler(ns_dc_getRSSNsHandler());
	}	


	/* prepare feed handler structure */
	fhp->typeStr = "pie";
	fhp->icon = ICON_AVAILABLE;
	fhp->directory = FALSE;
	fhp->feedParser	= atom10_parse_feed;
	fhp->checkFormat = atom10_format_check;
	fhp->merge = TRUE;

	return fhp;
}

