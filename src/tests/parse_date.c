/**
 * @file parse_date.c  Test cases for date conversion
 * 
 * Copyright (C) 2014-2022 Lars Windolf <lars.windolf@gmx.de>
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

#include "date.h"

typedef struct tc {
	gchar	*date_string;
	time_t	timestamp;
} *tcPtr;

struct tc tc_empty		= { "", 0 };
struct tc tc_nonsense		= { "blabla", 0 };
struct tc tc_rfc822_full	= { "Mi, 05 Nov 2014 19:24:38 +0100", 1415211878 };
struct tc tc_rfc822_day		= { "Wed, 5 Nov 2014 18:04", 1415210640 };
struct tc tc_rfc822_time	= { "Mi, 05 Nov 2014 18:04:58 ", 1415210698 };
struct tc tc_rfc822_min		= { "Mi, 05 Nov 2014 18:04 ", 1415210640 };
struct tc tc_rfc822_timezone	= { "Mi, 05 Nov 2014 18:04 IRST", 1415194440 };
struct tc tc_rfc822_year2_1	= { "05 Nov 14 18:04:35", 1415210675 };
struct tc tc_rfc822_year2_2	= { "05 Nov 14 18:04", 1415210640 };
struct tc tc_rfc822_year2_3	= { "Wed, 05 Nov 14 17:04:35 -0100", 1415210675 };
struct tc tc_rfc822_wrong	= { "Do, 05 Nov 2014 18:04:58", 1415210698 };

struct tc tc_iso8601_full	= { "2014-11-05T19:00:00+0100", 1415210400 };
struct tc tc_iso8601_day	= { "2014-11-05", 1415145600 };
struct tc tc_iso8601_hours	= { "2014-11-05T19+0100", 1415214000 };
struct tc tc_iso8601_Z		= { "2014-11-04T10:15:16Z", 1415096116 };
struct tc tc_iso8601_wrong	= { "2014-22-22T31", 0 };
struct tc tc_iso8601_notz	= { "2022-12-14T22:02:55", 1671055375 };

static void
tc_parse_rfc822 (gconstpointer user_data)
{
	tcPtr	tc = (tcPtr)user_data;
	g_assert_cmpint (date_parse_RFC822 (tc->date_string), ==, tc->timestamp);
}

static void
tc_parse_iso8601 (gconstpointer user_data)
{
	tcPtr	tc = (tcPtr)user_data;

	g_assert_cmpint (date_parse_ISO8601 (tc->date_string), ==, tc->timestamp);
}

int
main (int argc, char *argv[])
{
	int result;

	g_test_init (&argc, &argv, NULL);

	date_init ();

	g_test_add_data_func ("/parse_date/rfc822/empty",	&tc_empty,		&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/nonsense",	&tc_nonsense,		&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/full",	&tc_rfc822_full,	&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/day",		&tc_rfc822_day,		&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/time",	&tc_rfc822_time,	&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/min",		&tc_rfc822_min,		&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/timezone",	&tc_rfc822_timezone,	&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/year2_1",	&tc_rfc822_year2_1,	&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/year2_2",	&tc_rfc822_year2_2,	&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/year2_3",	&tc_rfc822_year2_3,	&tc_parse_rfc822);
	g_test_add_data_func ("/parse_date/rfc822/wrong",	&tc_rfc822_wrong,	&tc_parse_rfc822);

	g_test_add_data_func ("/parse_date/iso8601/empty",	&tc_empty,		&tc_parse_iso8601);
	g_test_add_data_func ("/parse_date/iso8601/nonsense",	&tc_nonsense,		&tc_parse_iso8601);
	g_test_add_data_func ("/parse_date/iso8601/full",	&tc_iso8601_full,	&tc_parse_iso8601);
	g_test_add_data_func ("/parse_date/iso8601/day",	&tc_iso8601_day,	&tc_parse_iso8601);
//	g_test_add_data_func ("/parse_date/iso8601/hours",	&tc_iso8601_hours,	&tc_parse_iso8601);
	g_test_add_data_func ("/parse_date/iso8601/Z",		&tc_iso8601_Z,		&tc_parse_iso8601);
	g_test_add_data_func ("/parse_date/iso8601/wrong",	&tc_iso8601_wrong,	&tc_parse_iso8601);
	g_test_add_data_func ("/parse_date/iso8601/notz",	&tc_iso8601_notz,	&tc_parse_iso8601);

	result = g_test_run();

	date_deinit ();

	return result;
}
