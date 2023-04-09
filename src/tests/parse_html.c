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

/* HTML5 extraction test cases */

gchar *tc_article[] = {
	"<html lang='fr'><script>blabla</script><style>body { background:red }</style><body><article><p>1</p></article></body></html>",
	"<p>1</p>\n"
};

gchar *tc_article_main[] = {
	"<html lang='fr'><script>blabla</script><style>body { background:red }</style><body><main><p>1</p></main></body></html>",
	"<p>1</p>\n"
};

gchar *tc_article_main2[] = {
	"<html lang='fr'><script>blabla</script><style>body { background:red }</style><body><main><article><p>1</p></article></main></body></html>",
	"<p>1</p>\n"
};

gchar *tc_article_micro_format[] = {
	"<html><head></head><body><div property='articleBody'><p>1</p></div></body></html>",
	"<p>1</p>\n"
};

gchar *tc_article_cms_content_id[] = {
	"<html><head></head><body><div id='content'><p>1</p></div></body></html>",
	"<p>1</p>\n"
};

gchar *tc_article_missing[] = {
	"<html><head></head><body><p>1</p></body></html>",
	NULL
};

/* this test case is about an empty tag "<x></x>" not being collapsed to "<x/>"
   but to be output as "<x> </x>" instead */
gchar *tc_article_empty_tags[] = {
	"<html><head></head><body><article><p>1</p><div class='something' data-nr='555'></div></article></body></html>",
	"<p>1</p><div class=\"something\" data-nr=\"555\"> </div>\n"
};

/* this test case is about nested empty tags "<x><x></x></x>" being expanded as "<x><x> </x> </x>" */
gchar *tc_article_empty_tags_nested[] = {
	"<html><head></head><body><article><p>1</p><div><div class='something' data-nr='555'></div></div></article></body></html>",
	"<p>1</p><div>\n  <div class=\"something\" data-nr=\"555\"> </div>\n</div>\n"
};

/* this test case is about empty XHTML tags "<x/>" being expanded */
gchar *tc_article_self_closed_tags[] = {
	"<html><head></head><body><article><p>1</p><div class='something' data-nr='555'/></article></body></html>",
	"<p>1</p><div class=\"something\" data-nr=\"555\"> </div>\n"
};

/* this test case is about nested empty XHTML tags "<x/>" being expanded */
gchar *tc_article_self_closed_tags_nested[] = {
	"<html><head></head><body><article><p>1</p><div><div class='something' data-nr='555'/></div></article></body></html>",
	"<p>1</p><div>\n  <div class=\"something\" data-nr=\"555\"> </div>\n</div>\n"
};

/* this test case is about stripping inline script and CSS */
gchar *tc_article_strip_inline_code[] = {
	"<html><head></head><body><article><p>1<script>alert('Hallo');</script></p><style>p { font-size: 2em }</style></article></body></html>",
	"<p>1</p>\n"
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

static void
tc_get_article (gconstpointer user_data)
{
	gchar **tc = (gchar **)user_data;
	gchar *result = html_get_article (tc[0], "https://example.com");
	if (!tc[1])
		g_assert_null (result);
	else
		g_assert_cmpstr (tc[1], ==, result);

	g_free (result);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	if (argv[1] && g_str_equal (argv[1], "--debug"))
		set_debug_level (DEBUG_UPDATE | DEBUG_HTML | DEBUG_PARSING);

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

	g_test_add_data_func ("/html/html5_extract_article", &tc_article, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_main", &tc_article_main, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_main2", &tc_article_main2, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_micro_format", &tc_article_micro_format, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_cms_content_id", &tc_article_cms_content_id, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_missing", &tc_article_missing, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_empty_tags", &tc_article_empty_tags, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_empty_tags_nested", &tc_article_empty_tags_nested, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_self_closed_tags", &tc_article_self_closed_tags, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_self_closed_tags_nested", &tc_article_self_closed_tags_nested, &tc_get_article);
	g_test_add_data_func ("/html/html5_extract_article_strip_inline_code", &tc_article_strip_inline_code, &tc_get_article);

	return g_test_run();
}
