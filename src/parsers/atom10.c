/**
 * @file atom10.c Atom 1.0 Parser
 * 
 * Copyright (C) 2005-2006 Nathan Conrad <t98502@users.sourceforge.net>
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include "conf.h"
#include "common.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "ns_dc.h"
#include "ns_content.h"
#include "ns_slash.h"
#include "ns_syn.h"
#include "ns_admin.h"
#include "ns_ag.h"
#include "ns_blogChannel.h"
#include "ns_cC.h"
#include "ns_photo.h"
#include "ns_wfw.h"
#include "metadata.h"
#include "subscription.h"
#include "xml.h"
#include "atom10.h"

#define ATOM10_NS BAD_CAST"http://www.w3.org/2005/Atom"

/* to store the ATOMNsHandler structs for all supported RDF namespace handlers */
GHashTable	*atom10_nstable = NULL;
GHashTable	*ns_atom10_ns_uri_table = NULL;
struct atom10ParserState {
	gboolean errorDetected;
};
typedef void 	(*atom10ElementParserFunc)	(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state);

gchar* atom10_mark_up_text_content(gchar* content) {
	gchar **tokens;
	gchar **token;
	gchar *str, *old_str;
	
	if(!content) return NULL;
	if(!*content) return g_strdup(content);
	
	tokens = g_strsplit(content, "\n\n", 0);
	
	if (tokens[0] == NULL) { /* No tokens */
		str = g_strdup("");
	} else if (tokens[1] == NULL) { /* One token */
		str = g_markup_escape_text(tokens[0], -1);
	} else { /* Many tokens */
		token = tokens;
		while (*token != NULL) {
			old_str = *token;
			str = g_strchug(g_strchomp(*token)); /* WARNING: modifies the token string*/
			if(str[0] != '\0') {
				*token = g_markup_printf_escaped("<p>%s</p>", str);
				g_free(old_str);
			} else {
				**token = '\0'; /* Erase the particular token because it is blank */
			}
			token++;
		}
		str = g_strjoinv("\n", tokens);
	}
	g_strfreev(tokens);
	
	return str;
}


/**
 * This parses an Atom content construct.
 *
 * @param cur	the XML node to be parsed
 * @param ctxt 	a valid feed parser context
 * @returns g_strduped string which must be freed by the caller.
 */
static gchar* atom10_parse_content_construct(xmlNodePtr cur, feedParserCtxtPtr ctxt) {
	gchar *ret;

	g_assert(NULL != cur);
	ret = NULL;
	
	if(xmlHasNsProp(cur, BAD_CAST"src", NULL )) {
		xmlChar *src = xmlGetNsProp(cur, BAD_CAST"src", NULL);
		
		if(!src) {
			ret = g_strdup(_("Liferea is unable to display this item's content."));
		} else {
			gchar *baseURL = common_utf8_fix(xmlNodeGetBase(cur->doc, cur));
			gchar *url;
			
			url = common_build_url(src, baseURL);
			ret = g_strdup_printf(_("<p><a href=\"%s\">View this item's content.</a></p>"), url);
			
			g_free(url);
			xmlFree(baseURL);
			xmlFree(src);
		}
	} else {
		gchar *type;

		/* determine encoding mode */
		type = xmlGetNsProp(cur, BAD_CAST"type", NULL);
		
		/* This that need to be de-encoded and should not contain sub-tags.*/
		if(type && (g_str_equal(type,"html") || !g_strcasecmp(type, "text/html"))) {
			ret = common_utf8_fix(xhtml_extract (cur, 0, NULL));
		} else if(!type || !strcmp(type, "text") || !strncasecmp(type, "text/",5)) {
			gchar *tmp;
			/* Assume that "text/ *" files can be directly displayed.. kinda stated in the RFC */
			ret = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			
			g_strchug(g_strchomp(ret));
			
			if(!type || !strcasecmp(type, "text"))
				tmp = atom10_mark_up_text_content(ret);
			else
				tmp = g_markup_printf_escaped("<pre>%s</pre>", ret);
			g_free(ret);
			ret = tmp;
		} else if(!strcmp(type,"xhtml") || !g_strcasecmp(type, "application/xhtml+xml")) {
			/* The spec says to only show the contents of the div tag that MUST be present */
			ret = common_utf8_fix(xhtml_extract (cur, 2, NULL));
		} else {
			/* Do nothing on unsupported content types. This allows summaries to be used. */
			ret = NULL;
		}
		
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
	type = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"type", NULL));
	
	/* not sure what MIME types are necessary... */
	
	/* This that need to be de-encoded and should not contain sub-tags.*/
	if (NULL == type || !strcmp(type, "text")) {
		ret = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
		if(ret) {
			g_strchug(g_strchomp(ret));

			if (htmlified) {
				tmp = atom10_mark_up_text_content(ret);
				g_free(ret);
				ret = tmp;
			}
		}
	} else if (!strcmp(type, "html")) {
		ret = common_utf8_fix(xhtml_extract (cur, 0, NULL));
		if (!htmlified)
			ret = unhtmlize(unxmlize(ret));
	} else if(!strcmp(type, "xhtml")) {
		/* The spec says to show the contents of the div tag that MUST be present */
		ret = common_utf8_fix(xhtml_extract (cur, 2, NULL));
		
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
				name = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
			}
			
			if (xmlStrEqual(cur->name, BAD_CAST"email")) {
				if (email != NULL)
					invalid = TRUE;
				g_free(email);
				tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
				email = g_strdup_printf(" - <a href=\"mailto:%s\">%s</a>", tmp, tmp);
				g_free(tmp);
			}
			
			if (xmlStrEqual(cur->name, BAD_CAST"uri")) {
				if (uri == NULL)
					invalid = TRUE;
				g_free(uri);
				tmp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
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

/* Note: this function is called for both item and feed context */
static gchar* atom10_parse_link(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *href, *alternate = NULL;
	
	href = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"href", NULL));
	if(href) {
		xmlChar *baseURL = xmlNodeGetBase(cur->doc, cur);
		gchar *url, *relation, *type, *escTitle = NULL, *title;
		
		if(!baseURL && ctxt->feed->htmlUrl && ctxt->feed->htmlUrl[0] != '|' &&
		   strstr(ctxt->feed->htmlUrl, "://"))
			baseURL = xmlStrdup(BAD_CAST(ctxt->feed->htmlUrl));
		url = common_build_url(href, baseURL);

		type = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"type", NULL));		
		relation = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"rel", NULL));
		title = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"title", NULL));
		if(title)
			escTitle = g_markup_escape_text(title, -1);
		
		if(!xmlHasNsProp(cur, BAD_CAST"rel", NULL) || !relation || g_str_equal(relation, BAD_CAST"alternate"))
			alternate = g_strdup(url);
		else if(g_str_equal(relation, "replies")) {
			if(!type || g_str_equal(type, BAD_CAST"application/atom+xml")) {
				gchar *commentUri = common_build_url(url, feed_get_html_url(ctxt->feed));
				if (ctxt->item)
					metadata_list_set(&ctxt->item->metadata, "commentFeedUri", commentUri);
			}
		} else if(g_str_equal(relation, "enclosure")) {
			if (ctxt->item) {
				ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "enclosure", url);
				ctxt->item->hasEnclosure = TRUE;
			}
		} else if(g_str_equal(relation, "related") || g_str_equal(relation, "via")) {	
			if (ctxt->item)
				ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, relation, url);
		} else {
			/* g_warning("Unhandled Atom link with unexpected relation \"%s\"\n", relation); */
		}
		xmlFree(title);
		xmlFree(baseURL);
		g_free(escTitle);
		g_free(url);
		g_free(relation);
		g_free(type);
		g_free(href);
	} else {
		/* FIXME: @href is required, this document is not valid Atom */;
	}
	
	return alternate;
}

static void atom10_parse_entry_author(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *author;
	
	author = atom10_parse_person_construct(cur);
	if(author) {
		ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "author", author);
		g_free(author);
	}
}

static void atom10_parse_entry_category(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *category = NULL, *categoryEsc;

	if(xmlHasNsProp(cur, BAD_CAST"label", NULL)) {
		category = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"label", NULL));
		
	} else if(xmlHasNsProp(cur, BAD_CAST"term", NULL)) {
		category = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"term", NULL));
	}
	if(category) {
		categoryEsc = g_markup_escape_text(category, -1);
		ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "category", categoryEsc);
		g_free(categoryEsc);
		xmlFree(category);
	}
}

static void atom10_parse_entry_content(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *content;
	
	content = atom10_parse_content_construct(cur, ctxt);
	if(content) {
		item_set_description(ctxt->item, content);
		g_free(content);
	}
}

static void atom10_parse_entry_contributor(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *contributor;
	
	contributor = atom10_parse_person_construct(cur);
	if(contributor) {
		ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "contributor", contributor);
		g_free(contributor);
	}
}

static void atom10_parse_entry_id(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *id;
	
	id = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	if(id) {
		item_set_id(ctxt->item, id);
		ctxt->item->validGuid = TRUE;
		g_free(id);
	}
}

static void atom10_parse_entry_link(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *href;
	
	href = atom10_parse_link(cur, ctxt, state);
	if(href) {
		item_set_source(ctxt->item, href);
		g_free(href);
	}
}

static void atom10_parse_entry_published(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *datestr;
	
	datestr = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	if(datestr) {
		ctxt->item->time = parseISO8601Date(datestr);
		ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "pubDate", datestr);
		g_free(datestr);
	}
}

static void atom10_parse_entry_rights(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *rights;

	rights = atom10_parse_text_construct(cur, FALSE);
	if(rights) {
		ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "copyright", rights);
		g_free(rights);
	}
}

/* <summary> can be used for short text descriptions, if there is no
   <content> description we show the <summary> content */
static void atom10_parse_entry_summary(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
		
	if(!item_get_description(ctxt->item)) {
		gchar *summary = atom10_parse_text_construct(cur, TRUE);
		if(summary) {
			item_set_description(ctxt->item, summary);
			g_free(summary);
		}
		/* FIXME: set a flag to show a "Read more" link to the user; but where? */
	}
}

static void atom10_parse_entry_title(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *title;
	
	title = common_utf8_fix(atom10_parse_text_construct(cur, FALSE));
	if(title) {
		item_set_title(ctxt->item, title);
		g_free(title);
	}
}

static void atom10_parse_entry_updated(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *datestr;
	
	datestr = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	if(datestr) {
		ctxt->item->time = parseISO8601Date(datestr);
		ctxt->item->metadata = metadata_list_append(ctxt->item->metadata, "contentUpdateDate", datestr);
		g_free(datestr);
	}
}

/* <content> tag support, FIXME: base64 not supported */
/* method to parse standard tags for each item element */
static itemPtr atom10_parse_entry(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	NsHandler		*nsh;
	parseItemTagFunc	pf;
	atom10ElementParserFunc func;
	static GHashTable *entryElementHash = NULL;
	
	g_assert(NULL != cur);
	
	if(entryElementHash == NULL) {
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

	ctxt->item = item_new();
	
	cur = cur->xmlChildrenNode;
	while(cur) {
		
		if(cur->type != XML_ELEMENT_NODE || cur->name == NULL || cur->ns == NULL) {
			cur = cur->next;
			continue;
		}
		
		if((cur->ns->href   && (nsh = (NsHandler *)g_hash_table_lookup(ns_atom10_ns_uri_table, (gpointer)cur->ns->href))) ||
		   (cur->ns->prefix && (nsh = (NsHandler *)g_hash_table_lookup(atom10_nstable, (gpointer)cur->ns->prefix)))) {
			
			pf = nsh->parseItemTag;
			if(pf)
				(*pf)(ctxt, cur);
			cur = cur->next;
			continue;
		}
		
		/* check namespace of this tag */
		if(!cur->ns->href) {
			/* This is an invalid feed... no idea what to do with the current element */
			debug1(DEBUG_PARSING, "element with no namespace found in atom feed (%s)!", cur->name);
			cur = cur->next;
			continue;
		}
		
		
		if(0 != xmlStrcmp(cur->ns->href, ATOM10_NS)) {
			debug1(DEBUG_PARSING, "unknown namespace %s found!", cur->ns->href);
			cur = cur->next;
			continue;
		}
		/* At this point, the namespace must be the Atom 1.0 namespace */
		func = g_hash_table_lookup(entryElementHash, cur->name);
		if(func != NULL) {
			(*func)(cur, ctxt, NULL);
		} else {
			debug1(DEBUG_PARSING, "unknown entry element \"%s\" found", cur->name);
		}
		
		cur = cur->next;
	}
	
	/* after parsing we fill the infos into the itemPtr structure */
	ctxt->item->readStatus = FALSE;
	
	if(0 == ctxt->item->time)
		ctxt->item->time = ctxt->feed->time;
	
	return ctxt->item;
}

static void atom10_parse_feed_author(xmlNodePtr cur, feedParserCtxtPtr ctxt, itemPtr ip, struct atom10ParserState *state) {
	/* parse feed author */
	gchar *author = atom10_parse_person_construct(cur);
	ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "author", author);
	g_free(author);
	/* FIXME: make item parsing use this author if not specified elsewhere */
}

static void atom10_parse_feed_category(xmlNodePtr cur, feedParserCtxtPtr ctxt, itemPtr ip, struct atom10ParserState *state) {
	gchar *label = NULL, *escaped;

	if(xmlHasNsProp(cur, BAD_CAST"label", NULL)) {
		label = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"label", NULL));
	} else if(xmlHasNsProp(cur, BAD_CAST"term", NULL)) {
		label = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"term", NULL));
	}
	if(label != NULL) {
		escaped = g_markup_escape_text(label, -1);
		ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "category", escaped);
		g_free(escaped);
		xmlFree(label);
	}
}

static void atom10_parse_feed_contributor(xmlNodePtr cur, feedParserCtxtPtr ctxt, itemPtr ip, struct atom10ParserState *state) {
	/* parse feed contributors */
	gchar *contributer = atom10_parse_person_construct(cur);
	ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "contributor", contributer);
	g_free(contributer);
}

static void atom10_parse_feed_generator(xmlNodePtr cur, feedParserCtxtPtr ctxt, itemPtr ip, struct atom10ParserState *state) {
	gchar *ret, *version, *tmp = "", *uri;

	ret = unhtmlize(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	if(ret != NULL && ret[0] != '\0') {
		version = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"version", NULL));
		if(version != NULL) {
			tmp = g_strdup_printf("%s %s", ret, version);
			g_free(ret);
			g_free(version);
			ret = tmp;
		}
		uri = common_utf8_fix(xmlGetNsProp(cur, BAD_CAST"uri", NULL));
		if(uri != NULL) {
			tmp = g_strdup_printf("<a href=\"%s\">%s</a>", uri, ret);
			g_free(uri);
			g_free(ret);
			ret = tmp;
		}
		ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "feedgenerator", tmp);
	}
	g_free(ret);
}
static void atom10_parse_feed_icon(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	/* FIXME: Parse icon and use as a favicon? */
}

static void atom10_parse_feed_id(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	/* FIXME: Parse ID, but I'm not sure where Liferea would use it */
}


static void atom10_parse_feed_link(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *href;
	
	href = atom10_parse_link(cur, ctxt, state);
	if(href) {
		feed_set_html_url(ctxt->feed, subscription_get_source(ctxt->subscription), href);
		/* Set the default base to the feed's HTML URL if not set yet */
		if (xmlNodeGetBase(cur->doc, xmlDocGetRootElement(cur->doc)) == NULL)
			xmlNodeSetBase(xmlDocGetRootElement(cur->doc), href);
		g_free(href);
	}
}

static void atom10_parse_feed_logo(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *logoUrl;
	
	logoUrl = atom10_parse_text_construct(cur, FALSE);
	if(logoUrl) {
		/* FIXME: Verify URL is not evil... */
		feed_set_image_url(ctxt->feed, logoUrl);
		g_free(logoUrl);
	}
}

static void atom10_parse_feed_rights(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *rights;
	
	rights = atom10_parse_text_construct(cur, FALSE);
	if(rights) {
		ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "copyright", rights);
		g_free(rights);
	}
}

static void
atom10_parse_feed_subtitle (xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state)
{
	gchar *subtitle;
	
	subtitle = atom10_parse_text_construct (cur, TRUE);
	if (subtitle) {
 		metadata_list_set (&ctxt->subscription->metadata, "description", subtitle);
		g_free (subtitle);
	}
}

static void atom10_parse_feed_title(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *title;
	
	title = atom10_parse_text_construct(cur, FALSE);
	if(title) {
		if(ctxt->title)
			g_free(ctxt->title);
		ctxt->title = title;
	}
}

static void atom10_parse_feed_updated(xmlNodePtr cur, feedParserCtxtPtr ctxt, struct atom10ParserState *state) {
	gchar *timestamp;
	
	timestamp = common_utf8_fix(xmlNodeListGetString(cur->doc, cur->xmlChildrenNode, 1));
	if(timestamp) {
		ctxt->subscription->metadata = metadata_list_append(ctxt->subscription->metadata, "contentUpdateDate", timestamp);
		ctxt->feed->time = parseISO8601Date(timestamp);
		g_free(timestamp);
	}
}

/* reads a Atom feed URL and returns a new channel structure (even if
   the feed could not be read) */
static void atom10_parse_feed(feedParserCtxtPtr ctxt, xmlNodePtr cur) {
	NsHandler		*nsh;
	parseChannelTagFunc	pf;
	atom10ElementParserFunc func;
	static GHashTable *feedElementHash = NULL;
	
	if(feedElementHash == NULL) {
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
			g_string_append(ctxt->feed->parseErrors, "<p>Could not find Atom 1.0 header!</p>");
			break;			
		}
		
		/* parse feed contents */
		cur = cur->xmlChildrenNode;
		while(cur) {
			if(!cur->name || cur->type != XML_ELEMENT_NODE || !cur->ns) {
				cur = cur->next;
				continue;
			}
			
			/* check if supported namespace should handle the current tag 
			   by trying to determine a namespace handler */
			   
			nsh = NULL;
			
			if(cur->ns->href)
				nsh = (NsHandler *)g_hash_table_lookup(ns_atom10_ns_uri_table, (gpointer)cur->ns->href);
			
			if(cur->ns->prefix && !nsh)
				nsh = (NsHandler *)g_hash_table_lookup(atom10_nstable, (gpointer)cur->ns->prefix);
				
			if(nsh) {
				pf = nsh->parseChannelTag;
				if(pf)
					(*pf)(ctxt, cur);
				cur = cur->next;
				continue;
			}
			
			/* check namespace of this tag */
			if(!cur->ns->href) {
				/* This is an invalid feed... no idea what to do with the current element */
				debug1(DEBUG_PARSING, "element with no namespace found in atom feed (%s)!", cur->name);
				cur = cur->next;
				continue;
			}

			if(xmlStrcmp(cur->ns->href, ATOM10_NS)) {
				debug1(DEBUG_PARSING, "unknown namespace %s found in atom feed!", cur->ns->href);
				cur = cur->next;
				continue;
			}
			/* At this point, the namespace must be the Atom 1.0 namespace */
			
			func = g_hash_table_lookup(feedElementHash, cur->name);
			if(func) {
				(*func)(cur, ctxt, NULL);
			} else if((xmlStrEqual(cur->name, BAD_CAST"entry"))) {
				ctxt->item = atom10_parse_entry(ctxt, cur);
				if(ctxt->item)
					ctxt->items = g_list_append(ctxt->items, ctxt->item);
			}
			cur = cur->next;
		}
		
		/* FIXME: Maybe check to see that the required information was actually provided (persuant to the RFC). */
		/* after parsing we fill in the infos into the feedPtr structure */		
		
		break;
	}
}

static gboolean atom10_format_check(xmlDocPtr doc, xmlNodePtr cur) {
	if (cur->name == NULL || cur->ns == NULL || cur->ns->href == NULL)
		return FALSE;
	return xmlStrEqual(cur->name, BAD_CAST"feed") && xmlStrEqual(cur->ns->href, ATOM10_NS);
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
		atom10_add_ns_handler(ns_bC_getRSSNsHandler());
		atom10_add_ns_handler(ns_dc_getRSSNsHandler());
  		atom10_add_ns_handler(ns_slash_getRSSNsHandler());
		atom10_add_ns_handler(ns_content_getRSSNsHandler());
		atom10_add_ns_handler(ns_syn_getRSSNsHandler());
		atom10_add_ns_handler(ns_admin_getRSSNsHandler());
		atom10_add_ns_handler(ns_ag_getRSSNsHandler());
		atom10_add_ns_handler(ns_cC_getRSSNsHandler());
		atom10_add_ns_handler(ns_photo_getRSSNsHandler());
		atom10_add_ns_handler(ns_pb_getRSSNsHandler());
		atom10_add_ns_handler(ns_wfw_getRSSNsHandler());

	}	
	/* prepare feed handler structure */
	fhp->typeStr = "pie";
	fhp->icon = ICON_AVAILABLE;
	fhp->feedParser	= atom10_parse_feed;
	fhp->checkFormat = atom10_format_check;
	fhp->merge = TRUE;

	return fhp;
}

