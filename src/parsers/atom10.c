/**
 * @file atom10.c Atom 1.0 Parser
 * 
 * Copyright (C) 2005 Nathan Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
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
struct atom10ParserState {
	gboolean errorDetected;
};
typedef void 	(*atom10ElementParserFunc)	(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState *state);


/**
 * This parses an Atom content construct.
 *
 * @param cur the parent node of the elements to be parsed.
 * @returns g_strduped string which must be freed by the caller.
 */

static gchar* atom10_parse_content_construct(xmlNodePtr cur) {
	gchar *ret;

	g_assert(NULL != cur);
	ret = NULL;
	
	if (xmlHasNsProp(cur, BAD_CAST"src", NULL )) {
		xmlChar *src = xmlGetNsProp(cur, BAD_CAST"src", NULL);
		
		if (src == NULL) {
			ret = g_strdup(_("Liferea is unable to display this item's contents."));
		} else {
			gchar *baseURL = utf8_fix(xmlNodeGetBase(cur->doc, cur));
			gchar *url;
			
			url = common_build_url(src, baseURL);
			ret = g_strdup_printf(_("<p><a href=\"%s\">View this item's contents.</a></p>"), url);
			
			g_free(url);
			xmlFree(baseURL);
			xmlFree(src);
		}
	} else {
		gchar *type;
		gboolean escapeAsText, includeChildTags;
		xmlChar *baseURL = NULL;

		/* determine encoding mode */
		type = xmlGetNsProp(cur, BAD_CAST"type", NULL);
		
		/* This that need to be de-encoded and should not contain sub-tags.*/
		if (type != NULL && (g_str_equal(type,"html") || !g_strcasecmp(type, "text/html"))) {
			escapeAsText = FALSE;
			includeChildTags = FALSE;
			baseURL = xmlNodeGetBase(cur->doc, cur);
		} else if (NULL == type || !strcmp(type, "text") || !strncasecmp(type, "text/",5)) { /* Assume that "text/*" files can be directly displayed.. kinda stated in the RFC */
			escapeAsText = TRUE;
			includeChildTags = FALSE;
		} else if (!strcmp(type,"xhtml") || !strcasecmp(type, "application/xhtml+xml")) {
			escapeAsText = FALSE;
			includeChildTags = TRUE;
			/* The spec says to only show the contents of the div tag that MUST be present */
			cur = cur->children;
			while (cur != NULL) {
				if (cur->type == XML_ELEMENT_NODE && cur->name != NULL && xmlStrEqual(cur->name, BAD_CAST"div"))
					break;
				cur = cur->next;
			}
			if (cur == NULL) {
				g_free(type);
				return g_strdup(_("This item's contents is invalid."));
			}
			baseURL = xmlNodeGetBase(cur->doc, cur);
		}
			
		if (includeChildTags)
			ret = utf8_fix(extractHTMLNode(cur, TRUE));
		else
			ret = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		
		if (escapeAsText) {
			gchar *tmp = g_markup_printf_escaped("<pre>%s</pre>", ret);
			g_free(ret);
			ret = tmp;
		}
		if (baseURL) {
			gchar *tmp = g_strdup_printf("<div xml:base=\"%s\">%s</div>", baseURL, ret);
			g_free(ret);
			ret = tmp;
		}
		xmlFree(baseURL);
		g_free(type);
	}
	
	return ret;
}

/** @param htmlified If set to 1, then HTML is returned. When set
   to 0, All HTML tags are removed
   
   @returns an escaped version of a text construct. */
static gchar* atom10_parse_text_construct(xmlNodePtr cur, gboolean htmlified) {
	gchar	*type, *tmp, *ret;
	
	g_assert(NULL != cur);
	ret = NULL;
	
	/* determine encoding mode */
	type = utf8_fix(xmlGetNsProp(cur, BAD_CAST"type", NULL));
	
	/* not sure what MIME types are necessary... */
	
	/* This that need to be de-encoded and should not contain sub-tags.*/
	if (NULL == type || !strcmp(type, "text")) {
		ret = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		
		if (htmlified) {
			tmp = ret;
			ret = g_markup_printf_escaped("<pre>%s</pre>", tmp);
			g_free(tmp);
		}
	} else if (!strcmp(type, "html")) {
		ret = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		if (!htmlified)
			ret = unhtmlize(unxmlize(ret));
	} else if(!strcmp(type, "xhtml")) {
		/* The spec says to only show the contents of the div tag that MUST be present */
		
		cur = cur->children;
		while (cur != NULL) {
			if (cur->type == XML_ELEMENT_NODE && cur->name != NULL && xmlStrEqual(cur->name, BAD_CAST"div"))
				break;
			cur = cur->next;
		}
		
		if (cur == NULL)
			ret = g_strdup(_("This item's contents is invalid."));
		else
			ret = utf8_fix(extractHTMLNode(cur, TRUE));
		
		if (!htmlified)
			ret = unhtmlize(ret);
	} else {
		/* Invalid Atom feed */
		ret = g_strdup("This attribute was invalidly specified in this Atom feed.");
	}
	
	g_free(type);
	
	return ret;
}

static gchar * atom10_parse_person_construct(xmlNodePtr cur) {
	gchar	*tmp = NULL;
	gchar	*name = NULL, *uri = NULL, *email = NULL;
	gboolean invalid = FALSE;
	
	g_assert(NULL != cur);
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		if(NULL == cur->name || cur->type != XML_ELEMENT_NODE || cur->ns == NULL || cur->ns->href == NULL) {
			cur = cur->next;
			continue;
		}
		
		if (xmlStrEqual(cur->ns->href, ATOM10_NS)) {
			if (xmlStrEqual(cur->name, BAD_CAST"name")) {
				g_free(name);
				name = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			}
			
			if (xmlStrEqual(cur->name, BAD_CAST"email")) {
				if (email != NULL)
					invalid = TRUE;
				g_free(email);
				tmp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				email = g_strdup_printf(" - <a href=\"mailto:%s\">%s</a>", tmp, tmp);
				g_free(tmp);
			}
			
			if (xmlStrEqual(cur->name, BAD_CAST"uri")) {
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
		name = g_strdup(_("Invalid Atom feed: unknown author"));
	}
	tmp = g_strdup_printf("%s%s%s", name, uri == NULL ? "" : uri, email == NULL ? "" : email);
	g_free(uri);
	g_free(email);
	g_free(name);
	return tmp;
}

static void atom10_parse_entry_author(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *author;
	
	author = atom10_parse_person_construct(cur);
	ip->metadata = metadata_list_append(ip->metadata, "author", author);
	g_free(author);
}

static void atom10_parse_entry_category(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *category = NULL, *categoryEsc;
	if (xmlHasNsProp(cur, BAD_CAST"label", NULL)) {
		category = utf8_fix(xmlGetNsProp(cur, BAD_CAST"label", NULL));
	} else if (xmlHasNsProp(cur, BAD_CAST"term", NULL)) {
		category = utf8_fix(xmlGetNsProp(cur, BAD_CAST"term", NULL));
	}
	if (category != NULL) {
		categoryEsc = g_markup_escape_text(category, -1);
		ip->metadata = metadata_list_append(ip->metadata, "category", categoryEsc);
		g_free(categoryEsc);
		xmlFree(category);
	}
}

static void atom10_parse_entry_content(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *content = atom10_parse_content_construct(cur);
	if (content != NULL)
		item_set_description(ip, content);
	g_free(content);
}

static void atom10_parse_entry_contributor(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *contributor = atom10_parse_person_construct(cur);
	ip->metadata = metadata_list_append(ip->metadata, "contributor", contributor);
	g_free(contributor);
}

static void atom10_parse_entry_id(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *id = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			if (id != NULL)
				item_set_id(ip, id);
			g_free(id);
}

static void atom10_parse_entry_link(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *href;

	if(NULL != (href = utf8_fix(xmlGetNsProp(cur, BAD_CAST"href", NULL)))) {
		xmlChar *baseURL = xmlNodeGetBase(cur->doc, cur);
		gchar *url, *relation, *escTitle = NULL, *title;
		
		if (baseURL == NULL && fp->htmlUrl != NULL && fp->htmlUrl[0] != '|' &&
			strstr(fp->htmlUrl, "://") != NULL)
			baseURL = xmlStrdup(BAD_CAST(fp->htmlUrl));
		url = common_build_url(href, baseURL);
		
		relation = utf8_fix(xmlGetNsProp(cur, BAD_CAST"rel", NULL));
		title = utf8_fix(xmlGetNsProp(cur, BAD_CAST"title", NULL));
		if (title != NULL)
			escTitle = g_markup_escape_text(title, -1);
		/* FIXME: Display the human readable title from the property "title" */
		/* This current code was copied from the RSS parser.*/
		
		if(!xmlHasNsProp(cur, BAD_CAST"rel", NULL) || relation == NULL || g_str_equal(relation, "alternate"))
			item_set_source(ip, url);
		else if (g_str_equal(relation, "enclosure"))
			ip->metadata = metadata_list_append(ip->metadata, "enclosure", url);
		else
			/* FIXME: Maybe do something with other links such as "related" and add metadata for "via"? */;
		xmlFree(baseURL);
		xmlFree(title);
		g_free(escTitle);
		g_free(url);
		g_free(relation);
		g_free(href);
	} else
		/* FIXME: @href is required, this document is not valid Atom */;

}

static void atom10_parse_entry_published(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *datestr = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	if(NULL != datestr) {
		item_set_time(ip, parseISO8601Date(datestr));
		ip->metadata = metadata_list_append(ip->metadata, "pubDate", datestr);
		g_free(datestr);
	}
}

static void atom10_parse_entry_rights(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *rights = atom10_parse_text_construct(cur, FALSE);
	if(NULL != rights)
		ip->metadata = metadata_list_append(ip->metadata, "copyright", rights);
	g_free(rights);
}

/* <summary> can be used for short text descriptions, if there is no
   <content> description we show the <summary> content */
static void atom10_parse_entry_summary(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *summary;
	if (NULL == item_get_description(ip)) {
		summary = atom10_parse_text_construct(cur, TRUE);
		if(NULL != summary)
			item_set_description(ip, summary);
		g_free(summary);
		/* FIXME: set a flag to show a "Read more" link to the user; but where? */
	}
}

static void atom10_parse_entry_title(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *title = utf8_fix(atom10_parse_text_construct(cur, FALSE));
	if (title != NULL)
		item_set_title(ip, title);
	g_free(title);
}

static void atom10_parse_entry_updated(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *datestr = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	if(NULL != datestr) {
		item_set_time(ip, parseISO8601Date(datestr));
		ip->metadata = metadata_list_append(ip->metadata, "contentUpdateDate", datestr);
	}
	g_free(datestr);
}
/* <content> tag support, FIXME: base64 not supported */
/* method to parse standard tags for each item element */
static itemPtr atom10_parse_entry(feedPtr fp, xmlNodePtr cur) {
	itemPtr			ip;
	NsHandler		*nsh;
	parseItemTagFunc	pf;
	atom10ElementParserFunc func;
	static GHashTable *entryElementHash = NULL;
	
	g_assert(NULL != cur);
	
	if (entryElementHash == NULL) {
		entryElementHash = g_hash_table_new(g_str_hash, g_str_equal);
		
		g_hash_table_insert(entryElementHash, "author", &atom10_parse_entry_author);
		g_hash_table_insert(entryElementHash, "category", &atom10_parse_entry_category);
		g_hash_table_insert(entryElementHash, "content", &atom10_parse_entry_content);
		g_hash_table_insert(entryElementHash, "contributor", &atom10_parse_entry_contributor);
		g_hash_table_insert(entryElementHash, "id", &atom10_parse_entry_id);
		g_hash_table_insert(entryElementHash, "link", &atom10_parse_entry_link);
		g_hash_table_insert(entryElementHash, "published", &atom10_parse_entry_published);
		g_hash_table_insert(entryElementHash, "rights", &atom10_parse_entry_rights);
		/* FIXME: Parse "source" */
		g_hash_table_insert(entryElementHash, "summary", &atom10_parse_entry_summary);
		g_hash_table_insert(entryElementHash, "title", &atom10_parse_entry_title);
		g_hash_table_insert(entryElementHash, "updated", &atom10_parse_entry_updated);
	}	

	ip = item_new();
	
	cur = cur->xmlChildrenNode;
	while (cur != NULL) {
		
		if (cur->type != XML_ELEMENT_NODE || cur->name == NULL || cur->ns == NULL) {
			cur = cur->next;
			continue;
		}
		
		if(((cur->ns->href != NULL)   && (NULL != (nsh = (NsHandler *)g_hash_table_lookup(ns_atom10_ns_uri_table, (gpointer)cur->ns->href)))) ||
		   ((cur->ns->prefix != NULL) && (NULL != (nsh = (NsHandler *)g_hash_table_lookup(atom10_nstable, (gpointer)cur->ns->prefix))))) {
			
			pf = nsh->parseItemTag;
			if(NULL != pf)
				(*pf)(ip, cur);
			cur = cur->next;
			continue;
		}
		
		/* check namespace of this tag */
		if(NULL == cur->ns->href) {
			/* This is an invalid feed... no idea what to do with the current element */
			printf("element with no namespace found!\n");
			cur = cur->next;
			continue;
		}
		
		
		if (xmlStrcmp(cur->ns->href, ATOM10_NS) != 0) {
			printf("unknown namespace found in feed\n");
			cur = cur->next;
			continue;
		}
		/* At this point, the namespace must be the Atom 1.0 namespace */
		func = g_hash_table_lookup(entryElementHash, cur->name);
		if (func != NULL) {
			(*func)(cur, fp, ip, NULL);
		} else {
			printf("unknown entry element \"%s\" found in feed\n", cur->name);
		}
		
		cur = cur->next;
	}
	
	/* after parsing we fill the infos into the itemPtr structure */
	ip->readStatus = FALSE;
	
	if(0 == item_get_time(ip))
		item_set_time(ip, feed_get_time(fp));
	
	return ip;
}

static void atom10_parse_feed_author(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	/* parse feed author */
	gchar *author = atom10_parse_person_construct(cur);
	fp->metadata = metadata_list_append(fp->metadata, "author", author);
	g_free(author);
	/* FIXME: make item parsing use this author if not specified elsewhere */
}

static void atom10_parse_feed_category(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *label = NULL, *escaped;
	if (xmlHasNsProp(cur, BAD_CAST"label", NULL)) {
		label = utf8_fix(xmlGetNsProp(cur, BAD_CAST"label", NULL));
	} else if (xmlHasNsProp(cur, BAD_CAST"term", NULL)) {
		label = utf8_fix(xmlGetNsProp(cur, BAD_CAST"term", NULL));
	}
	if (label != NULL) {
		escaped = g_markup_escape_text(label, -1);
		fp->metadata = metadata_list_append(fp->metadata, "category", escaped);
		g_free(escaped);
		xmlFree(label);
	}
}

static void atom10_parse_feed_contributor(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	/* parse feed contributors */
	gchar *contributer = atom10_parse_person_construct(cur);
	fp->metadata = metadata_list_append(fp->metadata, "contributor", contributer);
	g_free(contributer);
}

static void atom10_parse_feed_generator(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *ret, *version, *tmp = "", *uri;
	ret = unhtmlize(utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1)));
	if (ret != NULL && ret[0] != '\0') {
		version = utf8_fix(xmlGetNsProp(cur, BAD_CAST"version", NULL));
		if (version != NULL) {
			tmp = g_strdup_printf("%s %s", ret, version);
			g_free(ret);
			g_free(version);
			ret = tmp;
		}
		uri = utf8_fix(xmlGetNsProp(cur, BAD_CAST"uri", NULL));
		if (uri != NULL) {
			tmp = g_strdup_printf("<a href=\"%s\">%s</a>", uri, ret);
			g_free(uri);
			g_free(ret);
			ret = tmp;
		}
		fp->metadata = metadata_list_append(fp->metadata, "feedgenerator", tmp);
	}
	g_free(ret);
}
static void atom10_parse_feed_icon(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	/* FIXME: Parse icon and use as a favicon? */
}

static void atom10_parse_feed_id(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	/* FIXME: Parse ID, but I'm not sure where Liferea would use it */
}

static void atom10_parse_feed_link(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *href;
	
	if(NULL != (href = utf8_fix(xmlGetNsProp(cur, BAD_CAST"href", NULL)))) {
		gchar *baseURL = (gchar*)xmlNodeGetBase(cur->doc, cur);
		gchar *url, *relation, *escTitle = NULL, *title;
		
		if (baseURL == NULL && fp->htmlUrl != NULL && fp->htmlUrl[0] != '|' &&
		    strstr(fp->htmlUrl, "://") != NULL)
			baseURL = g_strdup(fp->htmlUrl);
		url = common_build_url(href, baseURL);
		
		relation = utf8_fix(xmlGetNsProp(cur, BAD_CAST"rel", NULL));
		title = utf8_fix(xmlGetNsProp(cur, BAD_CAST"rel", NULL));
		if (title != NULL)
			escTitle = g_markup_escape_text(title, -1);
		/* FIXME: Display the human readable title from the property "title" */
		/* This current code was copied from the RSS parser.*/
		
		if(!xmlHasNsProp(cur, BAD_CAST"rel", NULL) || relation == NULL || g_str_equal(relation, BAD_CAST"alternate"))
			feed_set_html_url(fp, url);
		else if (g_str_equal(relation, "enclosure")) {
			ip->metadata = metadata_list_append(ip->metadata, "enclosure", url);
		} else
			/* FIXME: Maybe do something with other links such as "related" and add metadata for "via"? */;
		xmlFree(title);
		xmlFree(escTitle);
		xmlFree(baseURL);
		g_free(url);
		g_free(relation);
		g_free(href);
	} 
}

static void atom10_parse_feed_logo(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *logoUrl = atom10_parse_text_construct(cur, FALSE);
	/* FIXME: Verify URL is not evil... */
	feed_set_image_url(fp, logoUrl);
	g_free(logoUrl);
}

static void atom10_parse_feed_rights(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *rights = atom10_parse_text_construct(cur, FALSE);
	if(NULL != rights)
		fp->metadata = metadata_list_append(fp->metadata, "copyright", rights);
	g_free(rights);
}

static void atom10_parse_feed_subtitle(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *subtitle = convertToHTML(atom10_parse_text_construct(cur, TRUE));
	if (subtitle != NULL)
		feed_set_description(fp, subtitle);
	g_free(subtitle);				
}

static void atom10_parse_feed_title(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	gchar *title = atom10_parse_text_construct(cur, FALSE);
	if (title != NULL)
		feed_set_title(fp, title);
	g_free(title);
}

static void atom10_parse_feed_updated(xmlNodePtr cur, feedPtr fp, itemPtr ip, struct atom10ParserState state) {
	
	gchar *timestamp = utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	if(NULL != timestamp) {
		fp->metadata = metadata_list_append(fp->metadata, "contentUpdateDate", timestamp);
		feed_set_time(fp, parseISO8601Date(timestamp));
		g_free(timestamp);
	}
}

/* reads a Atom feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void atom10_parse_feed(feedPtr fp, itemSetPtr sp, xmlDocPtr doc, xmlNodePtr cur) {
	itemPtr 		ip;
	int 			error = 0;
	NsHandler		*nsh;
	parseChannelTagFunc	pf;
	atom10ElementParserFunc func;
	static GHashTable *feedElementHash = NULL;
	
	if (feedElementHash == NULL) {
		feedElementHash = g_hash_table_new(g_str_hash, g_str_equal);
		
		g_hash_table_insert(feedElementHash, "author", &atom10_parse_feed_author);
		g_hash_table_insert(feedElementHash, "category", &atom10_parse_feed_category);
		g_hash_table_insert(feedElementHash, "contributor", &atom10_parse_feed_contributor);
		g_hash_table_insert(feedElementHash, "generator", &atom10_parse_feed_generator);
		g_hash_table_insert(feedElementHash, "icon", &atom10_parse_feed_icon);
		g_hash_table_insert(feedElementHash, "id", &atom10_parse_feed_id);
		g_hash_table_insert(feedElementHash, "link", &atom10_parse_feed_link);
		g_hash_table_insert(feedElementHash, "logo", &atom10_parse_feed_logo);
		g_hash_table_insert(feedElementHash, "rights", &atom10_parse_feed_rights);
		g_hash_table_insert(feedElementHash, "subtitle", &atom10_parse_feed_subtitle);
		g_hash_table_insert(feedElementHash, "title", &atom10_parse_feed_title);
		g_hash_table_insert(feedElementHash, "updated", &atom10_parse_feed_updated);
	}	

	while(TRUE) {
		if(xmlStrcmp(cur->name, BAD_CAST"feed")) {
			addToHTMLBuffer(&(fp->parseErrors), _("<p>Could not find Atom 1.0 header!</p>"));
			error = 1;
			break;			
		}
		
		/* parse feed contents */
		cur = cur->xmlChildrenNode;
		while(cur != NULL) {
			if(NULL == cur->name || cur->type != XML_ELEMENT_NODE || cur->ns == NULL) {
				cur = cur->next;
				continue;
			}
			
			if(((cur->ns->href != NULL)   && (NULL != (nsh = (NsHandler *)g_hash_table_lookup(ns_atom10_ns_uri_table, (gpointer)cur->ns->href)))) ||
			   ((cur->ns->prefix != NULL) && (NULL != (nsh = (NsHandler *)g_hash_table_lookup(atom10_nstable, (gpointer)cur->ns->prefix))))) {
				pf = nsh->parseChannelTag;
				if(NULL != pf)
					(*pf)(fp, cur);
				cur = cur->next;
				continue;
			}
			
			/* check namespace of this tag */
			if(cur->ns->href == NULL) {
				/* This is an invalid feed... no idea what to do with the current element */
				printf("element with no namespace found!\n");
				cur = cur->next;
				continue;
			}

			if(xmlStrcmp(cur->ns->href, ATOM10_NS) != 0) {
				printf("unknown namespace found in feed\n");
				cur = cur->next;
				continue;
			}
			/* At this point, the namespace must be the Atom 1.0 namespace */
			
			func = g_hash_table_lookup(feedElementHash, cur->name);
			if(func != NULL) {
				(*func)(cur, fp, NULL, NULL);
			} else if((xmlStrEqual(cur->name, BAD_CAST"entry"))) {
				if(NULL != (ip = atom10_parse_entry(fp, cur))) {
					itemset_add_item(sp, ip);
				}
			}
			cur = cur->next;
		}
		
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
	return xmlStrEqual(cur->name, BAD_CAST"feed") && cur->ns != NULL && cur->ns->href != NULL &&  xmlStrEqual(cur->ns->href, ATOM10_NS);
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

