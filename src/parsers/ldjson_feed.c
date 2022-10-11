/**
 * @file ldjson_feed.c  Parsing LD+JSON snippets in HTML5 webpages like feeds
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
#include "ldjson_feed.h"

#include "common.h"
#include "date.h"
#include "feed_parser.h"
#include "json.h"
#include "metadata.h"
#include "xml.h"

static void
ldjson_feed_parse_json_website (JsonNode *node, feedParserCtxtPtr ctxt)
{
	JsonNode	*logo;
	const gchar	*tmp;

	/* Try to extract subscription details (title, logo...)

	   Example:

	   {
		"@context" : "http://schema.org",
		"type" : "WebPage",
		"url" : "https://www.meinestadt.de/",
		"@type" : "Organization",
		"name": "meinestadt.de GmbH",
		"logo" : {
			"@type" : "ImageObject",
			"url" : "https://unternehmen.meinestadt.de/logos/meinestadt_de_logo.jpg",
			"width" : 500,
			"height" : 500
		},
		"sameAs" : [
			"https://www.facebook.com/meinestadt.de",
			"https://www.facebook.com/meinestadt.destellenmarkt",
			"https://www.facebook.com/meinestadt.deapp",
			"https://www.xing.com/company/meinestadt-de",
			"https://www.linkedin.com/company/allesklar.com-ag",
			"https://de.wikipedia.org/wiki/Meinestadt.de",
			"https://www.youtube.com/user/meinestadtde",
			"https://www.instagram.com/meinestadt.de/",
			"https://twitter.com/meinestadt_de"
		]
	    }
	*/

	if ((tmp = json_get_string (node, "name")))
		node_set_title (ctxt->subscription->node, tmp);

	if ((tmp = json_get_string (node, "url")))
		subscription_set_homepage (ctxt->subscription, tmp);

	if ((logo = json_get_node (node, "logo")))
		if ((tmp = json_get_string (logo, "url")))
			metadata_list_set (&ctxt->subscription->metadata, "imageUrl", tmp);
}

static void
ldjson_feed_parse_json_event (JsonNode *node, feedParserCtxtPtr ctxt)
{
	const gchar *tmp;
	const gchar *eventStatus, *type;

	ctxt->item = item_new ();

	/* what we expect here:

	1.) MusicEvent

	{
	"name": "Band name",
	"image": "...",
	"startDate": "2021-06-12T17:00:00+01:00",
	"endDate": "2021-06-12",
	"previousStartDate": "2020-06-17T16:00:00Z",
	"url": "https://example.com",
	"description": "detailed description",
	"offers": {
	   "url": "https://example.com/abc",
	   "availabilityStarts": "2019-07-05T10:00:00Z",
	   "priceCurrency": "GBP",
	   "price": "89",
	   "validFrom": "2019-07-05T10:00:00Z",
	   "@type": "Offer"
	},
	"performer": [
	  {
	    "name": "Performer 1",
	    "sameAs": "https://example.com/performer1",
	    "@type": "MusicGroup"
	  },
	  {
	    "name": "Performer 2",
	    "sameAs": "https://example.com/performer2",
	    "@type": "MusicGroup"
	   }
	],
	"@type": "MusicEvent"
	}
	*/

	/* eventStatus is a schema.org enum e.g. "https://schema.org/EventCancelled"
	   let's strip the prefix and use the enum name as is */
	type = json_get_string (node, "@type");
	eventStatus = json_get_string (node, "eventStatus");
	if (eventStatus && eventStatus == g_strstr_len (eventStatus, -1, "https://schema.org/Event"))
		eventStatus += 24;
	else
		eventStatus = NULL;

	if ((tmp = json_get_string (node, "startDate"))) {
		// schema.org says startDate should be ISO8601, but RFC822
		// is seen to often. So fuzzy match date format.
		if (strstr (tmp, " "))
			item_set_time (ctxt->item, date_parse_RFC822 (tmp));
		else
			item_set_time (ctxt->item, date_parse_ISO8601 (tmp));
	} else {
		// or default to current feed timestamp
		ctxt->item->time = ctxt->feed->time;
	}

	if ((tmp = json_get_string (node, "url"))) {
		xmlChar *link = common_build_url (tmp, ctxt->subscription->source);
		item_set_source (ctxt->item, (const gchar *)link);
		xmlFree (link);

		// we use the link as id, as on websites link point to unique
		// content in 99% of the cases
		ctxt->item->sourceId = g_strdup (tmp);
	}

	if ((tmp = json_get_string (node, "name"))) {
		gchar *title;

		if (eventStatus && !g_str_equal (eventStatus, "Scheduled"))
			title = g_strdup_printf("%s %s (%s)", g_str_equal(type, "MusicEvent")?"🎹":"🎪", tmp, eventStatus?eventStatus:"");
		else
			title = g_strdup (tmp);

		item_set_title (ctxt->item, title);
		g_free (title);
	}

	if ((tmp = json_get_string (node, "description"))) {
		GString *description = g_string_new (NULL);
		const gchar *image = json_get_string (node, "image");

		if (image)
			g_string_append_printf (description, "<p><img src='%s'/></p>", image);

		g_string_append (description, tmp);

		if (eventStatus)
			g_string_append_printf (description, "<p>Status: %s</p>", eventStatus);

		item_set_description (ctxt->item, description->str);
		g_string_free (description, TRUE);
	}

	// FIXME: extract 'location'
	// FIXME: extract 'performers'
	// FIXME: extract offer

	if (ctxt->item->sourceId && ctxt->item->title)
		ctxt->items = g_list_append (ctxt->items, ctxt->item);
	else
		item_unload (ctxt->item);
}

/* demux different LD+JSON types */
static void
ldjson_feed_parse_json_by_type (JsonNode *node, feedParserCtxtPtr ctxt) {
	const gchar *type = json_get_string (node, "@type");

	if (!type)
		return;

	if (g_str_equal (type, "Event") ||
	    g_str_equal (type, "MusicEvent"))
		ldjson_feed_parse_json_event (node, ctxt);

	if (g_str_equal (type, "WebPage"))
		ldjson_feed_parse_json_website (node, ctxt);

	// FIXME: implement offer
	// FIXME: implement job posting

}

static void
ldjson_feed_parse_json (xmlNodePtr xml, gpointer userdata) {
	feedParserCtxtPtr	ctxt = (feedParserCtxtPtr)userdata;
	JsonParser		*json = json_parser_new ();
	JsonNode		*node;

	xmlChar *data = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
	if (data) {
		json_parser_load_from_data (json, (gchar *)data, -1, NULL);
		node = json_parser_get_root (json);
		if (node) {
			/* Loop over array objects or directly use object as is */
			if (JSON_NODE_OBJECT == json_node_get_node_type (node))
				ldjson_feed_parse_json_by_type (node, ctxt);

			if (JSON_NODE_ARRAY == json_node_get_node_type (node)) {
				GList *list = json_array_get_elements (json_node_get_array (node));
				while (list) {
					ldjson_feed_parse_json_by_type ((JsonNode *)list->data, ctxt);
					list = g_list_next (list);
				}
				g_list_free (list);
			}
		}

		g_object_unref (json);
	}
	xmlFree (data);
}

/**
 * Parses given data as a HTML document containing LD+JSON data
 *
 * @param ctxt		the feed parser context
 * @param cur		the root node of the XML document
 */
static void
ldjson_feed_parse (feedParserCtxtPtr ctxt, xmlNodePtr root)
{
	gchar		*tmp;
	xmlNodePtr	cur;
	xmlChar		*baseURL = xmlNodeGetBase (root->doc, root);

	ctxt->feed->time = time(NULL);

	/* For homepage the HTML page itself is the default source */
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

	if(!xpath_foreach_match (root, "//script[@type='application/ld+json']", ldjson_feed_parse_json, ctxt)) {
		g_string_append(ctxt->feed->parseErrors, "<p>Could not find any ld+json tags!</p>");
		return;
	}
}

static void
ldjson_feed_check_json_type (JsonNode *node, gpointer userdata)
{
	const gchar	*type;
	gint 		*entryCount = (gint *)userdata;

	type = json_get_string (node, "@type");
	if (type) {
		/* Let us not count 'WebPage' here as it does not indicate items,
		   but only subscription infos */
		if (g_str_equal (type, "MusicEvent") ||
		    g_str_equal (type, "Event"))
			(*entryCount)++;
	}
}

static void
ldjson_feed_check_json (xmlNodePtr xml, gpointer userdata)
{
	xmlChar		*data;
	JsonParser	*json = json_parser_new ();
	JsonNode	*node;

	data = xmlNodeListGetString (xml->doc, xml->xmlChildrenNode, 1);
	if (data) {
		json_parser_load_from_data (json, (gchar *)data, -1, NULL);
		node = json_parser_get_root (json);
		if (node) {
			/* Loop over array objects or directly use object as is */
			if (JSON_NODE_OBJECT == json_node_get_node_type (node))
				ldjson_feed_check_json_type (node, userdata);

			if (JSON_NODE_ARRAY == json_node_get_node_type (node)) {
				GList *list = json_array_get_elements (json_node_get_array (node));
				while (list) {
					ldjson_feed_check_json_type ((JsonNode *)list->data, userdata);
					list = g_list_next (list);
				}
				g_list_free (list);
			}
		}
		g_object_unref (json);
	}
	xmlFree (data);
}

static gboolean
ldjson_feed_check (xmlDocPtr doc, xmlNodePtr root)
{
	gint		entryCount = 0;

	/* A HTML website with LD+JSON that we can parse like a feed must meet the
	   following criteria

		- XML readable XHTML/HTML5/HTML
		- multiple <script type="application/ld+json"> tags with JSON
		  content indicating accepted types like 'MusicEvent'
	 */
	xpath_foreach_match (root, "//script[@type='application/ld+json']", ldjson_feed_check_json, &entryCount);

	// let's say 3 suffices for "multiple" entries
	if (entryCount >= 3)
	   	return TRUE;

	return FALSE;
}

feedHandlerPtr
ldjson_init_feed_handler (void)
{
	feedHandlerPtr	fhp;

	fhp = g_new0 (struct feedHandler, 1);

	/* prepare feed handler structure */
	fhp->typeStr = "ldjson";
	fhp->feedParser	= ldjson_feed_parse;
	fhp->checkFormat = ldjson_feed_check;
	fhp->html = TRUE;

	return fhp;
}
