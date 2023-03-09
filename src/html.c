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
#include "render.h"
#include "xml.h"

enum {
	LINK_RSS_ALTERNATE,
	LINK_AMPHTML
};

#define XPATH_LINK_RSS_ALTERNATE    "/html/head/link[@rel='alternate'][@type='application/atom+xml' or @type='application/rss+xml' or @type='application/rdf+xml' or @type='text/xml']"

#define XPATH_LINK_MS_TILE_IMAGE                "/html/head/meta[@name='msapplication-TileImage']"

#define XPATH_LINK_SAFARI_MASK_ICON             "/html/head/link[@rel='mask-icon']"

#define XPATH_LINK_LARGE_ICON                   "/html/head/link[@rel='icon' or @rel='shortcut icon'][@sizes='192x192' or @sizes='144x144' or @sizes='128x128']"
#define XPATH_LINK_LARGE_ICON_OTHER_SIZES       "/html/head/link[@rel='icon' or @rel='shortcut icon'][@sizes]"
#define XPATH_LINK_FAVICON                      "/html/head/link[@rel='icon' or @rel='shortcut icon' or @rel='SHORTCUT ICON'][not(@sizes)]"

#define XPATH_LINK_APPLE_TOUCH_ICON             "/html/head/link[@rel='apple-touch-icon' or @rel='apple-touch-icon-precomposed'][@sizes='180x180' or @sizes='152x152' or @sizes='144x144' or @sizes='120x120']"
#define XPATH_LINK_APPLE_TOUCH_ICON_NO_SIZE     "/html/head/link[@rel='apple-touch-icon' or @rel='apple-touch-icon-precomposed'][not(@sizes)]"
#define XPATH_LINK_APPLE_TOUCH_ICON_OTHER_SIZES "/html/head/link[@rel='apple-touch-icon' or @rel='apple-touch-icon-precomposed'][@sizes]"


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

	if (linkType == LINK_RSS_ALTERNATE) {
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
	if (link)
		*links = g_slist_append (*links, link);
}

static void
html_auto_discover_collect_meta (xmlNodePtr match, gpointer user_data)
{
	GSList **values = (GSList **)user_data;
	gchar *value = xml_get_attribute (match, "content");
	if (value)
		*values = g_slist_append (*values, value);
}

GSList *
html_auto_discover_feed (const gchar* data, const gchar *defaultBaseUri)
{
	GSList		*iter, *links = NULL, *valid_links = NULL;
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
		gchar *tmp = (gchar *)common_build_url (iter->data, baseUri);

		/* We expect only relative URIs starting with '/' or absolute URIs starting with 'http://' or 'https://' */
		if ('h' == tmp[0] || '/' == tmp[0]) {
			debug1 (DEBUG_UPDATE, "search result: %s", (gchar *)iter->data);
			valid_links = g_slist_append (valid_links, tmp);
		} else {
			debug1 (DEBUG_UPDATE, "html_auto_discover_feed: discarding invalid URL %s", tmp ? tmp : "NULL");
			g_free (tmp);
		}

		iter = g_slist_next (iter);
	}
	g_slist_free_full (links, g_free);

	g_free (baseUri);
	xmlFreeDoc (doc);

	return valid_links;
}

GSList *
html_discover_favicon (const gchar * data, const gchar * defaultBaseUri)
{
	xmlDocPtr	doc;
	xmlNodePtr	node, root;
	GSList		*results = NULL, *iter;
	gchar		*baseUri = NULL;

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

	/* First try icons with guaranteed sizes */
	xpath_foreach_match (root, XPATH_LINK_LARGE_ICON,       html_auto_discover_collect_links, (gpointer)&results);
	xpath_foreach_match (root, XPATH_LINK_APPLE_TOUCH_ICON, html_auto_discover_collect_links, (gpointer)&results);
	xpath_foreach_match (root, XPATH_LINK_MS_TILE_IMAGE,    html_auto_discover_collect_meta,  (gpointer)&results);
	xpath_foreach_match (root, XPATH_LINK_SAFARI_MASK_ICON, html_auto_discover_collect_links, (gpointer)&results);

	/* Next try probably larger ones */
	xpath_foreach_match (root, XPATH_LINK_APPLE_TOUCH_ICON_NO_SIZE,     html_auto_discover_collect_links, (gpointer)&results); /* no size with Apple touch usually means 180x180px */
	xpath_foreach_match (root, XPATH_LINK_LARGE_ICON_OTHER_SIZES,       html_auto_discover_collect_links, (gpointer)&results); /* usually 96x96px and below */
	xpath_foreach_match (root, XPATH_LINK_APPLE_TOUCH_ICON_OTHER_SIZES, html_auto_discover_collect_links, (gpointer)&results); /* usually 96x96px and below */

	/* Finally try to small favicon */
	xpath_foreach_match (root, XPATH_LINK_FAVICON,          html_auto_discover_collect_links, (gpointer)&results);	/* has to be last! */

	/* Turn relative URIs into absolute URIs */
	iter = results;
	while (iter) {
		gchar *tmp = iter->data;
		iter->data = common_build_url (tmp, baseUri);
		g_free (tmp);
		debug1 (DEBUG_UPDATE, "search result: %s", (gchar *)iter->data);
		iter = g_slist_next (iter);
	}
	g_free (baseUri);
	xmlFreeDoc (doc);

	return results;
}

gchar *
html_get_article (const gchar *data, const gchar *baseUri) {
	xmlDocPtr	doc;
	xmlNodePtr	root;
	gchar		*result = NULL;

	doc = xhtml_parse ((gchar *)data, (size_t)strlen (data));
	if (!doc) {
		debug1 (DEBUG_PARSING, "XHTML parsing error on '%s'\n", baseUri);
		return NULL;
	}

	root = xmlDocGetRootElement (doc);
	if (root) {
		xmlDocPtr article = xhtml_extract_doc (root, 1, baseUri);
		if (article) {
			/* For debug output
			xmlSaveCtxt *s;
			s = xmlSaveToFd(0, NULL, 0);
			xmlSaveDoc(s, article);
			xmlSaveClose(s);
			*/

			result = render_xml (article, "html5-extract", NULL);
			xmlFreeDoc (article);
		}
		xmlFreeDoc (doc);
	}

	return result;
}

gchar *
html_get_body (const gchar *data, const gchar *baseUri) {
	xmlDocPtr	doc;
	xmlNodePtr	root;
	gchar		*result = NULL;

	doc = xhtml_parse ((gchar *)data, (size_t)strlen (data));
	if (!doc) {
		debug1 (DEBUG_PARSING, "XHTML parsing error on '%s'\n", baseUri);
		return NULL;
	}

	root = xmlDocGetRootElement (doc);
	if (root) {
		xmlDocPtr body = xhtml_extract_doc (root, 1, baseUri);
		if (body) {
			result = render_xml (body, "html-extract", NULL);
			xmlFreeDoc (body);
		}
		xmlFreeDoc (doc);
	}
	return result;
}

gchar *
html_get_amp_url (const gchar *data) {
	return search_links_dirty (data, LINK_AMPHTML);
}
