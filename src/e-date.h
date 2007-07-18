/* The e_date.c functionality is originally from Evolution. */

/*
 * e-util.h
 * Copyright 2000, 2001, Ximian, Inc.
 *
 * Authors:
 *   Chris Lahey <clahey@ximian.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License, version 2, as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 * 02111-1307, USA.
 */

#ifndef _E_UTIL_H_
#define _E_UTIL_H_

size_t   e_utf8_strftime_fix_am_pm  (char             *s,
				     size_t            max,
				     const char       *fmt,
				     const struct tm  *tm);

size_t   e_utf8_strftime	(char              *s,
				 size_t             max,
				 const char        *fmt,
				 const struct tm   *tm);

#endif
