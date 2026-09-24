/**
 * @file net.c  Test cases for networking helper logic
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

#include "net.h"

typedef struct tcRetryAfter {
	const gchar *name;
	const gchar *header;
	gint expected;
} tcRetryAfter;

static const tcRetryAfter tc_retry_after[] = {
	{ "/net/retry-after/valid-seconds", "123", 123 },
	{ "/net/retry-after/invalid-empty", "", 300 },
	{ "/net/retry-after/invalid-nonnumeric", "abc", 300 },
	{ "/net/retry-after/invalid-negative", "-10", 300 },
	{ "/net/retry-after/missing", NULL, 300 },
};

static void
tc_parse_retry_after (gconstpointer user_data)
{
	const tcRetryAfter *tc = (const tcRetryAfter *)user_data;
	g_assert_cmpint (network_get_retry_after_seconds (tc->header), ==, tc->expected);
}

int
test_net (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	for (guint i = 0; i < G_N_ELEMENTS (tc_retry_after); i++)
		g_test_add_data_func (tc_retry_after[i].name, &tc_retry_after[i], &tc_parse_retry_after);

	return g_test_run ();
}
