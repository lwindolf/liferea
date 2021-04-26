/**
 * @file favicon.c  Test cases for favicon auto discovery
 *
 * Copyright (C) 2020 Lars Windolf <lars.windolf@gmx.de>
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

#include "favicon.h"
#include "metadata.h"
#include "subscription.h"

/* Order of test data fields
 * 1. --> feed source URL
 * 2. --> HTML website URL
 * 3. --> feed provided icon URL
 *
 * Now the following results
 * 1. --> favicon from the feed (e.g. <icon> tag in atom feeds) (optional if 3. from above is NULL)
 * 2. --> HTML website URL (mandatory, needs to be identical to 2. from above)
 * 3. --> HTML website URL root of webserver
 * 4. --> feed URL root (e.g. server hosting the feed URL)
 * 5. --> feed URL base (e.g. server + path hosting the feed)
 * 6. --> downloading favicon from root of webserver of the RSS feed
 */

/** feed handler interface */
typedef struct tc {
	gchar *feedSource;
	gchar *htmlSource;
	gchar *iconUrl;		/**< can be NULL */

	gchar *results[];	/**< NULL terminated list of result URLs */
} tc;

// normal feed on same domain without icon in feed
tc tc1 = {
	"https://slashdot.org/slashdot.rss",
	"https://slashdot.org/",
	NULL,
	{
		// 1.) is missing
 		"https://slashdot.org/",
		"https://slashdot.org",
		"https://slashdot.org/favicon.ico",
		"https://slashdot.org/favicon.ico",
		"https://slashdot.org/favicon.ico",
		NULL
	}
};

// feed provides an icon link
tc tc2 = {
	"https://slashdot.org/feed/slashdot.rss",
	"https://slashdot.org/news/",
	"https://slashdot.org/favicon.ico",
	{
		"https://slashdot.org/favicon.ico",
		"https://slashdot.org/news/",
		"https://slashdot.org/feed",
		"https://slashdot.org/favicon.ico",
		"https://slashdot.org/feed/favicon.ico",
		"https://slashdot.org/favicon.ico",
		NULL
	}
};

// feed on different domain
tc tc3 = {
	"https://example.com/atom.xml",
	"https://news.com/news/",
	NULL,
	{
		// 1.) is missing
		"https://news.com/news/",
		"https://example.com",
		"https://news.com/favicon.ico",
		"https://example.com/favicon.ico",
		"https://example.com/favicon.ico",
		NULL
	}
};

// garbage test #1
tc tc4 = {
	"",
	"",
	NULL,
	{
		NULL		// all garbage tests are expected to return just NULL
	}
};

// garbage test #2
tc tc5 = {
	"/",
	"aslkslkslkj",
	NULL,
	{
		NULL
	}
};

// garbage test #3
tc tc6 = {
	"::::",
	"//",
	NULL,
	{
		NULL
	}
};

static void
tc_favicon_get_urls (gconstpointer user_data)
{
	subscriptionPtr	s;
	tc		*t = (tc *)user_data;
	GSList		*url, *results;
	gchar		*tmp;
	gint		i = 0;
	gboolean	feedsEqual = TRUE;

	/* Prepare test subscription */
	s = subscription_new (NULL, NULL, NULL);
	s->source = g_strdup (t->feedSource);
	metadata_list_set (&s->metadata, "icon", t->iconUrl);

	/* Run */
	results = favicon_get_urls (s, t->htmlSource);

	/* Compare URLs */
	url = results;
	while (t->results[i]) {
		if (!url->data || 0 != g_strcmp0 (t->results[i], url->data))
			feedsEqual = FALSE;

		url = g_slist_next (url);
		i++;
	}

	// On fail provide readable comparison
	if (g_slist_length (results) != i || !feedsEqual) {
		g_print ("URL lists mismatch!\n");
		g_print ("expected:\n");
		for (int j = 0; t->results[j] != NULL; j++) {
			g_print (" - %s\n", t->results[j]);
		}
		g_print ("actual:\n");
		for (url = results; url != NULL; url = g_slist_next (url)) {
			g_print (" - %s\n", (gchar *)url->data);
		}
	}

	g_assert (g_slist_length (results) == i);
	g_assert (feedsEqual);

	// Cleanup so we can use memcheck
	subscription_free (s);
	g_slist_free_full (results, g_free);
}

int
main (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	g_test_add_data_func ("/favicon/tc1", &tc1, &tc_favicon_get_urls);
	g_test_add_data_func ("/favicon/tc2", &tc2, &tc_favicon_get_urls);
	g_test_add_data_func ("/favicon/tc3", &tc3, &tc_favicon_get_urls);
	g_test_add_data_func ("/favicon/tc4", &tc4, &tc_favicon_get_urls);
	g_test_add_data_func ("/favicon/tc5", &tc5, &tc_favicon_get_urls);
	g_test_add_data_func ("/favicon/tc6", &tc6, &tc_favicon_get_urls);

	return g_test_run();
}
