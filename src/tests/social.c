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
	gchar   *name;
	gchar	*url;
	gchar	*result;
} tc;

static const tc tests[] = {
	{
		.name = "/social/tc_url",
		.url = "https://example.com/add?u={url}&t={title}",
		.result = "https://example.com/add?u=https://coolsite.org&t=TITLE"
	},
	{
		.name = "/social/tc_url2",
		.url = "https://example.com/add?u={url}&abc",
		.result = "https://example.com/add?u=https://coolsite.org&abc"
	},
	{
		.name = "/social/tc_title1",
		.url = "https://example.com/add?u={url}&t={title}",
		.result = "https://example.com/add?u=https://coolsite.org&t=TITLE"
	},
	{
		.name = "/social/tc_title2",
		.url = "https://example.com/add?t={title}&u={url}&abc",
		.result = "https://example.com/add?t=TITLE&u=https://coolsite.org&abc"
	}
};

static void
tc_build_uri (gconstpointer user_data)
{
	tc			*test = (tc *)user_data;
	g_autofree gchar	*result;
	GSList			*iter;

	social_register_bookmark_site (test->name, test->url);

	g_assert (bookmarkSites != NULL);

	// somewhat hacky: force site active
	iter = bookmarkSites;
	while (iter) {
		if (g_str_equal (((socialSitePtr)iter->data)->name, test->name))
			break;

		iter = g_slist_next (iter);
	}
	g_assert (iter);

	bookmarkSite = (socialSitePtr)iter->data;	// select our only added bookmark site
	result = social_get_bookmark_url ("https://coolsite.org", "TITLE");
	g_assert_cmpstr (result, ==, test->result);
}

int
test_social (int argc, char *argv[])
{
	int result;

	g_test_init (&argc, &argv, NULL);

	// Do not call social_init () as it would need gsettings
	g_assert (bookmarkSites == NULL);

        for (guint i = 0; i < G_N_ELEMENTS (tests); i++)
                g_test_add_data_func (tests[i].name, &tests[i], &tc_build_uri);

	result = g_test_run();

	return result;
}
