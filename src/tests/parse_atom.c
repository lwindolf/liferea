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

        1.     feed XML string
        2.     "true" for successfully parsed feed, "false" for unparseable
        3.     number of items
        4.     string of JSON serialized node
        5..n   string of JSON serialized items

 */

gchar *tc_atom_feed[] = {
        "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>T</title><link href=\"http://localhost\"/><entry><title>i1</title><link href=\"http://localhost/item1.html\"/><summary>D</summary></entry></feed>",
        "true",
        "1",
        "{\"type\":\"feed\",\"id\":\"dummy\",\"title\":null,\"unreadCount\":0,\"children\":0,\"source\":null,\"origSource\":null,\"error\":0,\"updateError\":null,\"httpError\":null,\"httpErrorCode\":0,\"filterError\":null,\"metadata\":[{\"homepage\":\"http://localhost\"}]}",
        "{\"id\":0,\"title\":\"i1\",\"description\":\"D\",\"source\":\"http://localhost/item1.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":1678397817,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":false,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[],\"enclosures\":[]}",
        NULL
};

// Test case with rel="alternate" and rel="self" links (alternate must win)
gchar *tc_atom_feed_rel_alternate[] = {
        "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>Bits from Debian</title><link href=\"https://bits.debian.org/\" rel=\"alternate\"/><link href=\"https://bits.debian.org/feeds/atom.xml\" rel=\"self\"/><id>https://bits.debian.org/</id><updated>2025-09-21T13:05:00+02:00</updated><entry><title>Bits From Argentina - August 2025</title><link href=\"https://bits.debian.org/2025/09/bits-from-argentina-august-2025.html\" rel=\"alternate\"/><published>2025-09-21T13:05:00+02:00</published><updated>2025-09-21T13:05:00+02:00</updated><author><name>Emmanuel Arias</name></author><id>tag:bits.debian.org,2025-09-21:/2025/09/bits-from-argentina-august-2025.html</id><content type=\"html\"><p>DebConf26 is already ...</p></content><category term=\"events\"/><category term=\"debconf26\"/><category term=\"Argentina\"/></entry></feed>",
        "true",
        "1",
        "{\"type\":\"feed\",\"id\":\"dummy\",\"title\":null,\"unreadCount\":0,\"children\":0,\"source\":null,\"origSource\":null,\"error\":0,\"updateError\":null,\"httpError\":null,\"httpErrorCode\":0,\"filterError\":null,\"metadata\":[{\"homepage\":\"https://bits.debian.org/\"}]}",
        "{\"id\":0,\"title\":\"i1\",\"description\":\"D\",\"source\":\"https://bits.debian.org/2025/09/bits-from-argentina-august-2025.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":1678397817,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":false,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[],\"enclosures\":[]}",
        NULL
};

// Test case with only rel="self" link
gchar *tc_atom_feed_rel_self[] = {
        "<?xml version=\"1.0\" encoding=\"utf-8\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><title>Bits from Debian</title><link href=\"https://bits.debian.org/feeds/atom.xml\" rel=\"self\"/><id>https://bits.debian.org/</id><updated>2025-09-21T13:05:00+02:00</updated><entry><title>Bits From Argentina - August 2025</title><link href=\"https://bits.debian.org/2025/09/bits-from-argentina-august-2025.html\" rel=\"alternate\"/><published>2025-09-21T13:05:00+02:00</published><updated>2025-09-21T13:05:00+02:00</updated><author><name>Emmanuel Arias</name></author><id>tag:bits.debian.org,2025-09-21:/2025/09/bits-from-argentina-august-2025.html</id><content type=\"html\"><p>DebConf26 is already in the air in Argentina. Organizing DebConf26 give us the opportunity to talk about Debian in our country again. This is not the first time that Debian has come here, previously Argentina has hosted DebConf 8 in Mar del Plata.</p> <p>In August, Nattie Mayer-Hutchings and Stefano Rivera from DebConf Committee visited the venue where the next DebConf will take place. They came to Argentina in order to see what it is like to travel from Buenos Aires to Santa Fe (the venue of the next DebConf). In addition, they were able to observe the layout and size of the classrooms and halls, as well as the infrastructure available at the venue, which will be useful for the Video Team.</p> <p>But before going to Santa Fe, on the August 27th, we organized a meetup in Buenos Aires at <a href=\"https://www.gcoop.coop/\">GCoop</a>, where we hosted some talks:</p> <ul> <li>¿Qué es Debian? - Pablo Gonzalez (sultanovich) / Emmanuel Arias</li> <li><a href=\"http://osiux.com/2022-10-20-howto-migrate-6300-hosts-to-gnu-linux-using-ansible-and-awx.html\">Cooperativismo y Software Libre</a> - Osiux (gcoop)</li> <li><a href=\"https://2025-08-debconf-talk-5a6e05.pages.debian.net/presentation.html\">Debian and DebConf</a> (Stefano Rivera)</li> </ul> <p><img alt=\"GCoop Talks\" src=\"https://bits.debian.org/images/gcoop.jpg\"></p> <p>On August 28th, we had the opportunity to get to know the Venue. We walked around the city and, obviously, sampled some of the beers from Santa Fe.</p> <p>On August 29th we met with representatives of the University and local government who were all very supportive. We are very grateful to them for opening their doors to DebConf.</p> <p><img alt=\"UNL Meeting\" src=\"https://bits.debian.org/images/debconf_university_unl.jpg\"></p> <p>In the afternoon we met some of the local free software community at an event we held in ATE <a href=\"https://www.ate.org/\">Santa Fe</a>. The event included several talks:</p> <ul> <li>¿Qué es Debian? - Pablo (sultanovich) / Emmanuel Arias</li> <li>Ciberrestauradores: Gestores de basura electrónica - Programa RAEES Acutis</li> <li>Debian and DebConf (Stefano Rivera/Nattie Mayer-Hutchings)</li> </ul> <p><img alt=\"ATE Talks\" src=\"https://bits.debian.org/images/ate_talks.jpg\"></p> <p>Thanks to Debian Argentina, and all the people who will make DebConf26 possible.</p> <p>Thanks to Nattie Mayer-Hutchings and Stefano Rivera for reviewing an earlier version of this article.</p></content><category term=\"events\"/><category term=\"debconf26\"/><category term=\"Argentina\"/></entry></feed>",
        "true",
        "1",
        "{\"type\":\"feed\",\"id\":\"dummy\",\"title\":null,\"unreadCount\":0,\"children\":0,\"source\":null,\"origSource\":null,\"error\":0,\"updateError\":null,\"httpError\":null,\"httpErrorCode\":0,\"filterError\":null,\"metadata\":[{\"homepage\":\"https://bits.debian.org/feeds/atom.xml\"}]}",
        "{\"id\":0,\"title\":\"i1\",\"description\":\"D\",\"source\":\"https://bits.debian.org/2025/09/bits-from-argentina-august-2025.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":1678397817,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":false,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[],\"enclosures\":[]}",
        NULL
};

static void
tc_parse_feed (gconstpointer user_data)
{
        gchar			**tc = (gchar **)user_data;
        Node			*node;
        feedParserCtxtPtr 	ctxt;
        int			i;
        GList			*iter;
        gchar			*json = NULL;

        node = node_new ("feed");
        g_free (node->id);
        node->id = g_strdup ("dummy");  // Force non-random ID for testing
        node_set_subscription (node, subscription_new (NULL, NULL, NULL));
        ctxt = feed_parser_ctxt_new (node->subscription, tc[0], strlen(tc[0]));

        g_assert_cmpstr (feed_parse (ctxt)?"true":"false", ==, tc[1]);
        g_assert (g_list_length (ctxt->items) == (guint)atoi(tc[2]));

        json = node_to_json (node);
        g_assert_cmpstr (tc[3], ==, json);
        g_free (json);

        i = 3;
        iter = ctxt->items;
        while (iter && tc[++i]) {
                itemPtr item = (itemPtr)iter->data;
                item->time = 1678397817;
                json = item_to_json (item);

                g_assert_cmpstr (tc[i], ==, json);
                g_free (json);

                iter = g_list_next (iter);
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

        g_test_add_data_func ("/atom/feed",                     &tc_atom_feed,		                &tc_parse_feed);
        g_test_add_data_func ("/atom/feed_rel_alternate",	&tc_atom_feed_rel_alternate,		&tc_parse_feed);
        g_test_add_data_func ("/atom/feed_rel_self",	        &tc_atom_feed_rel_self, 		&tc_parse_feed);

        result = g_test_run();

        xml_deinit ();

        return result;
}
