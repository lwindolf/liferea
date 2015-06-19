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

#define _XOPEN_SOURCE	600 /* glibc2 needs this (man strptime) */

#include "date.h"

#include <config.h>
#include <locale.h>
#include <ctype.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "e-date.h"

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

time_t
date_parse_ISO8601 (const gchar *date)
{
	struct tm	tm;
	time_t		t, t2, offset = 0;
	gboolean	success = FALSE;
	gchar *pos;
	
	g_assert (date != NULL);
	
	memset (&tm, 0, sizeof (struct tm));
	
	/* we expect at least something like "2003-08-07T15:28:19" and
	   don't require the second fractions and the timezone info

	   the most specific format:   YYYY-MM-DDThh:mm:ss.sTZD
	 */
	 
	/* full specified variant */
	pos = strptime (date, "%t%Y-%m-%dT%H:%M%t", &tm);
	if (pos) {
		/* Parse seconds */
		if (*pos == ':')
			pos++;
		if (isdigit (pos[0]) && !isdigit (pos[1])) {
			tm.tm_sec = pos[0] - '0';
			pos++;
		} else if (isdigit (pos[0]) && isdigit (pos[1])) {
			tm.tm_sec = 10*(pos[0]-'0') + pos[1] - '0';
			pos +=2;
		}
		/* Parse second fractions */
		if (*pos == '.') {
			while (*pos == '.' || isdigit (pos[0]))
				pos++;
		}
		/* Parse timezone */
		if (*pos == 'Z')
			offset = 0;
		else if ((*pos == '+' || *pos == '-') && isdigit (pos[1]) && isdigit (pos[2]) && strlen (pos) >= 3) {
			offset = (10*(pos[1] - '0') + (pos[2] - '0')) * 60 * 60;
			
			if (pos[3] == ':' && isdigit (pos[4]) && isdigit (pos[5]))
				offset +=  (10*(pos[4] - '0') + (pos[5] - '0')) * 60;
			else if (isdigit (pos[3]) && isdigit (pos[4]))
				offset +=  (10*(pos[3] - '0') + (pos[4] - '0')) * 60;
			
			offset *= (pos[0] == '+') ? 1 : -1;

		}
		success = TRUE;
	/* only date */
	} else if (NULL != strptime (date, "%t%Y-%m-%d", &tm)) {
		success = TRUE;
	}
	/* there were others combinations too... */

	if (success) {
		if ((time_t)(-1) != (t = mktime (&tm))) {
			/* Correct for the local timezone*/
			struct tm tmp_tm;
			
			t = t - offset;
			gmtime_r (&t, &tmp_tm);
			t2 = mktime (&tmp_tm);
			t = t - (t2 - t);
			
			return t;
		} else {
			debug0 (DEBUG_PARSING, "Internal error! time conversion error! mktime failed!");
		}
	} else {
		debug0 (DEBUG_PARSING, "Invalid ISO8601 date format! Ignoring <dc:date> information!");
	}
	
	return 0;
}

/* in theory, we'd need only the RFC822 timezones here
   in practice, feeds also use other timezones...        */
static struct {
	const char *name;
	int offset;
} tz_offsets [] = {
	{ "IDLW", -1200 },
	{ "HAST", -1000 },
	{ "AKST", -900 },
	{ "AKDT", -800 },
	{ "WESZ", 100 },
	{ "WEST", 100 },
	{ "WEDT", 100 },
	{ "MEST", 200 },
	{ "MESZ", 200 },
	{ "CEST", 200 },
	{ "CEDT", 200 },
	{ "EEST", 300 },
	{ "EEDT", 300 },
	{ "IRST", 430 },
	{ "CNST", 800 },
	{ "ACST", 930 },
	{ "ACDT", 1030 },
	{ "AEST", 1000 },
	{ "AEDT", 1100 },
	{ "IDLE", 1200 },
	{ "NZST", 1200 },
	{ "NZDT", 1300 },
	{ "GMT", 0 },
	{ "EST", -500 },
	{ "EDT", -400 },
	{ "CST", -600 },
	{ "CDT", -500 },
	{ "MST", -700 },
	{ "MDT", -600 },
	{ "PST", -800 },
	{ "PDT", -700 },
	{ "HDT", -900 },
	{ "YST", -900 },
	{ "YDT", -800 },
	{ "AST", -400 },
	{ "ADT", -300 },
	{ "VST", -430 },
	{ "NST", -330 },
	{ "NDT", -230 },
	{ "WET", 0 },
	{ "WEZ", 0 },
	{ "IST", 100 },
	{ "CET", 100 },
	{ "MEZ", 100 },
	{ "EET", 200 },
	{ "MSK", 300 },
	{ "MSD", 400 },
	{ "IRT", 330 },
	{ "IST", 530 },
	{ "ICT", 700 },
	{ "JST", 900 },
	{ "NFT", 1130 },
	{ "UT", 0 },
	{ "PT", -800 },
	{ "BT", 300 },
	{ "Z", 0 },
	{ "A", -100 },
	{ "M", -1200 },
	{ "N", 100 },
	{ "Y", 1200 }
};

/** @returns timezone offset in seconds */
static time_t
date_parse_rfc822_tz (char *token)
{
	int offset = 0;
	const char *inptr = token;
	int num_timezones = sizeof (tz_offsets) / sizeof ((tz_offsets)[0]);

	if (*inptr == '+' || *inptr == '-') {
		offset = atoi (inptr);
	} else {
		int t;

		if (*inptr == '(')
			inptr++;

		for (t = 0; t < num_timezones; t++)
			if (!strncmp (inptr, tz_offsets[t].name, strlen (tz_offsets[t].name))) {
				offset = tz_offsets[t].offset;
				break;
			}
	}
	
	return 60 * ((offset / 100) * 60 + (offset % 100));
}

time_t
date_parse_RFC822 (const gchar *date)
{
	struct tm	tm;
	time_t		t, t2;
	char 		*oldlocale;
	char		*pos;
	gboolean	success = FALSE;

	memset (&tm, 0, sizeof (struct tm));

	/* we expect at least something like "03 Dec 12 01:38:34" 
	   and don't require a day of week or the timezone

	   the most specific format we expect:  "Fri, 03 Dec 12 01:38:34 CET"
	 */
	
	/* skip day of week */
	pos = g_utf8_strchr(date, -1, ',');
	if (pos)
		date = ++pos;

	/* we expect English month names, so we set the locale */
	oldlocale = g_strdup (setlocale (LC_TIME, NULL));
	setlocale (LC_TIME, "C");
	
	/* standard format with seconds and 4 digit year */
	if (NULL != (pos = strptime ((const char *)date, " %d %b %Y %T", &tm)) && tm.tm_year > 0)
		success = TRUE;
	/* non-standard format without seconds and 4 digit year */
	else if (NULL != (pos = strptime ((const char *)date, " %d %b %Y %H:%M", &tm)) && tm.tm_year > 0)
		success = TRUE;
	/* non-standard format with seconds and 2 digit year */
	else if (NULL != (pos = strptime ((const char *)date, " %d %b %y %T", &tm)))
		success = TRUE;
	/* non-standard format without seconds 2 digit year */
	else if (NULL != (pos = strptime ((const char *)date, " %d %b %y %H:%M", &tm)))
		success = TRUE;
	
	while (pos && *pos != '\0' && isspace ((int)*pos))       /* skip whitespaces before timezone */
		pos++;
	
	if (oldlocale) {
		setlocale (LC_TIME, oldlocale);	/* and reset it again */
		g_free (oldlocale);
	}
	
	if (success) {
		if ((time_t)(-1) != (t = mktime (&tm))) {
			/* GMT time, with no daylight savings time
			   correction. (Usually, there is no daylight savings
			   time since the input is GMT.) */
			t = t - date_parse_rfc822_tz (pos);
			t2 = mktime (gmtime(&t));
			t = t - (t2 - t);
			return t;
		} else {
			debug0 (DEBUG_PARSING, "internal error! time conversion error! mktime failed!");
		}
	}
	
	return 0;
}

