/**
 * @file html.c  HTML parsing
 *
 * Copyright (C) 2004 ahmed el-helw <ahmedre@cc.gatech.edu>
 * Copyright (C) 2004-2020 Lars Windolf <lars.windolf@gmx.de>
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "html.h"
#include "xml.h"

enum {
	LINK_FAVICON,
	LINK_RSS_ALTERNATE,
	LINK_AMPHTML
};

#define XPATH_LINK_RSS_ALTERNATE "/html/head/link[@rel='alternate'][@type='application/atom+xml' or @type='application/rss+xml' or @type='application/rdf+xml' or @type='text/xml']"

/**
 * Fetch attribute of a html tag string
 */
static gchar *
html_get_attrib (const gchar* str, gchar *attrib_name) {
	gchar		*res;
	const gchar	*tmp, *tmp2;
	size_t		len = 0;
	gchar		quote;

	/*debug1(DEBUG_PARSING, "fetching href %s", str); */
	tmp = common_strcasestr (str, attrib_name);
	if (!tmp)
		return NULL;
	tmp += 5;

	/* skip spaces up to the first quote. This is really slightly
	 wrong.  SGML allows unquoted atributes, but not if they contain
	 slashes, so 99% of all URIs will require quotes. */
	while (*tmp != '\"' && *tmp != '\'') {
		if (*tmp == '>' || *tmp == '\0' || !isspace(*tmp))
			return NULL;
		tmp++;
	}
	quote = *tmp; /* The type of quote mark used to delimit the arg */
	tmp++;
	tmp2 = tmp;
	while ((*tmp2 != quote && *(tmp2-1) != '\\') && /* Escaped quote*/
		  *tmp2 != '\0')
		tmp2++, len++;

	res = g_strndup (tmp, len);
	return res;
}

static gchar *
html_check_link_ref (const gchar* str, gint linkType)
{
	gchar		*res;

	res = html_get_attrib (str, "href");

	if (linkType == LINK_FAVICON) {
		/* The type attribute is optional, so don't check for it,
		 * as according to the W3C, it must be must be png, gif or
		 * ico anyway: http://www.w3.org/2005/10/howto-favicon
		 * Instead, do stronger checks for the rel attribute. */
		if (((NULL != common_strcasestr (str, "rel=\"shortcut icon\"")) ||
		     (NULL != common_strcasestr (str, "rel=\"icon\"")) ||
		     (NULL != common_strcasestr (str, "rel=\'shortcut icon\'")) ||
		     (NULL != common_strcasestr (str, "rel=\'icon\'"))) /*&&
		    ((NULL != common_strcasestr (str, "image/x-icon")) ||
		     (NULL != common_strcasestr (str, "image/png")) ||
		     (NULL != common_strcasestr (str, "image/gif")))*/)
			return res;

		/* Also support high res (~128px) device icons */
		if((NULL != common_strcasestr (str, "rel=\"apple-touch-icon\"")) &&
		   ((NULL != common_strcasestr (str, "sizes=\"152x152\"")) ||
		    (NULL != common_strcasestr (str, "sizes=\"144x144\"")) ||
		    (NULL != common_strcasestr (str, "sizes=\"120x120\""))))
			return res;
	} else if (linkType == LINK_RSS_ALTERNATE) {
		if ((common_strcasestr (str, "alternate") != NULL) &&
		    ((common_strcasestr (str, "text/xml") != NULL) ||
		     (common_strcasestr (str, "rss+xml") != NULL) ||
		     (common_strcasestr (str, "rdf+xml") != NULL) ||
		     (common_strcasestr (str, "atom+xml") != NULL)))
			return res;
	} else if (linkType == LINK_AMPHTML) {
		if (common_strcasestr (str, "amphtml") != NULL)
			return res;
	}
	g_free (res);
	return NULL;
}

/**
 * Search tag in a html content, return link of the tag pointed by href
 */
static gchar *
search_tag_link_dirty (const gchar* data, const gchar *tagName, gchar** tagEnd)
{
	gchar	*ptr;
	const gchar	*tmp = data;
	gchar	*result = NULL;
	gchar	*tstr;
	gchar	*endptr;
	gchar   *tagname_start;

	if (tagEnd)
		*tagEnd = NULL;

	tagname_start = g_strconcat("<", tagName, NULL);
	ptr = common_strcasestr (tmp, tagname_start);
	g_free(tagname_start);

	if (!ptr)
		return NULL;

	endptr = strchr (ptr, '>');
	if (!endptr)
		return NULL;
	*endptr = '\0';
	tstr = g_strdup (ptr);
	*endptr = '>';
	result = html_get_attrib (tstr, "href");
	g_free (tstr);

	if (tagEnd) {
		endptr++;
		*tagEnd = endptr;
	}

	if (result) {
		/* URIs can contain escaped things....
		 * All ampersands must be escaped, for example */
		result = unhtmlize (result);
	}
	return result;
}

// FIXME: implement multiple links
static gchar *
search_links_dirty (const gchar* data, gint linkType)
{
	gchar	*ptr;
	const gchar	*tmp = data;
	gchar	*result = NULL;
	gchar	*res;
	gchar	*tstr;
	gchar	*endptr;

	while (1) {
		ptr = common_strcasestr (tmp, "<link");
		if (!ptr)
			return NULL;

		endptr = strchr (ptr, '>');
		if (!endptr)
			return NULL;
		*endptr = '\0';
		tstr = g_strdup (ptr);
		*endptr = '>';
		res = html_check_link_ref (tstr, linkType);
		g_free (tstr);
		if (res) {
			result = res;
			break;
/*		deactivated as long as we support only subscribing
		to the first found link (BTW this code crashes on
		sites like Groklaw!)

			gchar* t;
			if(result == NULL)
				result = res;
			else {
				t = g_strdup_printf("%s\n%s", result, res);
				g_free(res);
				g_free(result);
				result = t;
			}*/
		}
		tmp = endptr;
	}

	result = unhtmlize (result); /* URIs can contain escaped things.... All ampersands must be escaped, for example */
	return result;
}

static void
html_auto_discover_collect_links (xmlNodePtr match, gpointer user_data)
{
	GSList **links = (GSList **)user_data;
	gchar *link = xml_get_attribute (match, "href");
	if(link)
		*links = g_slist_append (*links, link);
}

GSList *
html_auto_discover_feed (const gchar* data, const gchar *defaultBaseUri)
{
	GSList		*iter, *links = NULL;
	gchar		*baseUri = NULL;
	xmlDocPtr	doc;
	xmlNodePtr	node, root;

	// If possible we want to use XML instead of tag soup
	doc = xhtml_parse ((gchar *)data, (size_t)strlen(data));
	if (!doc)
		return NULL;

	root = xmlDocGetRootElement (doc);

	// Base URL resolving
	node = xpath_find (root, "/html/head/base");
	if (node)
		baseUri = xml_get_attribute (node, "href");
	if (!baseUri)
		baseUri = g_strdup (search_tag_link_dirty (data, "base", NULL));
	if (!baseUri)
		baseUri = g_strdup (defaultBaseUri);

	debug0 (DEBUG_UPDATE, "searching through link tags");
	xpath_foreach_match (root, XPATH_LINK_RSS_ALTERNATE, html_auto_discover_collect_links, (gpointer)&links);
	if (!links) {
		gchar *tmp = search_links_dirty (data, LINK_RSS_ALTERNATE);
		if (tmp)
			links = g_slist_append (links, tmp);
	}

	/* Turn relative URIs into absolute URIs */
	iter = links;
	while (iter) {
		gchar *tmp = iter->data;
		iter->data = common_build_url (tmp, baseUri);
		g_free (tmp);
		debug1 (DEBUG_UPDATE, "search result: %s", (gchar *)iter->data);
		iter = g_slist_next (iter);
	}

	g_free (baseUri);

	return links;
}

gchar *
html_discover_favicon (const gchar * data, const gchar * baseUri)
{
	gchar			*res, *tmp;

	debug0 (DEBUG_UPDATE, "searching through link tags");
	res = search_links_dirty (data, LINK_FAVICON);
	debug1 (DEBUG_UPDATE, "search result: %s", res? res : "none found");

	// FIXME: take multiple links from search_links() and rank them by sizes
	// and return an ordered list

	if (res) {
		/* turn relative URIs into absolute URIs */
		tmp = res;
		res = common_build_url (res, baseUri);
		g_free (tmp);
	}

	return res;
}

gchar *
html_get_article (const gchar *data, const gchar *baseUri) {
	xmlDocPtr	doc;
	xmlNodePtr	node, root;
	gchar		*result = NULL;

	doc = xhtml_parse ((gchar *)data, (size_t)strlen(data));
	if (!doc) {
		debug1 (DEBUG_PARSING, "XHTML parsing error during HTML5 fetch of '%s'\n", baseUri);
		return NULL;
	}

	root = xmlDocGetRootElement (doc);
	if (root) {
		// Find HTML5 <article>, we only expect a single article...
		node = xpath_find (root, "//article");

		// Fallback to microformat <div property='articleBody'>
		if (!node)
			node = xpath_find (root, "//div[@property='articleBody']");

		// Fallback to <div id='content'> which is a quite common
		if (!node)
			node = xpath_find (root, "//div[@id='content']");

		if (node) {
			// No HTML stripping here, as we rely on Readability.js
			result = xhtml_extract (node, 1, baseUri);
		} else {
			debug1 (DEBUG_PARSING, "No article found during HTML5 parsing of '%s'\n", baseUri);
		}

		xmlFreeDoc (doc);
	}

	return result;
}

gchar *
html_get_amp_url (const gchar *data) {
	return search_links_dirty (data, LINK_AMPHTML);
}
