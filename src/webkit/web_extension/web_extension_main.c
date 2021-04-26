/**
 * @file web_extension_main.c  Control WebKit2 via DBUS from Liferea
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

#include <webkit2/webkit-web-extension.h>

#include "liferea_web_extension.h"

static LifereaWebExtension *extension = NULL;

G_MODULE_EXPORT void
webkit_web_extension_initialize_with_user_data (WebKitWebExtension *webkit_extension,
						GVariant *userdata)
{
	extension = liferea_web_extension_get ();
	liferea_web_extension_initialize (extension, webkit_extension, g_variant_get_string (userdata, NULL));
}

static void __attribute__((destructor))
web_extension_shutdown (void)
{
	if (extension)
		g_object_unref (extension);
}
