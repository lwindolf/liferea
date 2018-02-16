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

#if defined (G_OS_WIN32) && !defined (HAVE_GMTIME_R)
#define gmtime_r(t,o) gmtime_s (o,t)
#endif
#if defined (G_OS_WIN32) && !defined (HAVE_LOCALTIME_R)
#define localtime_r(t,o) localtime_s (o,t)
#endif

#if defined (G_OS_WIN32) && !defined (HAVE_STRPTIME)
/**
 * @file src/strptime.c
 * @brief This is a slightly modified version by the "R Project"
 * 				<http://www.r-project.org/> with locale support removed.
 */

/* XXX This version of the implementation is not really complete.
   Some of the fields cannot add information alone.  But if seeing
   some of them in the same format (such as year, week and weekday)
   this is enough information for determining the date.  */

#include <windows.h>
#include <ctype.h>
#include <limits.h>
#include <string.h>
#include <time.h>

#define match_char(ch1, ch2) if (ch1 != ch2) return NULL

#ifndef Macintosh
#if defined __GNUC__ && __GNUC__ >= 2
# define match_string(cs1, s2) \
  ({ size_t len = strlen (cs1);						      \
     int result = strncasecmp ((cs1), (s2), len) == 0;			      \
     if (result) (s2) += len;						      \
     result; })
#else
/* Oh come on.  Get a reasonable compiler.  */
# define match_string(cs1, s2) \
  (strncasecmp ((cs1), (s2), strlen (cs1)) ? 0 : ((s2) += strlen (cs1), 1))
#endif
#else
# define match_string(cs1, s2) \
  (strncmp ((cs1), (s2), strlen (cs1)) ? 0 : ((s2) += strlen (cs1), 1))
#endif /* mac */

/* We intentionally do not use isdigit() for testing because this will
   lead to problems with the wide character version.  */
#define get_number(from, to, n) \
  do {									      \
    int __n = n;							      \
    val = 0;								      \
    while (*rp == ' ')							      \
      ++rp;								      \
    if (*rp < '0' || *rp > '9')						      \
      return NULL;							      \
    do {								      \
      val *= 10;							      \
      val += *rp++ - '0';						      \
    } while (--__n > 0 && val * 10 <= to && *rp >= '0' && *rp <= '9');	      \
    if (val < from || val > to)						      \
      return NULL;							      \
  } while (0)
# define get_alt_number(from, to, n) \
  /* We don't have the alternate representation.  */			      \
  get_number(from, to, n)
#define recursive(new_fmt) \
  (*(new_fmt) != '\0'							      \
   && (rp = strptime_internal (rp, (new_fmt), tm, decided)) != NULL)

/* This version: may overwrite these with versions for the locale */
static char weekday_name[][20] =
{
    "Sunday", "Monday", "Tuesday", "Wednesday",
    "Thursday", "Friday", "Saturday"
};
static char ab_weekday_name[][10] =
{
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};
static char month_name[][20] =
{
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
};
static char ab_month_name[][10] =
{
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char am_pm[][4] = {"AM", "PM"};

static int have_used_strptime = 0;

static const unsigned short int __mon_yday[2][13] =
{
    /* Normal years.  */
    { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334, 365 },
    /* Leap years.  */
    { 0, 31, 60, 91, 121, 152, 182, 213, 244, 274, 305, 335, 366 }
};


/* Status of lookup: do we use the locale data or the raw data?  */
enum locale_status { not, loc, raw };

# define __isleap(year)	\
  ((year) % 4 == 0 && ((year) % 100 != 0 || (year) % 400 == 0))

/* Convert Windows date picture to POSIX style. The converted format string may not mach exactly. */
void conv_winpic(char *win, char **posix)
{
  char *src, *dst;
  
  src = win;
  *posix = dst = malloc(strlen(src) * 2 + 1);
  while(*src)
  {
    if (src[0] == 'y')
      if (src[1] == 'y')
        if (src[2] == 'y')
          if (src[3] == 'y')
          {
            strcpy(dst, "%Y");

            if (src[4] == 'y')
              src += 5; // yyyyy
            else
              src += 4; // yyyy
            
            dst += 2;
          }
          else // yyy
          {
            // undefined
            strcpy(dst, "yyy");
            dst += 3;
            src += 3;
          }
        else // yy
        {
          strcpy(dst, "%y");
          dst += 2;
          src += 2;
        }
      else // y
      {
        // impossible to convert properly
        strcpy(dst, "%y");
        dst += 2;
        src++;
      }
    else if (src[0] == 'M')
    {
      if (src[1] == 'M')
        if (src[2] == 'M')
          if (src[3] == 'M') // MMMM
          {
            strcpy(dst, "%b");
            src += 4;
          }
          else // MMM
          {
            // impossible to convert properly
            strcpy(dst, "%m");
            src += 3;
          }
        else // MM
        {
          strcpy(dst, "%m");
          src += 2;        
        }
      else // M
      {
        strcpy(dst, "%m");
        src += 1;
      }
      
      dst += 2;
    }
    else if (src[0] == 'd')
    {
      if (src[1] == 'd')
        if (src[2] == 'd')
          if (src[3] == 'd') // dddd
          {
            strcpy(dst, "%a");
            src += 4;
          }
          else // ddd
          {
            // impossible to convert properly
            strcpy(dst, "%d");
            src += 3;
          }
        else // dd
        {
          strcpy(dst, "%d");
          src += 2;
        }
      else // d
      {
        strcpy(dst, "%d");
        src++;
      }
      
      dst += 2;
    }
    else if (src[0] == 'g' && src[1] == 'g')
      // impossible to convert
      src += 2;
    else if (src[0] == 'h')
    {
      strcpy(dst, "%I");
      dst += 2;
      if (src[1] == 'h')
        src += 2;
      else
        src += 1;
    }
    else if (src[0] == 'H')
    {
      strcpy(dst, "%H");
      dst += 2;
      if (src[1] == 'H')
        src += 2;
      else
        src += 1;
    }
    else if (src[0] == 'm')
    {
      strcpy(dst, "%M");
      dst += 2;
      if (src[1] == 'm')
        src += 2;
      else
        src += 1;
    }
    else if (src[0] == 's')
    {
      strcpy(dst, "%S");
      dst += 2;
      if (src[1] == 's')
        src += 2;
      else
        src += 1;
    }
    else if (src[0] == 't')
    {
      strcpy(dst, "%p");
      dst += 2;
      if (src[1] == 't')
        src += 2;
      else
        src += 1;
    }
    else
    {
      *dst = *src;
      dst++;
      src++;
    }    
  }
  
  dst[0] = 0;
}

/* Compute the day of the week.  */
void
day_of_the_week (struct tm *tm)
{
    /* We know that January 1st 1970 was a Thursday (= 4).  Compute the
       the difference between this data in the one on TM and so determine
       the weekday.  */
    int corr_year = 1900 + tm->tm_year - (tm->tm_mon < 2);
    int wday = (-473
		+ (365 * (tm->tm_year - 70))
		+ (corr_year / 4)
		- ((corr_year / 4) / 25) + ((corr_year / 4) % 25 < 0)
		+ (((corr_year / 4) / 25) / 4)
		+ __mon_yday[0][tm->tm_mon]
		+ tm->tm_mday - 1);
    tm->tm_wday = ((wday % 7) + 7) % 7;
}

/* Compute the day of the year.  */
void
day_of_the_year (struct tm *tm)
{
    tm->tm_yday = (__mon_yday[__isleap (1900 + tm->tm_year)][tm->tm_mon]
		   + (tm->tm_mday - 1));
}

char *
strptime_internal (const char *rp, const char *fmt, struct tm *tm,
		   enum locale_status *decided)
{
    int cnt;
    ssize_t val;
    int have_I, is_pm;
    int century, want_century;
    int have_wday, want_xday;
    int have_yday;
    int have_mon, have_mday;

    have_I = is_pm = 0;
    century = -1;
    want_century = 0;
    have_wday = want_xday = have_yday = have_mon = have_mday = 0;

    while (*fmt != '\0')
    {
	/* A white space in the format string matches 0 more or white
	   space in the input string.  */
	if (isspace (*fmt))
	{
	    while (isspace (*rp))
		++rp;
	    ++fmt;
	    continue;
	}

	/* Any character but `%' must be matched by the same character
	   in the iput string.  */
	if (*fmt != '%')
	{
	    match_char (*fmt++, *rp++);
	    continue;
	}

	++fmt;

	/* We need this for handling the `E' modifier.  */
    start_over:

	switch (*fmt++)
	{
	case '%':
	    /* Match the `%' character itself.  */
	    match_char ('%', *rp++);
	    break;
	case 'a':
	case 'A':
	    /* Match day of week.  */
	    for (cnt = 0; cnt < 7; ++cnt)
	    {
		if (*decided != loc
		    && (match_string (weekday_name[cnt], rp)
			|| match_string (ab_weekday_name[cnt], rp)))
		{
		    *decided = raw;
		    break;
		}
	    }
	    if (cnt == 7)
		/* Does not match a weekday name.  */
		return NULL;
	    tm->tm_wday = cnt;
	    have_wday = 1;
	    break;
	case 'b':
	case 'B':
	case 'h':
	    /* Match month name.  */
	    for (cnt = 0; cnt < 12; ++cnt)
	    {
		if (match_string (month_name[cnt], rp)
		    || match_string (ab_month_name[cnt], rp))
		{
		    *decided = raw;
		    break;
		}
	    }
	    if (cnt == 12)
		/* Does not match a month name.  */
		return NULL;
	    tm->tm_mon = cnt;
	    want_xday = 1;
	    break;
	case 'c':
	    /* Match locale's date and time format.  */
      return strptime_internal (rp, "%x %X", tm, decided);
    break;
	case 'C':
	  /* Match century number.  */
	  get_number (0, 99, 2);
	  century = val;
	  want_xday = 1;
	  break;
	case 'd':
	case 'e':
	  /* Match day of month.  */
	  get_number (1, 31, 2);
	  tm->tm_mday = val;
	  have_mday = 1;
	  want_xday = 1;
	  break;
	case 'F':
	  if (!recursive ("%Y-%m-%d"))
	    return NULL;
	  want_xday = 1;
	  break;
	case 'x':
    {
      char *pic;
      unsigned int loc;
      char winpic[100];
      int ret;
      
      loc = GetThreadLocale();
      GetLocaleInfo(loc, LOCALE_SSHORTDATE, winpic, 100);
      
      conv_winpic(winpic, &pic);
      ret = recursive(pic);
      free(pic);
      if (!ret)
        return NULL;
      want_xday = 1;
    }
    break;
	case 'D':
	  /* Match standard day format.  */
	  if (!recursive ("%m/%d/%y"))
	    return NULL;
	  want_xday = 1;
	  break;
	case 'k':
	case 'H':
	  /* Match hour in 24-hour clock.  */
	  get_number (0, 23, 2);
	  tm->tm_hour = val;
	  have_I = 0;
	  break;
	case 'I':
	  /* Match hour in 12-hour clock.  */
	  get_number (1, 12, 2);
	  tm->tm_hour = val % 12;
	  have_I = 1;
	  break;
	case 'j':
	  /* Match day number of year.  */
	  get_number (1, 366, 3);
	  tm->tm_yday = val - 1;
	  have_yday = 1;
	  break;
	case 'm':
	  /* Match number of month.  */
	  get_number (1, 12, 2);
	  tm->tm_mon = val - 1;
	  have_mon = 1;
	  want_xday = 1;
	  break;
	case 'M':
	  /* Match minute.  */
	  get_number (0, 59, 2);
	  tm->tm_min = val;
	  break;
	case 'n':
	case 't':
	  /* Match any white space.  */
	  while (isspace (*rp))
	    ++rp;
	  break;
	case 'p':
	  /* Match locale's equivalent of AM/PM.  */
	  if (!match_string (am_pm[0], rp))
          {
	    if (match_string (am_pm[1], rp))
	      is_pm = 1;
	    else
	      return NULL;
          }
	  break;
	case 'r':
    return strptime_internal (rp, "%x %X", tm, decided);
	  break;
	case 'R':
	    if (!recursive ("%H:%M"))
		return NULL;
	    break;
	case 's':
	{
	    /* The number of seconds may be very high so we cannot use
	       the `get_number' macro.  Instead read the number
	       character for character and construct the result while
	       doing this.  */
	    time_t secs = 0;
	    if (*rp < '0' || *rp > '9')
		/* We need at least one digit.  */
		return NULL;

	    do
	    {
		secs *= 10;
		secs += *rp++ - '0';
	    }
	    while (*rp >= '0' && *rp <= '9');

	    if ((tm = localtime (&secs)) == NULL)
		/* Error in function.  */
		return NULL;
	}
	break;
	case 'S':
	    get_number (0, 61, 2);
	    tm->tm_sec = val;
	    break;
	case 'X':
    {
      char *pic;
      unsigned int loc;
      char winpic[100];
      int ret;
      
      loc = GetThreadLocale();
      GetLocaleInfo(loc, LOCALE_STIMEFORMAT, winpic, 100);
      
      conv_winpic(winpic, &pic);
      ret = recursive(pic);
      free(pic);
      if (!ret)
        return NULL;
    }
    break;
	case 'T':
      return strptime_internal (rp, "%H:%M:%S", tm, decided);
	    break;
	case 'u':
	    get_number (1, 7, 1);
	    tm->tm_wday = val % 7;
	    have_wday = 1;
	    break;
	case 'g':
	    get_number (0, 99, 2);
	    /* XXX This cannot determine any field in TM.  */
	    break;
	case 'G':
	    if (*rp < '0' || *rp > '9')
		return NULL;
	    /* XXX Ignore the number since we would need some more
	       information to compute a real date.  */
	    do
		++rp;
	    while (*rp >= '0' && *rp <= '9');
	    break;
	case 'U':
	case 'V':
	case 'W':
	    get_number (0, 53, 2);
	    /* XXX This cannot determine any field in TM without some
	       information.  */
	    break;
	case 'w':
	    /* Match number of weekday.  */
	    get_number (0, 6, 1);
	    tm->tm_wday = val;
	    have_wday = 1;
	    break;
	case 'y':
	    /* Match year within century.  */
	    get_number (0, 99, 2);
	    /* The "Year 2000: The Millennium Rollover" paper suggests that
	       values in the range 69-99 refer to the twentieth century.  */
	    tm->tm_year = val >= 69 ? val : val + 100;
	    /* Indicate that we want to use the century, if specified.  */
	    want_century = 1;
	    want_xday = 1;
	    break;
	case 'Y':
	    /* Match year including century number.  */
	    get_number (0, 9999, 4);
	    tm->tm_year = val - 1900;
	    want_century = 0;
	    want_xday = 1;
	    break;
	case 'Z':
	    /* XXX How to handle this?  */
	    break;
	case 'E':
	    /* We have no information about the era format.  Just use
	       the normal format.  */
	    if (*fmt != 'c' && *fmt != 'C' && *fmt != 'y' && *fmt != 'Y'
		&& *fmt != 'x' && *fmt != 'X')
		/* This is an invalid format.  */
		return NULL;

	    goto start_over;
	case 'O':
	    switch (*fmt++)
	    {
	    case 'd':
	    case 'e':
		/* Match day of month using alternate numeric symbols.  */
		get_alt_number (1, 31, 2);
		tm->tm_mday = val;
		have_mday = 1;
		want_xday = 1;
		break;
	    case 'H':
		/* Match hour in 24-hour clock using alternate numeric
		   symbols.  */
		get_alt_number (0, 23, 2);
		tm->tm_hour = val;
		have_I = 0;
		break;
	    case 'I':
		/* Match hour in 12-hour clock using alternate numeric
		   symbols.  */
		get_alt_number (1, 12, 2);
		tm->tm_hour = val - 1;
		have_I = 1;
		break;
	    case 'm':
		/* Match month using alternate numeric symbols.  */
		get_alt_number (1, 12, 2);
		tm->tm_mon = val - 1;
		have_mon = 1;
		want_xday = 1;
		break;
	    case 'M':
		/* Match minutes using alternate numeric symbols.  */
		get_alt_number (0, 59, 2);
		tm->tm_min = val;
		break;
	    case 'S':
		/* Match seconds using alternate numeric symbols.  */
		get_alt_number (0, 61, 2);
		tm->tm_sec = val;
		break;
	    case 'U':
	    case 'V':
	    case 'W':
		get_alt_number (0, 53, 2);
		/* XXX This cannot determine any field in TM without
		   further information.  */
		break;
	    case 'w':
		/* Match number of weekday using alternate numeric symbols.  */
		get_alt_number (0, 6, 1);
		tm->tm_wday = val;
		have_wday = 1;
		break;
	    case 'y':
		/* Match year within century using alternate numeric symbols.  */
		get_alt_number (0, 99, 2);
		tm->tm_year = val >= 69 ? val : val + 100;
		want_xday = 1;
		break;
	    default:
		return NULL;
	    }
	    break;
	default:
	    return NULL;
	}
    }

    if (have_I && is_pm)
	tm->tm_hour += 12;

    if (century != -1)
    {
	if (want_century)
	    tm->tm_year = tm->tm_year % 100 + (century - 19) * 100;
	else
	    /* Only the century, but not the year.  Strange, but so be it.  */
	    tm->tm_year = (century - 19) * 100;
    }

    if (want_xday && !have_wday) {
	if ( !(have_mon && have_mday) && have_yday)  {
	    /* we don't have tm_mon and/or tm_mday, compute them */
	    int t_mon = 0;
	    while (__mon_yday[__isleap(1900 + tm->tm_year)][t_mon] <= tm->tm_yday)
		t_mon++;
	    if (!have_mon)
		tm->tm_mon = t_mon - 1;
	    if (!have_mday)
		tm->tm_mday = tm->tm_yday - __mon_yday[__isleap(1900 + tm->tm_year)][t_mon - 1] + 1;
	}
	day_of_the_week (tm);
    }
    if (want_xday && !have_yday)
	day_of_the_year (tm);

    return (char *) rp;
}

char *
strptime (const char *buf, const char *format, struct tm *tm)
{
    enum locale_status decided;
    decided = raw;
    return strptime_internal (buf, format, tm, &decided);
}
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
