/**
 * @file parse_atom.c  Test cases for Atom parsing
 *
 * Copyright (C) 2025 Lars Windolf <lars.windolf@gmx.de>
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
#include <string.h>
#include <libxml/xmlversion.h>

#include "debug.h"
#include "node_providers/feed.h"
#include "feed_parser.h"
#include "item.h"
#include "json.h"
#include "subscription.h"
#include "xml.h"

/* Format of test cases:

        1.     test case name
        2.     feed XML string
        3.     "true" for successfully parsed feed, "false" for unparseable
        4.     number of items
        5.     string of JSON serialized node
        6.     string of JSON serialized 1st item (or NULL)

 */

 typedef struct tcAtomFeed {
        gchar   *name;
        gchar   *xml_string;
        gchar   *parse_result;
        gchar   *num_items;
        gchar   *node_json;
        gchar   *item_json;    // NULL-terminated array of item JSON strings
} *tcAtomFeedPtr;

struct tcAtomFeed tc_atom_feed[] = {
        {
                "/atom/basic feed parse",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>T</title><link href=\"http://localhost\"/><entry><title>i1</title><link href=\"http://localhost/item1.html\"/><summary>D</summary></entry></feed>",
                "true",
                "1",
                "{\"type\":\"feed\",\"id\":\"dummy\",\"title\":null,\"unreadCount\":0,\"children\":0,\"source\":null,\"origSource\":null,\"error\":0,\"updateError\":null,\"httpError\":null,\"httpErrorCode\":0,\"filterError\":null,\"metadata\":[{\"homepage\":\"http://localhost\"}]}",
                "{\"id\":0,\"title\":\"i1\",\"description\":\"D\",\"source\":\"http://localhost/item1.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":1678397817,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":false,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[],\"enclosures\":[]}"
        },
        // Test case with rel="alternate" and rel="self" links (alternate must win)
        {
                "/atom/rel=alternate-vs-self",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>Bits from Debian</title><link href=\"https://bits.debian.org/\" rel=\"alternate\"/><link href=\"https://bits.debian.org/feeds/atom.xml\" rel=\"self\"/><id>https://bits.debian.org/</id><updated>2025-09-21T13:05:00+02:00</updated><entry><title>Bits From Argentina - August 2025</title><link href=\"https://bits.debian.org/2025/09/bits-from-argentina-august-2025.html\" rel=\"alternate\"/><link href=\"nottobeused\" rel=\"self\"/><published>2025-09-21T13:05:00+02:00</published><updated>2025-09-21T13:05:00+02:00</updated><author><name>Emmanuel Arias</name></author><id>tag:bits.debian.org,2025-09-21:/2025/09/bits-from-argentina-august-2025.html</id><content type=\"html\"><p>DebConf26 is already ...</p></content><category term=\"events\"/><category term=\"debconf26\"/><category term=\"Argentina\"/></entry></feed>",
                "true",
                "1",
                "{\"type\":\"feed\",\"id\":\"dummy\",\"title\":null,\"unreadCount\":0,\"children\":0,\"source\":null,\"origSource\":null,\"error\":0,\"updateError\":null,\"httpError\":null,\"httpErrorCode\":0,\"filterError\":null,\"metadata\":[{\"homepage\":\"https://bits.debian.org/\"},{\"contentUpdateDate\":\"2025-09-21T13:05:00+02:00\"}]}",
                "{\"id\":0,\"title\":\"Bits From Argentina - August 2025\",\"description\":\"<div xmlns=\\\"http://www.w3.org/1999/xhtml\\\" xml:base=\\\"https://bits.debian.org/\\\"/>\",\"source\":\"https://bits.debian.org/2025/09/bits-from-argentina-august-2025.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":1678397817,\"validTime\":true,\"validGuid\":false,\"hasEnclosure\":false,\"sourceId\":\"tag:bits.debian.org,2025-09-21:/2025/09/bits-from-argentina-august-2025.html\",\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[{\"pubDate\":\"2025-09-21T13:05:00+02:00\"},{\"author\":\"Emmanuel Arias\"},{\"category\":\"events\"},{\"category\":\"debconf26\"},{\"category\":\"Argentina\"}],\"enclosures\":[]}"
        },
        // Test case with only rel="self" link
        {
                "/atom/rel=self only",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>Bits from Debian</title><link href=\"https://bits.debian.org/feeds/atom.xml\" rel=\"self\"/><id>https://bits.debian.org/</id></feed>",
                "true",
                "0",
                "{\"type\":\"feed\",\"id\":\"dummy\",\"title\":null,\"unreadCount\":0,\"children\":0,\"source\":null,\"origSource\":null,\"error\":0,\"updateError\":null,\"httpError\":null,\"httpErrorCode\":0,\"filterError\":null,\"metadata\":[{\"homepage\":\"https://bits.debian.org/feeds/atom.xml\"}]}",
                NULL
        },
        // Atom link type enclosure
        {      
                "/atom/enclosure link",
                "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>T</title><link href=\"https://example.com/\" rel=\"alternate\"/><entry><title>T</title><link href=\"https://example.com/2025/09/bits-from-argentina-august-2025.html\" rel=\"alternate\"/><link href=\"nottobeused\" rel=\"self\"/><published>2025-09-21T13:05:00+02:00</published><updated>2025-09-21T13:05:00+02:00</updated><author><name>Emmanuel Arias</name></author><id>tag:bits.debian.org,2025-09-21:/2025/09/</id><summary>S</summary><link href=\"https://example.com/enclosure\" rel=\"enclosure\"/></entry></feed>",
                "true",
                "1",
                "{\"type\":\"feed\",\"id\":\"dummy\",\"title\":null,\"unreadCount\":0,\"children\":0,\"source\":null,\"origSource\":null,\"error\":0,\"updateError\":null,\"httpError\":null,\"httpErrorCode\":0,\"filterError\":null,\"metadata\":[{\"homepage\":\"https://example.com/\"}]}",
                "{\"id\":0,\"title\":\"T\",\"description\":\"S\",\"source\":\"https://example.com/2025/09/bits-from-argentina-august-2025.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":1678397817,\"validTime\":true,\"validGuid\":false,\"hasEnclosure\":true,\"sourceId\":\"tag:bits.debian.org,2025-09-21:/2025/09/\",\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[{\"pubDate\":\"2025-09-21T13:05:00+02:00\"},{\"author\":\"Emmanuel Arias\"},{\"enclosure\":\"enc:0::0:https://example.com/enclosure\"}],\"enclosures\":[{\"url\":\"https://example.com/enclosure\",\"mime\":null}]}",
        },
        NULL
};

static void
tc_parse_feed (gconstpointer user_data)
{
        tcAtomFeedPtr		tc = (tcAtomFeedPtr)user_data;
        Node			*node;
        feedParserCtxtPtr 	ctxt;
        gchar			*json = NULL;

        node = node_new ("feed");
        g_free (node->id);
        node->id = g_strdup ("dummy");  // Force non-random ID for static test cases
        node_set_subscription (node, subscription_new (NULL, NULL, NULL));
        ctxt = feed_parser_ctxt_new (node->subscription, tc->xml_string, strlen(tc->xml_string));

        g_assert_cmpstr (feed_parse (ctxt)?"true":"false", ==, tc->parse_result);
        g_assert (g_list_length (ctxt->items) == (guint)atoi(tc->num_items));

        json = node_to_json (node);
        g_assert_cmpstr (tc->node_json, ==, json);
        g_free (json);

        if (ctxt->items) {
                itemPtr item = (itemPtr)ctxt->items->data;
                item->time = 1678397817;
                item->validGuid = FALSE;        // to avoid GUID validation (which requires DB access)
                json = item_to_json (item);
                g_assert_cmpstr (tc->item_json, ==, json);
                g_free (json);
        }

        g_list_free_full (ctxt->items, g_object_unref);
        feed_parser_ctxt_free (ctxt);
        g_object_unref (node);
}

int
test_parse_atom (int argc, char *argv[])
{
        int result;

        g_test_init (&argc, &argv, NULL);

        if (g_strv_contains ((const gchar **)argv, "--debug"))
                debug_set_flags (DEBUG_UPDATE | DEBUG_HTML | DEBUG_PARSING);

        xml_init ();

        for (int i = 0; tc_atom_feed[i].name != NULL; i++) {
		g_test_add_data_func (tc_atom_feed[i].name, &tc_atom_feed[i], &tc_parse_feed);
	}

        result = g_test_run();

        xml_deinit ();

        return result;
}
