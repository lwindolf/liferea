/**
 * @file test_http_server.c  Minimal in-process HTTP server for test cases
 *
 * Copyright (C) 2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "tests/test_http_server.h"

#include <string.h>

struct TestHttpServer {
	SoupServer	*soup;
	GSList		*routes;	/* Route*, in registration order */
	GPtrArray	*log;		/* "METHOD path" per request */
	GHashTable	*fails;		/* "METHOD path" -> status */
	GHashTable	*delays;	/* "METHOD path" -> ms */
	gchar		*auth;		/* expected Authorization header or NULL */
	gchar		*authPrefix;	/* paths requiring auth */
};

typedef struct {
	gchar		*prefix;
	TestHttpHandler	cb;
	gpointer	data;
} Route;

typedef struct {
	gchar	*type;
	gchar	*body;
} StaticBody;

typedef struct {
	TestHttpServer		*s;
	SoupServerMessage	*msg;
	gchar			*path;
} Delayed;

static void
dispatch (TestHttpServer *s, SoupServerMessage *msg, const gchar *path)
{
	for (GSList *i = s->routes; i; i = i->next) {
		Route *r = i->data;
		if (g_str_has_prefix (path, r->prefix)) {
			r->cb (s, msg, path, r->data);
			return;
		}
	}
	soup_server_message_set_status (msg, SOUP_STATUS_NOT_FOUND, NULL);
}

static gboolean
delayed_cb (gpointer data)
{
	Delayed *d = data;

	dispatch (d->s, d->msg, d->path);
	soup_server_message_unpause (d->msg);
	g_object_unref (d->msg);
	g_free (d->path);
	g_free (d);
	return G_SOURCE_REMOVE;
}

static void
server_cb (SoupServer *soup, SoupServerMessage *msg, const gchar *raw_path, GHashTable *query, gpointer user_data)
{
	TestHttpServer *s = user_data;
	g_autofree gchar *path = g_uri_unescape_string (raw_path, NULL);
	g_autofree gchar *key = g_strdup_printf ("%s %s", soup_server_message_get_method (msg), path);
	const gchar *auth = soup_message_headers_get_one (soup_server_message_get_request_headers (msg), "Authorization");
	guint status = GPOINTER_TO_UINT (g_hash_table_lookup (s->fails, key));
	guint delay = GPOINTER_TO_UINT (g_hash_table_lookup (s->delays, key));

	g_ptr_array_add (s->log, g_strdup (key));

	if (s->auth && g_str_has_prefix (path, s->authPrefix) && g_strcmp0 (auth, s->auth)) {
		soup_message_headers_append (soup_server_message_get_response_headers (msg), "WWW-Authenticate", "Basic realm=\"test\"");
		soup_server_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED, NULL);
	} else if (status) {
		soup_server_message_set_status (msg, status, NULL);
	} else if (delay) {
		Delayed *d = g_new0 (Delayed, 1);
		d->s = s;
		d->msg = g_object_ref (msg);
		d->path = g_strdup (path);
		soup_server_message_pause (msg);
		g_timeout_add (delay, delayed_cb, d);
	} else {
		dispatch (s, msg, path);
	}
}

TestHttpServer *
test_http_server_new (void)
{
	TestHttpServer *s = g_new0 (TestHttpServer, 1);
	GError *err = NULL;

	s->soup = soup_server_new (NULL, NULL);
	s->log = g_ptr_array_new_with_free_func (g_free);
	s->fails = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	s->delays = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
	soup_server_add_handler (s->soup, NULL, server_cb, s, NULL);
	g_assert_true (soup_server_listen_local (s->soup, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &err));
	g_assert_no_error (err);
	return s;
}

void
test_http_server_free (TestHttpServer *s)
{
	soup_server_disconnect (s->soup);
	g_object_unref (s->soup);
	for (GSList *i = s->routes; i; i = i->next) {
		Route *r = i->data;
		g_free (r->prefix);
		g_free (r);
	}
	g_slist_free (s->routes);
	g_ptr_array_unref (s->log);
	g_hash_table_destroy (s->fails);
	g_hash_table_destroy (s->delays);
	g_free (s->auth);
	g_free (s->authPrefix);
	g_free (s);
}

gchar *
test_http_server_get_url (TestHttpServer *s, const gchar *path)
{
	GSList *uris = soup_server_get_uris (s->soup);
	gchar *url = g_strdup_printf ("http://127.0.0.1:%d%s", g_uri_get_port (uris->data), path);

	g_slist_free_full (uris, (GDestroyNotify)g_uri_unref);
	return url;
}

void
test_http_server_add_handler (TestHttpServer *s, const gchar *path_prefix, TestHttpHandler cb, gpointer user_data)
{
	Route *r = g_new0 (Route, 1);

	r->prefix = g_strdup (path_prefix);
	r->cb = cb;
	r->data = user_data;
	s->routes = g_slist_append (s->routes, r);
}

static void
static_cb (TestHttpServer *s, SoupServerMessage *msg, const gchar *path, gpointer user_data)
{
	StaticBody *b = user_data;

	soup_server_message_set_response (msg, b->type, SOUP_MEMORY_COPY, b->body, strlen (b->body));
	soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
}

void
test_http_server_add_static (TestHttpServer *s, const gchar *path, const gchar *content_type, const gchar *body)
{
	StaticBody *b = g_new0 (StaticBody, 1);

	b->type = g_strdup (content_type);
	b->body = g_strdup (body);
	test_http_server_add_handler (s, path, static_cb, b);	/* leaked on purpose, lives as long as the test process */
}

void
test_http_server_require_basic_auth (TestHttpServer *s, const gchar *path_prefix, const gchar *user, const gchar *pass)
{
	g_autofree gchar *credentials = g_strdup_printf ("%s:%s", user, pass);
	g_autofree gchar *encoded = g_base64_encode ((const guchar *)credentials, strlen (credentials));

	g_free (s->auth);
	g_free (s->authPrefix);
	s->auth = g_strdup_printf ("Basic %s", encoded);
	s->authPrefix = g_strdup (path_prefix);
}

void
test_http_server_set_fail (TestHttpServer *s, const gchar *method, const gchar *path, guint status)
{
	g_hash_table_insert (s->fails, g_strdup_printf ("%s %s", method, path), GUINT_TO_POINTER (status));
}

void
test_http_server_set_delay (TestHttpServer *s, const gchar *method, const gchar *path, guint ms)
{
	g_hash_table_insert (s->delays, g_strdup_printf ("%s %s", method, path), GUINT_TO_POINTER (ms));
}

guint
test_http_server_request_count (TestHttpServer *s, const gchar *prefix)
{
	guint count = 0;

	for (guint i = 0; i < s->log->len; i++)
		if (g_str_has_prefix (g_ptr_array_index (s->log, i), prefix))
			count++;
	return count;
}
