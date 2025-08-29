/**
 * @file webkit.c  WebKit2GTK 6.0 support for Liferea
 *
 * Copyright (C) 2016-2019 Leiaz <leiaz@mailbox.org>
 * Copyright (C) 2007-2025 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2008 Lars Strojny <lars@strojny.net>
 * Copyright (C) 2009-2012 Emilio Pozuelo Monfort <pochu27@gmail.com>
 * Copyright (C) 2009 Adrian Bunk <bunk@users.sourceforge.net>
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
 
#include "webkit/liferea_webkit.h"

#include <string.h>
#include <math.h>

#include "browser.h"
#include "conf.h"
#include "common.h"
#include "debug.h"
#include "download.h"
#include "feedlist.h"
#include "net.h"
#include "ui/browser_tabs.h"
#include "ui/liferea_browser.h"

#include "web_extension/liferea_web_extension_names.h"
#include "liferea_web_view.h"

G_DEFINE_TYPE (LifereaWebKit, liferea_webkit, G_TYPE_OBJECT)

// singleton
static LifereaWebKit *liferea_webkit = NULL;
static WebKitUserStyleSheet *default_stylesheet = NULL;
static WebKitUserStyleSheet *user_stylesheet = NULL;

enum {
	PAGE_CREATED_SIGNAL,
	N_SIGNALS
};

static guint liferea_webkit_signals [N_SIGNALS];

static void
liferea_webkit_dispose (GObject *gobject)
{
	LifereaWebKit *self = LIFEREA_WEBKIT(gobject);

	g_clear_object (&self->dbus_server);
	g_list_free_full (self->dbus_connections, g_object_unref);

	/* Chaining dispose from parent class. */
	G_OBJECT_CLASS(liferea_webkit_parent_class)->dispose(gobject);
}

static void
liferea_webkit_class_init (LifereaWebKitClass *klass)
{
	GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

	GType signal_params[2] = {G_TYPE_POINTER, G_TYPE_UINT64};

	liferea_webkit_signals[PAGE_CREATED_SIGNAL] = g_signal_newv (
		"page-created",
		LIFEREA_TYPE_WEBKIT,
		G_SIGNAL_RUN_FIRST,
		NULL,
		NULL,
		NULL,
		NULL,
		G_TYPE_NONE,
		2,
		signal_params);

	gobject_class->dispose = liferea_webkit_dispose;
}

/**
 * Update the settings object if the preferences change.
 * This will affect all the webviews as they all use the same
 * settings object.
 */
static void
liferea_webkit_disable_javascript_cb (GSettings *gsettings,
				      gchar *key,
				      gpointer webkit_settings)
{
	g_return_if_fail (key != NULL);

	g_object_set (
		webkit_settings,
		"enable-javascript",
		!g_settings_get_boolean (gsettings, key),
		NULL
	);
}

static void
liferea_webkit_enable_itp_cb (GSettings *gsettings,
				  gchar *key,
				  gpointer user_data)
{
	g_return_if_fail (key != NULL);

	webkit_network_session_set_itp_enabled (
	    webkit_network_session_get_default (),
	    g_settings_get_boolean (gsettings, key));
}

static gboolean
liferea_webkit_authorize_authenticated_peer (GDBusAuthObserver 	*observer,
					     GIOStream		*stream,
					     GCredentials	*credentials,
					     gpointer		user_data)
{
	gboolean authorized = FALSE;
	GCredentials *own_credentials = NULL;
	GError *error = NULL;

	if (!credentials) {
		g_printerr ("No credentials received from web extension.\n");
		return FALSE;
	}

	own_credentials = g_credentials_new ();

	if (g_credentials_is_same_user (credentials, own_credentials, &error)) {
		authorized = TRUE;
	} else {
		g_printerr ("Error authorizing web extension : %s\n", error->message);
		g_error_free (error);
	}
	g_object_unref (own_credentials);

	return authorized;
}

static void
liferea_webkit_on_dbus_connection_close (GDBusConnection *connection,
					 gboolean        remote_peer_vanished,
					 GError          *error,
					 gpointer        user_data)
{
	LifereaWebKit *webkit_impl = LIFEREA_WEBKIT (user_data);

	if (!remote_peer_vanished && error)
		g_warning ("DBus connection closed with error : %s", error->message);

	webkit_impl->dbus_connections = g_list_remove (webkit_impl->dbus_connections, connection);
	g_object_unref (connection);
}

static void
liferea_webkit_emit_page_created (GDBusConnection *connection,
			     const gchar *sender_name,
			     const gchar *object_path,
			     const gchar *interface_name,
			     const gchar *signal_name,
			     GVariant *parameters,
			     gpointer user_data)
{
	guint64 page_id;
	LifereaWebKit *webkit_impl = LIFEREA_WEBKIT (user_data);

	g_variant_get (parameters, "(t)", &page_id);
	g_signal_emit (webkit_impl,
		liferea_webkit_signals[PAGE_CREATED_SIGNAL],
		0,
		(gpointer) connection,
		page_id);
}

static void
on_page_created (LifereaWebKit *instance,
		 GDBusConnection *connection,
		 guint64 page_id,
		 gpointer web_view)
{
	if (webkit_web_view_get_page_id (WEBKIT_WEB_VIEW (web_view)) == page_id)
		liferea_web_view_set_dbus_connection (LIFEREA_WEB_VIEW (web_view), connection);
}


static gboolean
liferea_webkit_on_new_dbus_connection (GDBusServer *server, GDBusConnection *connection, gpointer user_data)
{
	LifereaWebKit *webkit_impl = LIFEREA_WEBKIT (user_data);

	webkit_impl->dbus_connections = g_list_append (webkit_impl->dbus_connections, g_object_ref (connection));
	g_signal_connect (connection,
			  "closed",
			  G_CALLBACK (liferea_webkit_on_dbus_connection_close),
			  webkit_impl);

	g_dbus_connection_signal_subscribe (
		connection,
		NULL,
		LIFEREA_WEB_EXTENSION_INTERFACE_NAME,
		"PageCreated",
		LIFEREA_WEB_EXTENSION_OBJECT_PATH,
		NULL,
		G_DBUS_SIGNAL_FLAGS_NONE,
		(GDBusSignalCallback)liferea_webkit_emit_page_created,
                webkit_impl,
                NULL);

	return TRUE;
}

static void
liferea_webkit_initialize_web_extensions (WebKitWebContext 	*context,
					  gpointer		user_data)
{
	gchar 	*guid = NULL;
	gchar 	*address = NULL;
	gchar 	*server_address = NULL;
	GError	*error = NULL;
	GDBusAuthObserver *observer = NULL;
	LifereaWebKit *webkit_impl = LIFEREA_WEBKIT (user_data);

	guid = g_dbus_generate_guid ();
	address = g_strdup_printf ("unix:tmpdir=%s", g_get_tmp_dir ());
	observer = g_dbus_auth_observer_new ();

	g_signal_connect (observer,
			  "authorize-authenticated-peer",
			  G_CALLBACK (liferea_webkit_authorize_authenticated_peer),
			  NULL);

	webkit_impl->dbus_server = g_dbus_server_new_sync (address,
					      G_DBUS_SERVER_FLAGS_NONE,//Flags
					      guid,
					      observer,
					      NULL, //Cancellable
					      &error);
	g_free (guid);
	g_free (address);
	g_object_unref (observer);
	if (webkit_impl->dbus_server == NULL) {
		g_printerr ("Error creating DBus server : %s\n", error->message);
		g_error_free (error);
		return;
        }
	g_dbus_server_start (webkit_impl->dbus_server);

	g_signal_connect (webkit_impl->dbus_server,
			  "new-connection",
			  G_CALLBACK (liferea_webkit_on_new_dbus_connection),
			  webkit_impl);

	webkit_web_context_set_web_process_extensions_directory (context, WEB_EXTENSIONS_DIR);
	server_address = g_strdup (g_dbus_server_get_client_address (webkit_impl->dbus_server));
	webkit_web_context_set_web_process_extensions_initialization_user_data (context, g_variant_new_take_string (server_address));
}

static void
liferea_webkit_download_started (WebKitWebContext	*context,
				      WebKitDownload 	*download,
				      gpointer 		user_data)
{
	WebKitURIRequest *request = webkit_download_get_request (download);
	webkit_download_cancel (download);
	download_url (webkit_uri_request_get_uri (request));
	download_show ();
}

static void
liferea_webkit_handle_liferea_scheme (WebKitURISchemeRequest *request, gpointer user_data)
{
	const gchar *path = webkit_uri_scheme_request_get_path (request);
	g_autofree gchar *rpath = NULL;
	GBytes *b;

	rpath = g_strdup_printf ("/org/gnome/liferea%s", path);
	b = g_resources_lookup_data (rpath, 0, NULL);
	if (b) {
		gsize length = 0;
		const guchar *data = g_bytes_get_data (b, &length);
		const gchar *mime;
		g_autoptr(GInputStream) stream = g_memory_input_stream_new_from_data (data, length, NULL);
		
		// For now we assume all resources are javascript, so MIME is hardcoded
		if (g_str_has_suffix (path, ".js"))
			mime = "text/javascript";
		else if (g_str_has_suffix (path, ".html"))
			mime = "text/html";
		else if (g_str_has_suffix (path, ".png"))
			mime = "image/png";
		else if (g_str_has_suffix (path, ".css"))
			mime = "text/css";
		else
			mime = "text/plain";

		webkit_uri_scheme_request_finish (request, stream, length, mime);
	} else {
		debug (DEBUG_HTML, "Failed to load liferea:// request for path %s (%s)", path, rpath);
		g_autoptr(GError) error = g_error_new (G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "Resource not found");
		webkit_uri_scheme_request_finish_error (request, error);
	}
}

static void
liferea_webkit_handle_feed_scheme (WebKitURISchemeRequest *request, gpointer user_data)
{
        const gchar *path = webkit_uri_scheme_request_get_path (request);

        feedlist_add_subscription (g_strdup (path), NULL, NULL, UPDATE_REQUEST_PRIORITY_HIGH);
}

static void
liferea_webkit_init (LifereaWebKit *self)
{
	gboolean			enable_itp;
	WebKitSecurityManager		*security_manager;
		
	self->dbus_connections = NULL;
	webkit_web_context_register_uri_scheme (webkit_web_context_get_default(), "liferea",
		(WebKitURISchemeRequestCallback) liferea_webkit_handle_liferea_scheme, NULL, NULL);
	webkit_web_context_register_uri_scheme (webkit_web_context_get_default(), "feed",
		(WebKitURISchemeRequestCallback) liferea_webkit_handle_feed_scheme, NULL, NULL);

	security_manager = webkit_web_context_get_security_manager (webkit_web_context_get_default ());
	
	/* CORS is necessary to have ESM modules working. Security wise this should be ok as this scheme 
	   is only used to fetch static resources. */
	webkit_security_manager_register_uri_scheme_as_cors_enabled (security_manager, "liferea");

	conf_signal_connect (
		"changed::" ENABLE_ITP,
		G_CALLBACK (liferea_webkit_enable_itp_cb),
		NULL
	);

	conf_get_bool_value (ENABLE_ITP, &enable_itp);

	webkit_network_session_set_itp_enabled (webkit_network_session_get_default (), enable_itp);

	/* Webkit web extensions */
	/*g_signal_connect (
		webkit_web_context_get_default (),
		"initialize-web-extensions",
		G_CALLBACK (liferea_webkit_initialize_web_extensions),
		self);
*/
	/*g_signal_connect (
		webkit_web_context_get_default (),
		"download-started",
		G_CALLBACK (liferea_webkit_download_started),
		self);*/
}

/**
 * Load HTML string into the rendering scrollpane
 *
 * Load an HTML string into the web view. This is used to render
 * HTML documents created internally.
 */
void
liferea_webkit_write_html (
	GtkWidget *webview,
	const gchar *string,
	const guint length,
	const gchar *base,
	const gchar *content_type
)
{
	// FIXME Avoid doing a copy ?
	GBytes *string_bytes = g_bytes_new (string, length);

	g_object_set (webkit_web_view_get_settings (WEBKIT_WEB_VIEW (webview)), "enable-javascript", TRUE, NULL);

	/* Note: we explicitely ignore the passed base URL
	   because we don't need it as Webkit supports <div href="">
	   and throws a security exception when accessing file://
	   with a non-file:// base URL */
	webkit_web_view_load_bytes (
		WEBKIT_WEB_VIEW (webview),
		string_bytes,
		content_type,
		"UTF-8",
		"liferea://"
	);
	g_bytes_unref (string_bytes);
}

void
liferea_webkit_clear (GtkWidget *webview)
{
	webkit_web_view_load_html (WEBKIT_WEB_VIEW (webview), "", NULL);
}

/**
 * Reset settings to safe preferences
 */
static void
liferea_webkit_default_settings (WebKitSettings *settings)
{
	gchar		*user_agent;
	gboolean	disable_javascript;

	conf_get_bool_value (DISABLE_JAVASCRIPT, &disable_javascript);
	g_object_set (settings, "enable-javascript", !disable_javascript, NULL);

	user_agent = network_get_user_agent ();
	webkit_settings_set_user_agent (settings, user_agent);
	g_free (user_agent);

	conf_signal_connect (
		"changed::" DISABLE_JAVASCRIPT,
		G_CALLBACK (liferea_webkit_disable_javascript_cb),
		settings
	);
}

/**
 * Create new WebkitWebView object and connect signals to a LifereaHtmlview
 */
GtkWidget *
liferea_webkit_new (LifereaBrowser *htmlview)
{
	WebKitWebView 	*view;
	WebKitSettings	*settings;

	if (!liferea_webkit)
		liferea_webkit = g_object_new (LIFEREA_TYPE_WEBKIT, NULL);

	view = WEBKIT_WEB_VIEW (liferea_web_view_new ());

	settings = webkit_settings_new ();
	liferea_webkit_default_settings (settings);
	webkit_web_view_set_settings (view, settings);

	/* Always drop cache on startup, so it does not grow over time */
	webkit_website_data_manager_clear (
		webkit_network_session_get_website_data_manager (webkit_network_session_get_default ()),
		WEBKIT_WEBSITE_DATA_ALL, 0, NULL, NULL, NULL);
  
	g_signal_connect_object (
		liferea_webkit,
		"page-created",
		G_CALLBACK (on_page_created),
		view,
		G_CONNECT_AFTER);

	/** Pass LifereaBrowser into the WebKitWebView object */
	g_object_set_data (
		G_OBJECT (view),
		"htmlview",
		htmlview
	);

	return GTK_WIDGET (view);
}

void
liferea_webkit_launch_url (GtkWidget *webview, const gchar *url)
{
	// FIXME: hack to make URIs like "gnome.org" work
	// https://bugs.webkit.org/show_bug.cgi?id=24195
	gchar *http_url;
	if (!strstr (url, "://")) {
		http_url = g_strdup_printf ("https://%s", url);
	} else {
		http_url = g_strdup (url);
	}

	// Force preference JS settings when launching external URL
	// needed, because we might be switching from internal reader mode
	liferea_webkit_default_settings (webkit_web_view_get_settings (WEBKIT_WEB_VIEW (webview)));

	webkit_web_view_load_uri (
		WEBKIT_WEB_VIEW (webview),
		http_url
	);

	g_free (http_url);
}

void
liferea_webkit_change_zoom_level (GtkWidget *webview, gfloat zoom_level)
{
	webkit_web_view_set_zoom_level (WEBKIT_WEB_VIEW (webview), zoom_level);
}

gfloat
liferea_webkit_get_zoom_level (GtkWidget *webview)
{
	return webkit_web_view_get_zoom_level (WEBKIT_WEB_VIEW (webview));
}

void
liferea_webkit_copy_selection (GtkWidget *webview)
{
	webkit_web_view_execute_editing_command (WEBKIT_WEB_VIEW (webview), WEBKIT_EDITING_COMMAND_COPY);
}

void
liferea_webkit_scroll_pagedown (GtkWidget *webview)
{
	liferea_web_view_scroll_pagedown (LIFEREA_WEB_VIEW (webview));
}

void
liferea_webkit_set_proxy (ProxyDetectMode mode)
{
	switch (mode) {
		default:
		case PROXY_DETECT_MODE_MANUAL:
		case PROXY_DETECT_MODE_AUTO:
			webkit_network_session_set_proxy_settings (
			     webkit_network_session_get_default (),
			     WEBKIT_NETWORK_PROXY_MODE_DEFAULT,
			     NULL);
			break;
		case PROXY_DETECT_MODE_NONE:
			webkit_network_session_set_proxy_settings (
			     webkit_network_session_get_default (),
			     WEBKIT_NETWORK_PROXY_MODE_NO_PROXY,
			     NULL);
			break;
	}
}

/**
 * Load liferea.css via user style sheet
 */
void
liferea_webkit_reload_style (GtkWidget *webview, const gchar *userCSS, const gchar *defaultCSS)
{
	WebKitUserContentManager *manager = webkit_web_view_get_user_content_manager (WEBKIT_WEB_VIEW (webview));

	webkit_user_content_manager_remove_all_style_sheets (manager);

	if (default_stylesheet)
		webkit_user_style_sheet_unref (default_stylesheet);

	// default stylesheet should only apply to HTML written to the view,
	// not when browsing
	const gchar *deny[] = { "http://*/*", "https://*/*",  NULL };
	default_stylesheet = webkit_user_style_sheet_new (defaultCSS,
		WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
		WEBKIT_USER_STYLE_LEVEL_USER,
		NULL,
		deny);
	webkit_user_content_manager_add_style_sheet (manager, default_stylesheet);

	if (user_stylesheet)
		webkit_user_style_sheet_unref (user_stylesheet);

	user_stylesheet = webkit_user_style_sheet_new (userCSS,
		WEBKIT_USER_CONTENT_INJECT_ALL_FRAMES,
		WEBKIT_USER_STYLE_LEVEL_USER,
		NULL,
		NULL);
	webkit_user_content_manager_add_style_sheet (manager, user_stylesheet);
}

/**
 * Reload the current contents of webview
 */
void
liferea_webkit_reload (GtkWidget *webview)
{
	liferea_webkit_default_settings (webkit_web_view_get_settings (WEBKIT_WEB_VIEW (webview)));

	webkit_web_view_reload (WEBKIT_WEB_VIEW (webview));
}
