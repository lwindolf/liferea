/**
 * @file liferea_web_extension.c  Control WebKit2 via DBUS from Liferea
 *
 *   Copyright (C) 2016 Leiaz <leiaz@mailbox.org>
 *   Copyright (C) 2024 Lars Windolf <lars.windolf@gmx.de>
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
#include <JavaScriptCore/JavaScript.h>

#include "liferea_web_extension.h"
#include "liferea_web_extension_names.h"

struct _LifereaWebExtension {
	GObject 		parent;

	GDBusConnection 	*connection;
	WebKitWebExtension 	*webkit_extension;
	GArray 			*pending_pages_created;
	gboolean 		initialized;

	GSettings 		*liferea_settings;
};

struct _LifereaWebExtensionClass {
	GObjectClass parent_class;
};

G_DEFINE_TYPE (LifereaWebExtension, liferea_web_extension, G_TYPE_OBJECT)

static const char introspection_xml[] =
  "<node>"
  " <interface name='net.sf.liferea.WebExtension'>"
  "  <method name='EvalJs'>"
  "   <arg type='t' name='page_id' direction='in'/>"
  "   <arg type='s' name='js' direction='in'/>"
  "   <arg type='s' name='result' direction='out'/>"
  "  </method>"
  "  <method name='EvalJsNoResult'>"
  "   <arg type='t' name='page_id' direction='in'/>"
  "   <arg type='s' name='js' direction='in'/>"
  "  </method>"
  "  <signal name='PageCreated'>"
  "   <arg type='t' name='page_id' direction='out'/>"
  "  </signal>"
  " </interface>"
  "</node>";

static void
liferea_web_extension_dispose (GObject *object)
{
	LifereaWebExtension *extension = LIFEREA_WEB_EXTENSION (object);

	g_clear_object (&extension->connection);
	g_clear_object (&extension->webkit_extension);
	g_clear_object (&extension->liferea_settings);
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
	self->webkit_extension = NULL;
	self->connection = NULL;
	self->pending_pages_created = NULL;
	self->initialized = FALSE;
	self->liferea_settings = g_settings_new ("net.sf.liferea");
}

static gboolean
on_authorize_authenticated_peer (GDBusAuthObserver 	*observer,
				 GIOStream		*stream,
				 GCredentials		*credentials,
				 gpointer		extension)
{
	gboolean authorized = FALSE;
	GCredentials *own_credentials = NULL;
	GError *error = NULL;

	if (!credentials) {
		g_warning ("No credentials received from Liferea.\n");
		return FALSE;
	}

	own_credentials = g_credentials_new ();

	if (g_credentials_is_same_user (credentials, own_credentials, &error)) {
		authorized = TRUE;
	} else {
		g_warning ("Error authorizing connection to Liferea : %s\n", error->message);
		g_error_free (error);
	}
	g_object_unref (own_credentials);

	return authorized;
}

static void
handle_dbus_method_call (GDBusConnection 	*connection,
			 const gchar 		*sender,
			 const gchar 		*object_path,
			 const gchar 		*interface_name,
			 const gchar 		*method_name,
			 GVariant 		*parameters,
			 GDBusMethodInvocation 	*invocation,
			 gpointer 		user_data)
{
	guint64		page_id;
	WebKitWebPage	*page;

	// We only implement a generic callbacks 'EvalJs' and 'EvalJsNoResult' which should 
	// be used to run all everything we might need through Javascript...

	// EvalJs handling inspired by https://github.com/fanglingsu/vimb
	if (g_strcmp0 (method_name, "EvalJs") == 0) {
		gchar *script;
		JSCValue *result = NULL;
		JSCContext *jsContext;

		g_variant_get(parameters, "(ts)", &page_id, &script);

		page = webkit_web_extension_get_page(LIFEREA_WEB_EXTENSION (user_data)->webkit_extension, page_id);
		if (!page) {
        		g_warning ("invalid page id %lu", page_id);
        		g_dbus_method_invocation_return_error(invocation, G_DBUS_ERROR,
                	        G_DBUS_ERROR_INVALID_ARGS, "Invalid page ID: %"G_GUINT64_FORMAT, page_id);
			return;
    		}

		jsContext = webkit_frame_get_js_context_for_script_world (
			webkit_web_page_get_main_frame (page),
			webkit_script_world_get_default ()
		);

		result = jsc_context_evaluate (jsContext, script, -1);
		if (!g_strcmp0 (method_name, "EvalJsNoResult")) {
			g_dbus_method_invocation_return_value(invocation, NULL);
		} else {
			g_autofree gchar *str = jsc_value_to_string (result);
			g_dbus_method_invocation_return_value(invocation, g_variant_new("(s)", str));
		}
		g_object_unref (result);
	}
}

static const GDBusInterfaceVTable interface_vtable = {
	handle_dbus_method_call,
	NULL,
	NULL
};

static void
liferea_web_extension_emit_page_created (LifereaWebExtension *extension, guint64 page_id)
{
	g_dbus_connection_emit_signal (
		extension->connection,
		NULL,
		LIFEREA_WEB_EXTENSION_OBJECT_PATH,
		LIFEREA_WEB_EXTENSION_INTERFACE_NAME,
		"PageCreated",
		g_variant_new ("(t)", page_id),
		NULL);
}

static void
liferea_web_extension_queue_page_created (LifereaWebExtension *extension, guint64 page_id)
{
	if (!extension->pending_pages_created) {
		extension->pending_pages_created = g_array_new (FALSE, FALSE, sizeof (guint64));
	}

	g_array_append_val (extension->pending_pages_created, page_id);
}

static void
liferea_web_extension_emit_pending_pages_created (LifereaWebExtension *extension)
{
	guint i;

	if (!extension->pending_pages_created)
		return;

	for (i = 0;i<extension->pending_pages_created->len;++i) {
		guint64 page_id = g_array_index (extension->pending_pages_created, guint64, i);
		liferea_web_extension_emit_page_created (extension, page_id);
	}
	g_array_free (extension->pending_pages_created, TRUE);
	extension->pending_pages_created = NULL;
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
on_page_created (WebKitWebExtension *webkit_extension,
		 WebKitWebPage      *web_page,
		 gpointer            extension)
{
	guint64 page_id;

	g_signal_connect (
		web_page,
		"send-request",
		G_CALLBACK (on_send_request),
		extension
	);

	page_id = webkit_web_page_get_id (web_page);
	if (LIFEREA_WEB_EXTENSION (extension)->connection) {
		liferea_web_extension_emit_page_created (LIFEREA_WEB_EXTENSION (extension), page_id);
	} else {
		liferea_web_extension_queue_page_created (LIFEREA_WEB_EXTENSION (extension), page_id);
	}
}

static void
on_dbus_connection_created (GObject 		*source_object,
			    GAsyncResult 	*result,
			    gpointer	 	user_data)
{
	GDBusNodeInfo *introspection_data = NULL;
	GDBusConnection *connection = NULL;
	guint registration_id = 0;
	GError *error = NULL;
	LifereaWebExtension *extension = LIFEREA_WEB_EXTENSION (user_data);

	introspection_data = g_dbus_node_info_new_for_xml (introspection_xml, NULL);

	connection = g_dbus_connection_new_for_address_finish (result, &error);
	if (error) {
		g_warning ("Extension failed to connect : %s", error->message);
		g_error_free (error);
		return;
	}

	registration_id = g_dbus_connection_register_object (connection,
		LIFEREA_WEB_EXTENSION_OBJECT_PATH,
		introspection_data->interfaces[0],
		&interface_vtable,
		extension,
		NULL,
		&error);

	g_dbus_node_info_unref (introspection_data);
	if (!registration_id) {
		g_warning ("Failed to register web extension object: %s\n", error->message);
		g_error_free (error);
		g_object_unref (connection);
		return;
	}

	extension->connection = connection;
	liferea_web_extension_emit_pending_pages_created (extension);
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
				  WebKitWebExtension 	*webkit_extension,
				  const gchar 		*server_address)
{

	if (extension->initialized)
		return;

	g_signal_connect (
		webkit_extension,
		"page-created",
		G_CALLBACK (on_page_created),
		extension);

	GDBusAuthObserver	*observer;

	extension->initialized = TRUE;
	extension->webkit_extension = g_object_ref (webkit_extension);

	observer = g_dbus_auth_observer_new ();

	g_signal_connect (
		observer,
		"authorize-authenticated-peer",
		G_CALLBACK (on_authorize_authenticated_peer),
		extension);

	g_dbus_connection_new_for_address (
		server_address,
		G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT,
		observer,
		NULL,
		(GAsyncReadyCallback)on_dbus_connection_created,
		extension);

	g_object_unref (observer);
}
