/**
 * @file liferea_web_extension.c  Control WebKit2 via DBUS from Liferea
 *
 *   Copyright (C) 2016 Leiaz <leiaz@mailbox.org>
 *   Copyright (C) 2024-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "liferea_web_extension.h"

struct _LifereaWebExtension {
	GObject 		parent;

	gboolean 			initialized;

	GSettings 			*liferea_settings;
};

struct _LifereaWebExtensionClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE (LifereaWebExtension, liferea_web_extension, G_TYPE_OBJECT)

static void
liferea_web_extension_dispose (GObject *object)
{
	LifereaWebExtension *extension = LIFEREA_WEB_EXTENSION (object);

	g_clear_object (&extension->liferea_settings);

	G_OBJECT_CLASS(liferea_web_extension_parent_class)->finalize(object);
}

static void
liferea_web_extension_class_init (LifereaWebExtensionClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->dispose = liferea_web_extension_dispose;
}

static void
liferea_web_extension_init (LifereaWebExtension *self)
{
	self->initialized = FALSE;
	self->liferea_settings = g_settings_new ("net.sf.liferea");
}

static gboolean
on_send_request (WebKitWebPage 		*web_page,
		 WebKitURIRequest 	*request,
		 WebKitURIResponse 	*redirected_response,
		 gpointer 		web_extension)
{
	SoupMessageHeaders *headers = webkit_uri_request_get_http_headers (request);

	if (!headers)
		return FALSE;

	if (g_settings_get_boolean (LIFEREA_WEB_EXTENSION (web_extension)->liferea_settings, "do-not-track"))
		soup_message_headers_append (headers, "DNT", "1");

	if (g_settings_get_boolean (LIFEREA_WEB_EXTENSION (web_extension)->liferea_settings, "do-not-sell"))
		soup_message_headers_append (headers, "Sec-GPC", "1");

	return FALSE;
}

static void
on_page_created (WebKitWebProcessExtension *webkit_extension,
		 WebKitWebPage      *web_page,
		 gpointer            extension)
{
	g_signal_connect (
		web_page,
		"send-request",
		G_CALLBACK (on_send_request),
		extension
	);
}

static gpointer
liferea_web_extension_new (gpointer data)
{
	return g_object_new (LIFEREA_TYPE_WEB_EXTENSION, NULL);
}

LifereaWebExtension *
liferea_web_extension_get (void)
{
	static GOnce init_once = G_ONCE_INIT;

	g_once (&init_once, liferea_web_extension_new, NULL);

	return init_once.retval;
}

void
liferea_web_extension_initialize (LifereaWebExtension 	*extension,
				  WebKitWebProcessExtension 	*webkit_extension)
{

	if (extension->initialized)
		return;

	g_signal_connect (
		webkit_extension,
		"page-created",
		G_CALLBACK (on_page_created),
		extension);

	extension->initialized = TRUE;
}
