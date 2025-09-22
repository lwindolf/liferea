/**
 * @file parse_rss.c  Test cases for RSS parsing
 *
 * Copyright (C) 2023-2025 Lars Windolf <lars.windolf@gmx.de>
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
   4..n   string of JSON serialized items
 */

gchar *tc_rss_feed1[] = {
	"<rss version=\"2.0\"><channel><title>T</title><link>http://localhost</link><item><title>i1</title><link>http://localhost/item1.html</link><description>D</description></item><item><title>i2</title><link>https://localhost/item2.html</link></item></channel></rss>",
	"true",
	"2",
	"{\"id\":0,\"title\":\"i1\",\"description\":\"<div xmlns=\\\"http://www.w3.org/1999/xhtml\\\">"
#if LIBXML_VERSION < 21400
	"<p>"
#endif
	"D"
#if LIBXML_VERSION < 21400
	"</p>"
#endif
	"</div>\",\"source\":\"http://localhost/item1.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":1678397817,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":false,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[],\"enclosures\":[]}",
	"{\"id\":0,\"title\":\"i2\",\"description\":null,\"source\":\"https://localhost/item2.html\",\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":1678397817,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":false,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[],\"enclosures\":[]}",
	NULL
};

/* Test case to prevent | command injection in item link which could trigger
   a HTML5 extraction */
gchar *tc_rss_feed2_rce[] = {
	"<rss version=\"2.0\"><channel><title>T</title><item><title>i1</title><link>|date >/tmp/bad-item-link.txt</link></item></channel></rss>",
	"true",
	"1",
	"{\"id\":0,\"title\":\"i1\",\"description\":null,\"source\":null,\"readStatus\":false,\"updateStatus\":false,\"flagStatus\":false,\"time\":1678397817,\"validTime\":false,\"validGuid\":false,\"hasEnclosure\":false,\"sourceId\":null,\"nodeId\":null,\"parentNodeId\":null,\"metadata\":[],\"enclosures\":[]}",
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

	node = node_new ("feed");
 	node_set_subscription (node, subscription_new (NULL, NULL, NULL));
	ctxt = feed_parser_ctxt_new (node->subscription, tc[0], strlen(tc[0]));

	g_assert_cmpstr (feed_parse (ctxt)?"true":"false", ==, tc[1]);
	g_assert (g_list_length (ctxt->items) == (guint)atoi(tc[2]));

	i = 2;
	iter = ctxt->items;
	while (iter && tc[++i]) {
		gchar	*json = NULL;

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
test_parse_rss (int argc, char *argv[])
{
	int result;

	g_test_init (&argc, &argv, NULL);

        if (g_strv_contains ((const gchar **)argv, "--debug"))
		debug_set_flags (DEBUG_UPDATE | DEBUG_HTML | DEBUG_PARSING);

	xml_init ();

	g_test_add_data_func ("/rss/feed1",	&tc_rss_feed1,		&tc_parse_feed);
	g_test_add_data_func ("/rss/feed2_rce",	&tc_rss_feed2_rce,	&tc_parse_feed);

	result = g_test_run();

	xml_deinit ();

	return result;
}
