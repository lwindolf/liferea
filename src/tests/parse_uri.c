/**
 * @file parse_uri.c  Test cases for URI parsing
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

#include "common.h"

/* Format of sanitize test cases:

   1.     input URI
   2.     escaped output URI
 */

gchar *tc_uri_sanitize1[] = {
	"https://example.com/?abc=123&def=15",
	"https://example.com/?abc=123&def=15",
	NULL
};

gchar *tc_uri_sanitize2[] = {	// plus sign
	"https://example.com/?abc=1+2",
	"https://example.com/?abc=1+2",
	NULL
};

gchar *tc_uri_sanitize3[] = {	// spaces
	"https://example.com/?abc=1 2",
	"https://example.com/?abc=1%202",
	NULL
};

gchar *tc_uri_sanitize4[] = {	// non UTF-8 encoded characters (see Github #904) must not be decoded
	"https://example.com/?szukaj=%AF%F3%B3ty%20dom",
	"https://example.com/?szukaj=%AF%F3%B3ty%20dom",
	NULL
};

gchar *tc_uri_sanitize5[] = {	// umlauts and quotes
	"https://example.com/?abc=äöü&def=?'\"",
	"https://example.com/?abc=%C3%A4%C3%B6%C3%BC&def=?'\"",
	NULL
};

// Base URI test cases
//
// 1. URL to make absolute
// 2. base URI
// 3. result

gchar *tc_base_uri1[] = {
	"https://example.com/a/b.html",
	"https://example.com",
	"https://example.com/a/b.html",
	NULL
};

gchar *tc_base_uri2[] = {
	"https://example.com/a/b.html",
	"http://example.com",
	"https://example.com/a/b.html",
	NULL
};

gchar *tc_base_uri3[] = {
	"https://example.com/a/b.html",
	"htp://example.com",
	"https://example.com/a/b.html",
	NULL
};

gchar *tc_base_uri4[] = {
	"/a/b.html",
	"https://example.com",
	"https://example.com/a/b.html",
	NULL
};

gchar *tc_base_uri5[] = {
	"a/b.html",
	"https://example.com",
	"https://example.com/a/b.html",
	NULL
};

gchar *tc_base_uri6[] = {
	"/b.html",
	"https://example.com/a",
	"https://example.com/b.html",
	NULL
};

gchar *tc_base_uri7[] = {
	"b.html",
	"https://example.com/a",
	"https://example.com/b.html",
	NULL
};

gchar *tc_base_uri8[] = {
	"/b.html",
	"https://example.com/a/",
	"https://example.com/b.html",
	NULL
};

static void
tc_uri_sanitize (gconstpointer user_data)
{
	gchar			**tc = (gchar **)user_data;
	g_autofree gchar	*sanitized = NULL;

	sanitized = (gchar *)common_uri_sanitize ((xmlChar *)tc[0]);
	g_assert_cmpstr (sanitized, ==, tc[1]);
}

static void
tc_base_uri (gconstpointer user_data)
{
	gchar			**tc = (gchar **)user_data;
	g_autofree gchar	*result = NULL;

	result = (gchar *)common_build_url (tc[0], tc[1]);
	g_assert_cmpstr (result, ==, tc[2]);
}

int
main (int argc, char *argv[])
{
	int result;

	g_test_init (&argc, &argv, NULL);

	g_test_add_data_func ("/uri/sanitize1",	&tc_uri_sanitize1,	&tc_uri_sanitize);
	g_test_add_data_func ("/uri/sanitize2",	&tc_uri_sanitize2,	&tc_uri_sanitize);
	g_test_add_data_func ("/uri/sanitize3",	&tc_uri_sanitize3,	&tc_uri_sanitize);
	g_test_add_data_func ("/uri/sanitize4",	&tc_uri_sanitize4,	&tc_uri_sanitize);
	g_test_add_data_func ("/uri/sanitize5",	&tc_uri_sanitize5,	&tc_uri_sanitize);

	g_test_add_data_func ("/uri/base1",	&tc_base_uri1,	&tc_base_uri);
	g_test_add_data_func ("/uri/base2",	&tc_base_uri2,	&tc_base_uri);
	g_test_add_data_func ("/uri/base3",	&tc_base_uri3,	&tc_base_uri);
	g_test_add_data_func ("/uri/base4",	&tc_base_uri4,	&tc_base_uri);
	g_test_add_data_func ("/uri/base5",	&tc_base_uri5,	&tc_base_uri);
	g_test_add_data_func ("/uri/base6",	&tc_base_uri6,	&tc_base_uri);
	g_test_add_data_func ("/uri/base7",	&tc_base_uri7,	&tc_base_uri);
	g_test_add_data_func ("/uri/base8",	&tc_base_uri8,	&tc_base_uri);

	result = g_test_run();

	return result;
}
