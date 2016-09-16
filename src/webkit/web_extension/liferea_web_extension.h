/**
 * @file liferea_web_extension.h  Control WebKit2 via DBUS from Liferea
 *
 * Copyright (C) 2016 Leiaz <leiaz@free.fr>
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

#ifndef _LIFEREA_WEB_EXTENSION_H
#define _LIFEREA_WEB_EXTENSION_H

#include <glib-object.h>
#include <webkit2/webkit-web-extension.h>

#define LIFEREA_TYPE_WEB_EXTENSION liferea_web_extension_get_type ()

G_DECLARE_FINAL_TYPE (LifereaWebExtension, liferea_web_extension, LIFEREA, WEB_EXTENSION, GObject)

LifereaWebExtension* liferea_web_extension_get (void);
void liferea_web_extension_initialize (LifereaWebExtension *extension, WebKitWebExtension *webkit_extension,  const gchar *server_address);

#endif
