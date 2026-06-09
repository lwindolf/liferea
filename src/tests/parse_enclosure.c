/**
 * @file parse_enclosure.c  Test cases for enclosure handling
 *
 * Copyright (C) 2026 Lars Windolf <lars.windolf@gmx.de>
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

 #include <glib.h>

#include "debug.h"
#include "enclosure.h"
#include "node_providers/feed.h"
#include "feed_parser.h"
#include "item.h"
#include "json.h"
#include "subscription.h"

typedef struct tcEnclosureDiscovery {
        const gchar	*name;
        const gchar     *feed;          // a feed source string
        const gchar     *itemJson;      // expected serialized JSON of first item
} tcEnclosureDiscovery;

// discovery test cases test for enclosures in the first item only
static const tcEnclosureDiscovery tc_discovery[] = {
        {
                "/enclosure/discovery/atom_with_link_rel_enclosure",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>T</title><link href=\"http://localhost\"/><entry><title>i1</title><link href=\"http://localhost/item1.html\"/><summary>D</summary><link rel=\"enclosure\" type=\"audio/mpeg\" length=\"1337\" href=\"http://example.org/audio/ph34r_my_podcast.mp3\"/></entry></feed>",
                "{\"id\":0,\"title\":\"i1\",\"description\":\"D\",\"source\":\"http://localhost/item1.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":0,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":true,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[{\"enclosure\":{\"mime\":\"audio/mpeg\",\"size\":1337,\"url\":\"http://example.org/audio/ph34r_my_podcast.mp3\"}}]}"        },
        {
                "/enclosure/discovery/atom_with_multiple_links",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>T</title><link href=\"http://localhost\"/><entry><title>i1</title><link href=\"http://localhost/item1.html\"/><summary>D</summary><link rel=\"enclosure\" type=\"audio/mpeg\" length=\"1337\" href=\"http://example.org/audio/ph34r_my_podcast.mp3\"/><link rel=\"enclosure\" type=\"audio/mpeg\" length=\"1337\" href=\"http://example.org/audio/ph34r_my_podcast2.mp3\"/></entry></feed>",
                "{\"id\":0,\"title\":\"i1\",\"description\":\"D\",\"source\":\"http://localhost/item1.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":0,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":true,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[{\"enclosure\":{\"mime\":\"audio/mpeg\",\"size\":1337,\"url\":\"http://example.org/audio/ph34r_my_podcast.mp3\"}},{\"enclosure\":{\"mime\":\"audio/mpeg\",\"size\":1337,\"url\":\"http://example.org/audio/ph34r_my_podcast2.mp3\"}}]}"
        },
        {
                "/enclosure/discovery/rss2_with_ns_media_audio",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><rss version=\"2.0\" xmlns:media=\"http://search.yahoo.com/mrss/\"><channel><title>T</title><link>http://localhost</link><item><title>i1</title><link>http://localhost/item1.html</link><description>D</description><media:content url=\"https://example.com/cool.ogg\" type=\"audio/ogg\"/></item></channel></rss>",
                "{\"id\":0,\"title\":\"i1\",\"description\":\"<div xmlns=\\\"http://www.w3.org/1999/xhtml\\\" xmlns:media=\\\"http://search.yahoo.com/mrss/\\\">D</div>\",\"source\":\"http://localhost/item1.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":0,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":true,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[{\"enclosure\":{\"mime\":\"audio/ogg\",\"url\":\"https://example.com/cool.ogg\"}}]}"
        },
        {
                "/enclosure/discovery/rss2_with_ns_media_video",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><rss version=\"2.0\" xmlns:media=\"http://search.yahoo.com/mrss/\"><channel><title>T</title><link>http://localhost</link><item><title>i1</title><link>http://localhost/item1.html</link><description>D</description><media:content url=\"https://example.com/cool.mp4\" type=\"video/mp4\" width=\"400\" height=\"300\"/></item></channel></rss>",
                "{\"id\":0,\"title\":\"i1\",\"description\":\"<div xmlns=\\\"http://www.w3.org/1999/xhtml\\\" xmlns:media=\\\"http://search.yahoo.com/mrss/\\\">D</div>\",\"source\":\"http://localhost/item1.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":0,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":true,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[{\"enclosure\":{\"mime\":\"video/mp4\",\"width\":400,\"height\":300,\"url\":\"https://example.com/cool.mp4\"}}]}"
        },
        {
                "/enclosure/discovery/rss2_with_ns_media_multiple",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><rss version=\"2.0\" xmlns:media=\"http://search.yahoo.com/mrss/\"><channel><title>T</title><link>http://localhost</link><item><title>i1</title><link>http://localhost/item1.html</link><description>D</description><media:content url=\"https://example.com/cool.mp4\" type=\"video/mp4\" width=\"400\" height=\"300\"/><media:content url=\"https://example.com/cool.ogg\" type=\"audio/ogg\"/></item></channel></rss>",
                "{\"id\":0,\"title\":\"i1\",\"description\":\"<div xmlns=\\\"http://www.w3.org/1999/xhtml\\\" xmlns:media=\\\"http://search.yahoo.com/mrss/\\\">D</div>\",\"source\":\"http://localhost/item1.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":0,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":true,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[{\"enclosure\":{\"mime\":\"video/mp4\",\"width\":400,\"height\":300,\"url\":\"https://example.com/cool.mp4\"}},{\"enclosure\":{\"mime\":\"audio/ogg\",\"url\":\"https://example.com/cool.ogg\"}}]}"
        },
        {
                "/enclosure/discovery/rss1_with_enclosure",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><rss version=\"1.0\"><channel><title>T</title><link>http://localhost</link><item><title>i1</title><link>http://localhost/item1.html</link><description>D</description><enclosure url=\"https://example.com/cool.ogg\" type=\"audio/ogg\" length=\"1337\"/></item></channel></rss>",
                "{\"id\":0,\"title\":\"i1\",\"description\":\"<div xmlns=\\\"http://www.w3.org/1999/xhtml\\\">D</div>\",\"source\":\"http://localhost/item1.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":0,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":true,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[{\"enclosure\":{\"mime\":\"audio/ogg\",\"size\":1337,\"url\":\"https://example.com/cool.ogg\"}}]}"
        }
};

typedef struct tcEnclosureParsing {
	const gchar	*name;
	const gchar	*input;
	gboolean	valid;
	const gchar	*url;
	const gchar	*mime;
	gssize		size;
	gboolean	downloaded;
} tcEnclosureParsing;

static const tcEnclosureParsing tc_parsing[] = {
	{
		"/enclosure/parsing/legacy-only-url",
		"https://example.com/file.mp3",
		TRUE,
		"https://example.com/file.mp3",
		NULL,
		0,
		FALSE
	},
	{
		"/enclosure/parsing/legacy-encoded-full",
		"enc:1:audio/ogg:237423414:https://example.com/cool.ogg",
		TRUE,
		"https://example.com/cool.ogg",
		"audio/ogg",
		237423414,
		TRUE
	},
	{
		"/enclosure/parsing/legacy-encoded-empty-mime",
		"enc:0::12:https://example.com/blob.bin",
		TRUE,
		"https://example.com/blob.bin",
		NULL,
		12,
		FALSE
	},
	{
		"/enclosure/parsing/legacy-malformed",
		"enc:0:audio/mpeg:not-a-number:https://example.com/broken.mp3",
		FALSE,
		NULL,
		NULL,
		0,
		FALSE
	},
        {
                "/enclosure/parsing/json-full",
                "{\"downloaded\":true,\"mime\":\"audio/ogg\",\"size\":1500,\"url\":\"https://example.com/cool.ogg\"}",
                TRUE,
                "https://example.com/cool.ogg",
                "audio/ogg",
                1500,
                TRUE
        },
        {
                "/enclosure/parsing/json-invalid",
                "{\"downloaded\":true,\"mime\":\"audio/ogg\",\"size\":2000}",
                FALSE,
                NULL,
                NULL,
                0,
                FALSE
        },
        {
                "/enclosure/parsing/json-syntax-error",
                "{\"downloaded\":true,\"mime\":\"audi",
                FALSE,
                NULL,
                NULL,
                0,
                FALSE
        }
};

typedef struct tcEnclosureSerialization {
	const gchar	*name;
	const gchar	*url;
	const gchar	*mime;
	gssize		size;
	gboolean	downloaded;
	const gchar	*expected_serialized;
} tcEnclosureSerialization;

static const tcEnclosureSerialization tc_serialization[] = {
	{
		"/enclosure/serialization/full",
		"https://example.com/cool.ogg",
		"audio/ogg",
		237423414,
		TRUE,
		"{\"mime\":\"audio/ogg\",\"size\":237423414,\"url\":\"https://example.com/cool.ogg\"}"
	},
	{
		"/enclosure/serialization/no-mime",
		"https://example.com/nomime.bin",
		NULL,
		0,
		FALSE,
		"{\"url\":\"https://example.com/nomime.bin\"}"
	},
	{
		"/enclosure/serialization/escape-and-clamp",
		"https://example.com/a b.mp3",
		"audio/mpeg",
		-1,
		FALSE,
                "{\"mime\":\"audio/mpeg\",\"url\":\"https://example.com/a%20b.mp3\"}"
	}
};

static void
tc_enclosure_discovery (gconstpointer user_data)
{
        const tcEnclosureDiscovery *tc = (const tcEnclosureDiscovery *)user_data;
        Node *node;
        feedParserCtxtPtr ctxt;

        node = node_new ("feed");
        g_free (node->id);
        node->id = g_strdup ("dummy");  // Force non-random ID for static test cases
        node_set_subscription (node, subscription_new (NULL, NULL, NULL));
        ctxt = feed_parser_ctxt_new (node->subscription, tc->feed, strlen(tc->feed));

        g_assert_true (feed_parse (ctxt));
        g_assert_cmpint (g_list_length (ctxt->items), ==, 1);

        itemPtr item = (itemPtr)ctxt->items->data;
        item->time = 0; // Force fixed time for static test cases
        g_autofree gchar *itemJson = item_to_json (item);
        g_assert_cmpstr (itemJson, ==, tc->itemJson);

        g_list_free_full (ctxt->items, g_object_unref);
        feed_parser_ctxt_free (ctxt);
        g_object_unref (node);
}

static void
tc_enclosure_parsing (gconstpointer user_data)
{
	const tcEnclosureParsing	*tc = (const tcEnclosureParsing *)user_data;
	enclosurePtr			enclosure = enclosure_from_string (tc->input);
	g_autofree gchar		*url = enclosure_get_url (tc->input);

	if (!tc->valid) {
		g_assert_null (enclosure);
		g_assert_null (url);
                return;
	}

	g_assert_nonnull (enclosure);
	g_assert_cmpstr (enclosure->url, ==, tc->url);
	g_assert_cmpstr (enclosure->mime, ==, tc->mime);
	g_assert_true (enclosure->size == tc->size);
	g_assert_cmpint (enclosure->downloaded, ==, tc->downloaded);
	g_assert_cmpstr (url, ==, tc->url);

	enclosure_free (enclosure);
}

static void
tc_enclosure_serialization (gconstpointer user_data)
{
	const tcEnclosureSerialization	*tc = (const tcEnclosureSerialization *)user_data;
	enclosurePtr			e = enclosure_new (tc->url, tc->mime, tc->size, -1, -1);
        g_autofree gchar		*serialized = enclosure_to_string (e);

	g_assert_cmpstr (serialized, ==, tc->expected_serialized);

	enclosure_free (e);
}

int
test_parse_enclosure (int argc, char *argv[])
{
	int result;

	g_test_init (&argc, &argv, NULL);

        for (guint i = 0; i < G_N_ELEMENTS (tc_discovery); i++)
                g_test_add_data_func (tc_discovery[i].name, &tc_discovery[i], &tc_enclosure_discovery);

	for (guint i = 0; i < G_N_ELEMENTS (tc_parsing); i++)
		g_test_add_data_func (tc_parsing[i].name, &tc_parsing[i], &tc_enclosure_parsing);
        
	for (guint i = 0; i < G_N_ELEMENTS (tc_serialization); i++)
		g_test_add_data_func (tc_serialization[i].name, &tc_serialization[i], &tc_enclosure_serialization);

	result = g_test_run ();
	return result;
}