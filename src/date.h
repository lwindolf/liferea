/**
 * @file date.h date formatting and parsing routines for Liferea
 *
 * Copyright (C) 2008-2009 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _DATE_H
#define _DATE_H

#include <glib.h>

/**
 * Generic date formatting function. Uses either the 
 * user defined format string, or (if date_format is NULL)
 * a formatted date string whose format string depends
 * on the time difference to today.
 *
 * @param t		the timestamp
 * @param date_format	NULL or a strptime format string (encoded in UTF-8)
 *
 * @returns a newly allocated formatted date string (encoded in UTF-8)
 */
gchar * date_format (time_t date, const gchar *date_format);

/**
 * Parses a ISO8601 date.
 *
 * @param date		the date string to parse
 *
 * @returns timestamp
 */
time_t date_parse_ISO8601 (const gchar *date);

/**
 * Parses a RFC822 format date. This FAILS if a timezone string is
 * specified such as EDT or EST and that timezone is in daylight
 * savings time.
 *
 * @param date		the date string to parse
 *
 * @returns timestamp (GMT, no daylight savings time)
 */
time_t date_parse_RFC822 (const gchar *date);


#endif
