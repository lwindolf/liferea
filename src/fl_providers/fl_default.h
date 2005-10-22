/**
 * @file fl_default.h default static feedlist provider
 * 
 * Copyright (C) 2005 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _FL_DEFAULT_H
#define _FL_DEFAULT_H

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef USE_DBUS

/* Yes, we know that DBUS API isn't stable yet */
#define DBUS_API_SUBJECT_TO_CHANGE
#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>

#define DBUS_RSS_SERVICE "org.gnome.feed.Reader"
#define DBUS_RSS_OBJECT  "/org/gnome/feed/Reader"
#define DBUS_RSS_METHOD  "Subscribe"

#endif

/* allowing special feed creation (for feedster searches) */
void fl_default_feed_add(const gchar *source, gchar *filter, gint flags);

#endif
