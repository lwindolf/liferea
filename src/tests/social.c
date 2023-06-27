/**
 * @file social.c  Test cases for bookmarking links
 * 
 * Copyright (C) 2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "conf.h"
#include "social.h"

extern GSList *bookmarkSites;
extern socialSitePtr bookmarkSite;

typedef struct tc {
	gchar	*url;
	gchar	*result;
} *tcPtr;

struct tc tc_url = { "https://example.com/add?u={url}", "https://example.com/add?u=https://coolsite.org" };
struct tc tc_url2 = { "https://example.com/add?u={url}&abc", "https://example.com/add?u=https://coolsite.org&abc" };

struct tc tc_title1 = { "https://example.com/add?u={url}&t={title}", "https://example.com/add?u=https://coolsite.org&t=TITLE" };
struct tc tc_title2 = { "https://example.com/add?t={title}&u={url}&abc", "https://example.com/add?t=TITLE&u=https://coolsite.org&abc" };


static void
tc_build_uri (gconstpointer user_data)
{
	tcPtr			tc = (tcPtr)user_data;
	g_autofree gchar	*result;

	social_free (); // to cleanup previous site

	social_register_bookmark_site ("TEST", tc->url);
	bookmarkSite = bookmarkSites->data;	// select our only added bookmark site
	result = social_get_bookmark_url ("https://coolsite.org", "TITLE");
	g_assert_cmpstr (result, ==, tc->result);
}

int
main (int argc, char *argv[])
{
	int result;

	g_test_init (&argc, &argv, NULL);

	//conf_init ();

	g_test_add_data_func ("/social/url",	&tc_url,	&tc_build_uri);
	g_test_add_data_func ("/social/url2",	&tc_url2,	&tc_build_uri);

	g_test_add_data_func ("/social/title1",	&tc_title1,	&tc_build_uri);
	g_test_add_data_func ("/social/title2",	&tc_title2,	&tc_build_uri);

	result = g_test_run();

	social_free ();

	return result;
}
