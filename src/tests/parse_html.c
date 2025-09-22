/**
 * @file parse_html.c  Test cases for feed link auto discovery
 *
 * Copyright (C) 2014-2023 Lars Windolf <lars.windolf@gmx.de>
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
#include "html.h"

/* We need two groups of autodiscovery test cases, one for the tag soup fuzzy
   finding and one for the XML parsing + XPath extraction variant. */

/* Tag soup test cases */

gchar *tc_xml[] = {
	"<link rel=\"alternate\" type=\"text/xml\" href='http://example.com/news.rss'>",
	"http://example.com",
	"http://example.com/news.rss",
	NULL
};

gchar *tc_xml_base_url[] = {
	"<link rel=\"alternate\" type=\"text/xml\" href='news.rss'>",
	"http://example.com",
	"http://example.com/news.rss",
	NULL
};

gchar *tc_rdf[] = {
	"<link rel=\"alternate\" type=\"application/rdf+xml\" title=\"Aktuelle News von heise online (für ältere RSS-Reader)\" href=\"http://www.heise.de/newsticker/heise.rdf\">",
	"http://example.com",
	"http://www.heise.de/newsticker/heise.rdf",
	NULL
};

gchar *tc_rss[] = {
	"<link rel=\"alternate\" type=\"application/rss+xml\" title=\"Aktuelle News von heise online (für ältere RSS-Reader)\" href=\"http://www.heise.de/newsticker/heise.rdf\">",
	"http://example.com",
	"http://www.heise.de/newsticker/heise.rdf",
	NULL
};

gchar *tc_atom[] = {
	"<link rel=\"alternate\" type=\"application/atom+xml\" title=\"Aktuelle News von heise online\" href=\"http://www.heise.de/newsticker/heise-atom.xml\">",
	"http://example.com",
	"http://www.heise.de/newsticker/heise-atom.xml",
	NULL
};

gchar *tc_atom2[] = {
	"<link rel=\"alternate\" type=\"application/atom+xml\" title=\"Aktuelle News von heise online\" href=\"http://www.heise.de/newsticker/heise-atom.xml\"><link rel=\"alternate\" type=\"application/atom+xml\" title=\"Aktuelle News von heise online\" href=\"http://www.heise.de/newsticker/heise-atom2.xml\">",
	"http://example.com",
	"http://www.heise.de/newsticker/heise-atom.xml",
	"http://www.heise.de/newsticker/heise-atom2.xml",
	NULL
};

gchar *tc_broken_tag[] = {
	"<link rel=\"alternate\" type=\"application/atom+xml\"",
	"http://example.com",
	NULL
};

gchar *tc_garbage[] = {
	"wqdfkjfkj ööö \0564sjj\076jdjdj",
	"http:::::://///.com!",
	NULL
};

/* XML parsable HTML test cases */

gchar *tc_xml_atom[] = {
	"<html><head><link rel=\"alternate\" type=\"application/atom+xml\" title=\"Aktuelle News von heise online\" href=\"http://www.heise.de/newsticker/heise-atom.xml\"></head></html>",
	"http://example.com",
	"http://www.heise.de/newsticker/heise-atom.xml",
	NULL
};

gchar *tc_xml_atom2[] = {
	"<html><head><link rel=\"alternate\" type=\"application/atom+xml\" href=\"http://www.heise.de/newsticker/heise-atom.xml\"><link rel=\"alternate\" type=\"application/atom+xml\" href=\"http://www.heise.de/newsticker/heise-atom2.xml\"></head></html>",
	"http://example.com",
	"http://www.heise.de/newsticker/heise-atom.xml",
	"http://www.heise.de/newsticker/heise-atom2.xml",
	NULL
};

// HTML5 and relative Atom link present at same time (see Github #1033, we expect Atom to win)
gchar *tc_xml_atom3[] = {
	"<!DOCTYPE html>"
	"<html lang=\"en\">"
	"<head>"
	"<meta charset=\"UTF-8\" />"
	"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\" />"
	"<title>About - Site</title>"
	"<link href=\"&#x2F;atom.xml\" rel=\"alternate\" type=\"application/atom+xml\" />"
	"</head>"
	"<body></body></html>",
	"/",
	"/atom.xml",
	NULL
};

// Injection via "|"" command must not result in command subscription
gchar *tc_xml_rce[] = {
	"<html><head><link rel=\"alternate\" type=\"application/rss+xml\" href=\"|date &gt;/tmp/bad-feed-discovery.txt\"></html>",
	NULL,
	NULL
};

static void
tc_auto_discover_link (gconstpointer user_data)
{
	gchar **tc = (gchar **)user_data;
	g_autofree gchar *tmp;
	GSList *list, *result;
	guint	i = 2;

	tmp = g_strdup (tc[0]);
	list = result = html_auto_discover_feed (tmp, tc[1]);
	do {
		if (!tc[i]) {
			g_assert_null (result);
		} else {
			g_assert_cmpstr (tc[i], ==, result->data);
			result = g_slist_next (result);
		}
	} while(tc[i++]);

	g_slist_free_full (list, g_free);
}

int
test_parse_html (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

        if (g_strv_contains ((const gchar **)argv, "--debug"))
		debug_set_flags (DEBUG_UPDATE | DEBUG_HTML | DEBUG_PARSING);

	g_test_add_data_func ("/html/auto_discover_link_xml", &tc_xml, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_xml_base_url", &tc_xml_base_url, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_rss", &tc_rss, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_rdf", &tc_rdf, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_atom", &tc_atom, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_atom2", &tc_atom2, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_broken_tag", &tc_broken_tag, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_garbage", &tc_garbage, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_xml_atom", &tc_xml_atom, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_xml_atom2", &tc_xml_atom2, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_xml_atom3", &tc_xml_atom3, &tc_auto_discover_link);
	g_test_add_data_func ("/html/auto_discover_link_xml_rce", &tc_xml_rce, &tc_auto_discover_link);

	return g_test_run();
}
