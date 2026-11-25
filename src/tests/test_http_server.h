/**
 * @file test_http_server.h  Minimal in-process HTTP server for test cases
 *
 * The server listens on a random localhost port and is served by the default
 * main context, so test code must iterate the main loop for it to respond.
 * Paths passed to handlers are already URI-decoded.
 */

#ifndef _TEST_HTTP_SERVER_H
#define _TEST_HTTP_SERVER_H

#include <libsoup/soup.h>

typedef struct TestHttpServer TestHttpServer;

typedef void (*TestHttpHandler) (TestHttpServer *s, SoupServerMessage *msg, const gchar *path, gpointer user_data);

TestHttpServer *test_http_server_new (void);
void test_http_server_free (TestHttpServer *s);

/** Returns "http://127.0.0.1:<port><path>" */
gchar *test_http_server_get_url (TestHttpServer *s, const gchar *path);

/** Routes all requests with a path starting with path_prefix to cb (first match wins) */
void test_http_server_add_handler (TestHttpServer *s, const gchar *path_prefix, TestHttpHandler cb, gpointer user_data);

/** Serves a fixed body for GET requests of path */
void test_http_server_add_static (TestHttpServer *s, const gchar *path, const gchar *content_type, const gchar *body);

/** Answers requests below path_prefix without matching Basic auth credentials with 401 */
void test_http_server_require_basic_auth (TestHttpServer *s, const gchar *path_prefix, const gchar *user, const gchar *pass);

/** Answers requests for method + path with the given status instead of routing them */
void test_http_server_set_fail (TestHttpServer *s, const gchar *method, const gchar *path, guint status);

/** Delays routing of requests for method + path by ms milliseconds */
void test_http_server_set_delay (TestHttpServer *s, const gchar *method, const gchar *path, guint ms);

/** Number of requests seen whose "METHOD path" starts with prefix */
guint test_http_server_request_count (TestHttpServer *s, const gchar *prefix);

#endif
