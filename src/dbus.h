/**
 * @file dbus.c DBUS interface to control Liferea
 * 
 * Copyright (C) 2007 mooonz <mooonz@users.sourceforge.net>
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

#ifndef __LIFEREA_DBUS_H__
#define __LIFEREA_DBUS_H__

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#ifdef USE_DBUS	
#include <dbus.h>
#include <dbus/dbus-glib.h>
#endif

#define LF_DBUS_PATH "/org/gnome/feed/Reader"
#define LF_DBUS_SERVICE "org.gnome.feed.Reader"

typedef struct _LifereaDBus {
	GObject parent;
} LifereaDBus;

typedef struct _LifereaDBusClass {
	GObjectClass parent;
} LifereaDBusClass;

GType liferea_dbus_get_type();

#define LIFEREA_DBUS_TYPE              (liferea_dbus_get_type ())
#define LIFEREA_DBUS(object)           (G_TYPE_CHECK_INSTANCE_CAST ((object), LIFEREA_DBUS_TYPE, LifereaDBus))
#define LIFEREA_DBUS_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_DBUS_TYPE, LifereaDBusClass))
#define IS_LIFEREA_DBUS(object)        (G_TYPE_CHECK_INSTANCE_TYPE ((object), LIFEREA_DBUS_TYPE))
#define IS_LIFEREA_DBUS_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_DBUS_TYPE))
#define LIFEREA_DBUS_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), LIFEREA_DBUS_TYPE, LifereaDBusClass))

LifereaDBus* liferea_dbus_new();

#endif
