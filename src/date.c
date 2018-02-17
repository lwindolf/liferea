/**
 * @file date.c date formatting routines
 * 
 * Copyright (C) 2008-2011  Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006  Nathan J. Conrad <t98502@users.sourceforge.net>
 * 
 * The date formatting was reused from the Evolution code base
 *
 *    Author: Chris Lahey <clahey@ximian.com
 *
 *    Copyright 2001, Ximian, Inc.
 *
 * parts of the RFC822 timezone decoding were reused from the gmime source
 *
 *    Authors: Michael Zucchi <notzed@helixcode.com>
 *             Jeffrey Stedfast <fejj@helixcode.com>
 *
 *    Copyright 2000 Helix Code, Inc. (www.helixcode.com)
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

#include "date.h"

#include <config.h>
#include <locale.h>
#include <ctype.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "e-date.h"

#if defined (G_OS_WIN32) && !defined (HAVE_LOCALTIME_R)
#define localtime_r(t,o) localtime_s (o,t)
#endif

#define	TIMESTRLEN	256

/* date formatting methods */

/* This function is originally from the Evolution 2.6.2 code (e-cell-date.c) */
static gchar *
date_format_nice (time_t date)
{
	time_t nowdate = time(NULL);
	time_t yesdate;
	struct tm then, now, yesterday;
	gchar *temp, *buf;
	gboolean done = FALSE;
	
	if (date == 0) {
		return g_strdup ("");
	}
	
	buf = g_new0(gchar, TIMESTRLEN + 1);

	localtime_r (&date, &then);
	localtime_r (&nowdate, &now);

/*	if (nowdate - date < 60 * 60 * 8 && nowdate > date) {
		e_utf8_strftime_fix_am_pm (buf, TIMESTRLEN, _("%l:%M %p"), &then);
		done = TRUE;
	}*/

	if (!done) {
		if (then.tm_mday == now.tm_mday &&
		    then.tm_mon == now.tm_mon &&
		    then.tm_year == now.tm_year) {
		    	/* translation hint: date format for today, reorder format codes as necessary */
			e_utf8_strftime_fix_am_pm (buf, TIMESTRLEN, _("Today %l:%M %p"), &then);
			done = TRUE;
		}
	}
	if (!done) {
		yesdate = nowdate - 60 * 60 * 24;
		localtime_r (&yesdate, &yesterday);
		if (then.tm_mday == yesterday.tm_mday &&
		    then.tm_mon == yesterday.tm_mon &&
		    then.tm_year == yesterday.tm_year) {
		    	/* translation hint: date format for yesterday, reorder format codes as necessary */
			e_utf8_strftime_fix_am_pm (buf, TIMESTRLEN, _("Yesterday %l:%M %p"), &then);
			done = TRUE;
		}
	}
	if (!done) {
		int i;
		for (i = 2; i < 7; i++) {
			yesdate = nowdate - 60 * 60 * 24 * i;
			localtime_r (&yesdate, &yesterday);
			if (then.tm_mday == yesterday.tm_mday &&
			    then.tm_mon == yesterday.tm_mon &&
			    then.tm_year == yesterday.tm_year) {
			    	/* translation hint: date format for dates older than 2 days but not older than a week, reorder format codes as necessary */
				e_utf8_strftime_fix_am_pm (buf, TIMESTRLEN, _("%a %l:%M %p"), &then);
				done = TRUE;
				break;
			}
		}
	}
	if (!done) {
		if (then.tm_year == now.tm_year) {
			/* translation hint: date format for dates older than a week but from this year, reorder format codes as necessary */
			e_utf8_strftime_fix_am_pm (buf, TIMESTRLEN, _("%b %d %l:%M %p"), &then);
		} else {
			/* translation hint: date format for dates from the last years, reorder format codes as necessary */
			e_utf8_strftime_fix_am_pm (buf, TIMESTRLEN, _("%b %d %Y"), &then);
		}
	}

	temp = buf;
	while ((temp = strstr (temp, "  "))) {
		memmove (temp, temp + 1, strlen (temp));
	}
	temp = g_strstrip (buf);
	return temp;
}

gchar *
date_format (time_t date, const gchar *date_format)
{
	gchar		*result;
	struct tm	date_tm;
	
	if (date == 0) {
		return g_strdup ("");
	}

	if (date_format) {
		localtime_r (&date, &date_tm);
	
		result = g_new0 (gchar, TIMESTRLEN);
		e_utf8_strftime_fix_am_pm (result, TIMESTRLEN, date_format, &date_tm);
	} else {
		result = date_format_nice (date);
	}

	return result;
}

/* date parsing methods */

gint64
date_parse_ISO8601 (const gchar *date)
{
	GTimeVal 	timeval;
	GDateTime 	*datetime = NULL;
	gboolean 	result;
	guint64 	year, month, day;
	gint64 		t = 0;
	gchar		*pos, *next, *ascii_date = NULL;
	
	g_assert (date != NULL);
	
	/* we expect at least something like "2003-08-07T15:28:19" and
	   don't require the second fractions and the timezone info

	   the most specific format:   YYYY-MM-DDThh:mm:ss.sTZD
	 */

	/* full specified variant */
	result = g_time_val_from_iso8601 (date, &timeval);
	if (result)
		return timeval.tv_sec;


	/* only date */
	ascii_date = g_str_to_ascii (date, "C");
	ascii_date = g_strstrip (ascii_date);

	/* Parsing year */
	year = g_ascii_strtoull (ascii_date, &next, 10);
	if ((*next != '-') || (next == ascii_date))
		goto parsing_failed;
	pos = next + 1;

	/* Parsing month */
	month = g_ascii_strtoull (pos, &next, 10);
	if ((*next != '-') || (next == pos))
		goto parsing_failed;
	pos = next + 1;

	/* Parsing day */
	day = g_ascii_strtoull (pos, &next, 10);
	if ((*next != '\0') || (next == pos))
		goto parsing_failed;

	/* there were others combinations too... */

	datetime = g_date_time_new_utc (year, month, day, 0,0,0);
	if (datetime) {
		t = g_date_time_to_unix (datetime);
		g_date_time_unref (datetime);
	}
	
parsing_failed:
	if (!t)
		debug0 (DEBUG_PARSING, "Invalid ISO8601 date format! Ignoring <dc:date> information!");
	g_free (ascii_date);
	return t;
}

/* in theory, we'd need only the RFC822 timezones here
   in practice, feeds also use other timezones...        */
static struct {
	const char *name;
	const char *offset;
} tz_offsets [] = {
	{ "IDLW","-1200" },
	{ "HAST","-1000" },
	{ "AKST","-0900" },
	{ "AKDT","-0800" },
	{ "WESZ","+0100" },
	{ "WEST","+0100" },
	{ "WEDT","+0100" },
	{ "MEST","+0200" },
	{ "MESZ","+0200" },
	{ "CEST","+0200" },
	{ "CEDT","+0200" },
	{ "EEST","+0300" },
	{ "EEDT","+0300" },
	{ "IRST","+0430" },
	{ "CNST","+0800" },
	{ "ACST","+0930" },
	{ "ACDT","+1030" },
	{ "AEST","+1000" },
	{ "AEDT","+1100" },
	{ "IDLE","+1200" },
	{ "NZST","+1200" },
	{ "NZDT","+1300" },
	{ "GMT", "+00" },
	{ "EST", "-0500" },
	{ "EDT", "-0400" },
	{ "CST", "-0600" },
	{ "CDT", "-0500" },
	{ "MST", "-0700" },
	{ "MDT", "-0600" },
	{ "PST", "-0800" },
	{ "PDT", "-0700" },
	{ "HDT", "-0900" },
	{ "YST", "-0900" },
	{ "YDT", "-0800" },
	{ "AST", "-0400" },
	{ "ADT", "-0300" },
	{ "VST", "-0430" },
	{ "NST", "-0330" },
	{ "NDT", "-0230" },
	{ "WET", "+00" },
	{ "WEZ", "+00" },
	{ "IST", "+0100" },
	{ "CET", "+0100" },
	{ "MEZ", "+0100" },
	{ "EET", "+0200" },
	{ "MSK", "+0300" },
	{ "MSD", "+0400" },
	{ "IRT", "+0330" },
	{ "IST", "+0530" },
	{ "ICT", "+0700" },
	{ "JST", "+0900" },
	{ "NFT", "+1130" },
	{ "UT", "+00" },
	{ "PT", "-0800" },
	{ "BT", "+0300" },
	{ "Z", "+00" },
	{ "A", "-0100" },
	{ "M", "-1200" },
	{ "N", "+0100" },
	{ "Y", "+1200" }
};

/** date_parse_rfc822_tz:
 * @token: String representing the timezone.
 *
 * Returns: (transfer full): a GTimeZone to be freed by g_time_zone_unref
 */
static GTimeZone *
date_parse_rfc822_tz (char *token)
{
	const char *inptr = token;
	int num_timezones = sizeof (tz_offsets) / sizeof ((tz_offsets)[0]);

	if (*inptr == '+' || *inptr == '-') {
		return g_time_zone_new (inptr);
	} else {
		int t;

		if (*inptr == '(')
			inptr++;

		for (t = 0; t < num_timezones; t++)
			if (!strncmp (inptr, tz_offsets[t].name, strlen (tz_offsets[t].name)))
				return g_time_zone_new (tz_offsets[t].offset);
	}
	
	return g_time_zone_new_utc ();
}

static const gchar * rfc822_months[] = { "Jan", "Feb", "Mar", "Apr", "May",
	     "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

GDateMonth
date_parse_month (const gchar *str)
{
	int i;
	for (i = 0;i < 12;i++) {
		if (!g_ascii_strncasecmp (str, rfc822_months[i], 3))
			return i + 1;
	}
	return 0;
}

gint64
date_parse_RFC822 (const gchar *date)
{
	guint64 	day, month, year, hour, minute, second = 0;
	GTimeZone	*tz = NULL;
	GDateTime 	*datetime = NULL;
	gint64		t = 0;
	gchar		*pos, *next, *ascii_date = NULL;

	/* we expect at least something like "03 Dec 12 01:38:34" 
	   and don't require a day of week or the timezone

	   the most specific format we expect:  "Fri, 03 Dec 12 01:38:34 CET"
	 */
	
	/* skip day of week */
	pos = g_utf8_strchr(date, -1, ',');
	if (pos)
		date = ++pos;

	ascii_date = g_str_to_ascii (date, "C");

	/* Parsing day */
	day = g_ascii_strtoull (ascii_date, &next, 10);
	if ((*next == '\0') || (next == ascii_date))
		goto parsing_failed;
	pos = next;

	/* Parsing month */
	while (pos && *pos != '\0' && isspace ((int)*pos))       /* skip whitespaces before month */
		pos++;
	if (strlen (pos) < 3)
		goto parsing_failed;
	month = date_parse_month (pos);
	pos += 3;

	/* Parsing year */
	year = g_ascii_strtoull (pos, &next, 10);
	if ((*next == '\0') || (next == pos))
		goto parsing_failed;
	if (year < 100) {
		/* If year is 2 digits, years after 68 are in 20th century (strptime convention) */
		if (year > 68)
			year += 1900;
		else
			year += 2000;
	}
	pos = next;

	/* Parsing hour */
	hour = g_ascii_strtoull (pos, &next, 10);
	if ((next == pos) || (*next != ':'))
		goto parsing_failed;
	pos = next + 1;

	/* Parsing minute */
	minute = g_ascii_strtoull (pos, &next, 10);
	if (next == pos)
		goto parsing_failed;

	/* Optional second */
	if (*next == ':') {
		pos = next + 1;
		second = g_ascii_strtoull (pos, &next, 10);
		if (next == pos)
			goto parsing_failed;
	}
	pos = next;

	/* Optional Timezone */
	while (pos && *pos != '\0' && isspace ((int)*pos))       /* skip whitespaces before timezone */
		pos++;
	if (*pos != '\0')
		tz = date_parse_rfc822_tz (pos);

	if (!tz)
		datetime = g_date_time_new_utc (year, month, day, hour, minute, second);
	else {
		datetime = g_date_time_new (tz, year, month, day, hour, minute, second);
		g_time_zone_unref (tz);
	}

	if (datetime) {
		t = g_date_time_to_unix (datetime);
		g_date_time_unref (datetime);
	}
	
parsing_failed:
	if (!t)
		debug0 (DEBUG_PARSING, "Invalid RFC822 date !");
	g_free (ascii_date);
	return t;
}
