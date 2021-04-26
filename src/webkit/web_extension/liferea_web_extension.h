/**
 * @file liferea_web_extension.h  Control WebKit2 via DBUS from Liferea
 *
 * Copyright (C) 2016 Leiaz <leiaz@mailbox.org>
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

#define LIFEREA_WEB_EXTENSION(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), LIFEREA_TYPE_WEB_EXTENSION, LifereaWebExtension))
#define IS_LIFEREA_WEB_EXTENSION(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), LIFEREA_TYPE_WEB_EXTENSION))
#define LIFEREA_WEB_EXTENSION_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), LIFEREA_TYPE_WEB_EXTENSION, LifereaWebExtensionClass))
#define IS_LIFEREA_WEB_EXTENSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), LIFEREA_TYPE_WEB_EXTENSION))
#define LIFEREA_WEB_EXTENSION_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), LIFEREA_TYPE_WEB_EXTENSION, LifereaWebExtensionClass))

typedef struct _LifereaWebExtension LifereaWebExtension;

typedef struct _LifereaWebExtensionClass LifereaWebExtensionClass;

GType liferea_web_extension_get_type (void);

LifereaWebExtension* liferea_web_extension_get (void);
void liferea_web_extension_initialize (LifereaWebExtension *extension, WebKitWebExtension *webkit_extension,  const gchar *server_address);

#endif
