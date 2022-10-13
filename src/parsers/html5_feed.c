/**
 * @file html5_feed.c  Parsing semantic annotated HTML5 webpages like feeds
 *
 * Copyright (C) 2020-2022 Lars Windolf <lars.windolf@gmx.de>
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
#include "html5_feed.h"

#include "common.h"
#include "date.h"
#include "feed_parser.h"
#include "metadata.h"
#include "xml.h"

static void
html5_feed_parse_article_title (xmlNodePtr cur, feedParserCtxtPtr ctxt)
{
	gchar *tmp = unxmlize (xhtml_extract (cur, 2, NULL));
	if (tmp) {
		g_strchug (g_strchomp (tmp));
		item_set_title (ctxt->item, tmp);
		g_free(tmp);
	}
}

static void
html5_feed_parse_article (xmlNodePtr itemNode, gpointer userdata)
{
	feedParserCtxtPtr	ctxt = (feedParserCtxtPtr)userdata;
	xmlNodePtr			cur;
	gchar				*tmp;

	ctxt->item = item_new ();

	// Get article time
	if ((cur = xpath_find (itemNode, ".//time/@datetime"))) {
		tmp = xhtml_extract (cur, 0, NULL);
		if (tmp) {
			item_set_time (ctxt->item, date_parse_RFC822 (tmp));
			g_free(tmp);
		}
	}

	// or default to current feed timestamp
	if (0 == ctxt->item->time)
		ctxt->item->time = ctxt->feed->time;

	// get link
	if ((cur = xpath_find (itemNode, ".//a/@href"))) {
		tmp = (gchar *)xmlNodeListGetString (cur->doc, cur->xmlChildrenNode, 1);
		if (tmp) {
			xmlChar *link = common_build_url (tmp, ctxt->subscription->source);
			item_set_source (ctxt->item, (gchar *)link);
			xmlFree (link);

			// we use the link as id, as on websites link point to unique
			// content in 99% of the cases
			ctxt->item->sourceId = tmp;
		}
	}

	// extract title from most relevant header tag
	if ((cur = xpath_find (itemNode, ".//h1")))
		html5_feed_parse_article_title (cur, ctxt);
	else if ((cur = xpath_find (itemNode, ".//h2")))
		html5_feed_parse_article_title (cur, ctxt);
	else if ((cur = xpath_find (itemNode, ".//h3")))
		html5_feed_parse_article_title (cur, ctxt);
	else if ((cur = xpath_find (itemNode, ".//h4")))
		html5_feed_parse_article_title (cur, ctxt);
	else if ((cur = xpath_find (itemNode, ".//h5")))
		html5_feed_parse_article_title (cur, ctxt);

	// Extract the actual article
	tmp = xhtml_extract (itemNode, 1, NULL);
	if (tmp) {
		item_set_description (ctxt->item, tmp);
		g_free(tmp);
	}

	if (ctxt->item->sourceId && ctxt->item->title && ctxt->item->description)
		ctxt->items = g_list_append (ctxt->items, ctxt->item);
	else
		item_unload (ctxt->item);
}

/**
 * Parses given data as a HTML5 document
 *
 * @param ctxt		the feed parser context
 * @param cur		the root node of the XML document
 */
static void
html5_feed_parse (feedParserCtxtPtr ctxt, xmlNodePtr root)
{
	gchar		*tmp;
	xmlNodePtr	cur;
	xmlChar		*baseURL = xmlNodeGetBase (root->doc, root);

	ctxt->feed->time = time(NULL);

	/* For HTML5 source the homepage is the source */
	subscription_set_homepage (ctxt->subscription, ctxt->subscription->source);

	/* Set the default base to the feed's HTML URL if not set yet */
	if (baseURL == NULL)
		xmlNodeSetBase (root, (xmlChar *)ctxt->subscription->source);

	if ((cur = xpath_find (root, "/html/head/title"))) {
		ctxt->title = unxmlize (xhtml_extract (cur, 0, NULL));
	}

	if ((cur = xpath_find (root, "/html/head/meta[@name = 'description']"))) {
		tmp = xhtml_extract (cur, 0, NULL);
		if (tmp) {
			metadata_list_set (&ctxt->subscription->metadata, "description", tmp);
			g_free (tmp);
		}
	}

	if (!xpath_foreach_match (root, "/html/body//article", html5_feed_parse_article, ctxt)) {
		g_string_append(ctxt->feed->parseErrors, "<p>Could not find HTML5 tags!</p>");
		return;
	}
}

static void
html5_feed_check_article (xmlNodePtr cur, gpointer userdata)
{
	gint *articleCount = (gint *)userdata;

	/* We consider <h1>...<h5> tags inside an article as indication
	   for extract worthy content */
	if (xpath_find (cur, ".//h1 | .//h2 | .//h3 | .//h4 | .//h5"))
		(*articleCount)++;
}

static gboolean
html5_feed_check (xmlDocPtr doc, xmlNodePtr root)
{
	gint		articleCount = 0;

	/* A HTML5 website that we can parse like a feed must meet the
	   following criteria

		- XHTML
		- multiple <article> tags
		- inside the <article> an <h1> <h2> or <h3> tag that we can use as title
	 */
	xpath_foreach_match (root, "/html/body//article", html5_feed_check_article, &articleCount);

	// let's say 3 suffices for "multiple" articles
	if (articleCount >= 3)
	   	return TRUE;

	return FALSE;
}

feedHandlerPtr
html5_init_feed_handler (void)
{
	feedHandlerPtr	fhp;

	fhp = g_new0 (struct feedHandler, 1);

	/* prepare feed handler structure */
	fhp->typeStr = "html5";
	fhp->feedParser	= html5_feed_parse;
	fhp->checkFormat = html5_feed_check;
	fhp->html = TRUE;

	return fhp;
}
