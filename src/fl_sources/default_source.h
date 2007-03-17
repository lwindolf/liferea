/**
 * @file default_source.h default static feedlist provider
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

#ifndef _DEFAULT_SOURCE_H
#define _DEFAULT_SOURCE_H

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
#define DBUS_RSS_INTERFACE "org.gnome.feed.Reader"
#define DBUS_RSS_OBJECT  "/org/gnome/feed/Reader"
#define DBUS_RSS_METHOD  "Subscribe"
#define DBUS_FEED_READER_SET_ONLINE_METHOD  "SetOnline"
#define DBUS_FEED_READER_PING_METHOD  "Ping"
#define DBUS_INTROSPECT_METHOD "Introspect"

#endif /* USE_DBUS */

#include "fl_sources/node_source.h"

/**
 * Returns default source type implementation info.
 */
nodeSourceTypePtr default_source_get_type(void);

#endif /* _DEFAULT_SOURCE_H */
