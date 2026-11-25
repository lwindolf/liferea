/**
 * @file webdav_source.c  Test cases for the WebDAV node source
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

#include <glib.h>
#include <string.h>

#include "common.h"
#include "conf.h"
#include "db.h"
#include "debug.h"
#include "feedlist.h"
#include "itemlist.h"
#include "net_monitor.h"
#include "node.h"
#include "node_source.h"
#include "node_sources/webdav_source.h"
#include "subscription.h"
#include "update_job_queue.h"
#include "xml.h"
#include "tests/test_http_server.h"

#define DAV_COL		"/dav/Liferea Sync/"
#define DAV_INDEX	DAV_COL "index.json"

#define EMPTY_OPML	"<?xml version=\"1.0\"?><opml version=\"1.0\"><head/><body>%s</body></opml>"
#define ATOM		"<?xml version=\"1.0\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><id>urn:test</id><title>Test</title>" \
			"<updated>2026-01-01T00:00:00Z</updated><entry><id>urn:1</id><title>One</title>" \
			"<updated>2026-01-01T00:00:00Z</updated><link href=\"http://localhost/1\"/></entry></feed>"

/* Test fixture: server + WebDAV node source (client) */

typedef struct {
	TestHttpServer	*srv;
	GHashTable	*fs;		/* fake WebDAV backend: path -> body, collections have a trailing "/" in their path */
	guint		index404s;	/* number of GETs of index.json that found nothing */
	Node		*root;
	gchar		*feedUrl;	/* feed served by the test server */
	const gchar	*waitKey;	/* fs path to wait for in fs_has_wait_key() */
} Fx;

/* Fake WebDAV backend */

static void
dav_put (GHashTable *fs, const gchar *path, const gchar *body)
{
	g_hash_table_insert (fs, g_strdup (path), g_strdup (body));
}

static void
dav_cb (TestHttpServer *s, SoupServerMessage *msg, const gchar *path, gpointer data)
{
	Fx *fx = data;
	GHashTable *fs = fx->fs;
	const gchar *method = soup_server_message_get_method (msg);
	g_autofree gchar *file = g_strdup (path);
	g_autofree gchar *dir = NULL, *parent = NULL, *col = NULL;
	guint status = SOUP_STATUS_METHOD_NOT_ALLOWED;

	if (g_str_has_suffix (file, "/"))
		file[strlen (file) - 1] = '\0';
	dir = g_path_get_dirname (file);
	parent = g_strconcat (dir, "/", NULL);
	col = g_strconcat (file, "/", NULL);

	if (!g_strcmp0 (method, "MKCOL")) {
		status = g_hash_table_contains (fs, col) ? 405 : !g_hash_table_contains (fs, parent) ? 409 : 201;
		if (status == 201)
			dav_put (fs, col, "");
	} else if (!g_strcmp0 (method, "PUT")) {
		SoupMessageBody *body = soup_server_message_get_request_body (msg);

		status = !g_hash_table_contains (fs, parent) ? 409 : g_hash_table_contains (fs, file) ? 204 : 201;
		if (status != 409) {
			g_autofree gchar *content = g_strndup (body->data, body->length);
			dav_put (fs, file, content);
		}
	} else if (!g_strcmp0 (method, "GET")) {
		const gchar *content = g_hash_table_lookup (fs, file);

		status = content ? 200 : 404;
		if (!content && !g_strcmp0 (file, DAV_INDEX))
			fx->index404s++;
		if (content)
			soup_server_message_set_response (msg, "application/json", SOUP_MEMORY_COPY, content, strlen (content));
	}
	soup_server_message_set_status (msg, status, NULL);
}

/* Test fixture: server + WebDAV node source (client) */

static void
setup_once (void)
{
	static gboolean done = FALSE;
	g_autofree gchar *tmp = NULL, *filename = NULL;

	if (done)
		return;
	done = TRUE;

	// Never touch real user data
	tmp = g_dir_make_tmp ("liferea-test-XXXXXX", NULL);
	g_setenv ("XDG_CONFIG_HOME", tmp, TRUE);
	g_setenv ("XDG_DATA_HOME", tmp, TRUE);
	g_setenv ("XDG_CACHE_HOME", tmp, TRUE);

	conf_init ();
	network_monitor_get ();
	update_job_queue_get_instance ();
	db_init ();
	xml_init ();
	g_object_new (ITEMLIST_TYPE, NULL);

	// An existing empty feed list avoids importing (and updating) the default feed list
	filename = common_create_config_filename ("feedlist.opml");
	g_file_set_contents (filename, g_strdup_printf (EMPTY_OPML, ""), -1, NULL);
	g_object_new (FEED_LIST_TYPE, NULL);
}

static Fx *
fx_new (void)
{
	Fx *fx = g_new0 (Fx, 1);

	setup_once ();
	fx->srv = test_http_server_new ();
	fx->fs = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	dav_put (fx->fs, "/dav/", "");
	test_http_server_add_handler (fx->srv, "/dav/", dav_cb, fx);
	test_http_server_add_static (fx->srv, "/feeds/test.atom", "application/atom+xml", ATOM);
	test_http_server_require_basic_auth (fx->srv, "/dav/", "user", "pass");
	fx->feedUrl = test_http_server_get_url (fx->srv, "/feeds/test.atom");
	return fx;
}

// Creates the source like the "new source" dialog does and starts the login.
// The OPML outlines are the local feeds the source starts with.
static void
fx_start (Fx *fx, const gchar *password, const gchar *outlines)
{
	g_autofree gchar *url = test_http_server_get_url (fx->srv, "/dav");
	g_autofree gchar *opml = g_strdup_printf (EMPTY_OPML, outlines);

	fx->root = node_new ("source");

	g_autofree gchar *basename = g_strdup_printf ("feedlist_%s.opml", fx->root->id);
	g_autofree gchar *filename = common_create_data_filename (basename);
	g_file_set_contents (filename, opml, -1, NULL);

	node_source_new (fx->root, webdav_source_get_type (), url);
	node_set_title (fx->root, "test");
	subscription_set_auth_info (fx->root->subscription, "user", password);
	feedlist_node_added (fx->root);
	node_source_update (fx->root);
}

static void
fx_free (Fx *fx)
{
	test_http_server_free (fx->srv);
	g_hash_table_destroy (fx->fs);
	g_free (fx->feedUrl);
	g_free (fx);
}

static gboolean
wait_for (gboolean (*cond) (Fx *), Fx *fx, guint seconds)
{
	gint64 end = g_get_monotonic_time () + seconds * G_USEC_PER_SEC;

	while (!cond (fx) && g_get_monotonic_time () < end) {
		g_main_context_iteration (NULL, FALSE);
		g_usleep (5000);
	}
	return cond (fx);
}

static gboolean
login_done (Fx *fx)
{
	return fx->root->source->loginState == NODE_SOURCE_STATE_ACTIVE ||
	       fx->root->source->loginState == NODE_SOURCE_STATE_AUTH_FAILED;
}

static gboolean
fs_has_wait_key (Fx *fx)
{
	return g_hash_table_contains (fx->fs, fx->waitKey);
}

static gboolean
folder_exists (Fx *fx)
{
	return node_from_id ("folder1") != NULL;
}

static gboolean
feed_has_items (Fx *fx)
{
	Node *feed = feedlist_find_node (fx->root, NODE_BY_URL, fx->feedUrl);

	return feed && db_itemset_get_item_count (feed->id) > 0;
}

/* Test cases */

// Regression: nothing local and nothing remote must still end up in a fully created remote collection
static void
tc_empty_local_and_remote (void)
{
	Fx *fx = fx_new ();
	const gchar *index;

	fx_start (fx, "pass", "");
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_ACTIVE);
	g_assert_true (fx->root->available);
	g_assert_cmpuint (test_http_server_request_count (fx->srv, "MKCOL "), >, 0);	// server only accepts requests with Basic auth
	g_assert_true (g_hash_table_contains (fx->fs, DAV_COL));
	fx->waitKey = DAV_INDEX;
	g_assert_true (wait_for (fs_has_wait_key, fx, 5));
	index = g_hash_table_lookup (fx->fs, DAV_INDEX);
	g_assert_nonnull (index);
	g_assert_nonnull (strstr (index, "\"nodes\""));
	fx_free (fx);
}

static void
tc_remote_feeds_imported (void)
{
	Fx *fx = fx_new ();
	g_autofree gchar *index = g_strdup_printf (
		"{\"nodes\":[{\"type\":\"folder\",\"id\":\"folder1\",\"title\":\"Folder\"},"
		"{\"type\":\"feed\",\"id\":\"feed1\",\"parent\":\"folder1\",\"title\":\"Feed\",\"source\":\"%s\",\"feed_mtime\":0,\"state_mtime\":0}]}", fx->feedUrl);
	g_autofree gchar *nodeJson = g_strdup_printf ("{\"node\":{\"id\":\"feed1\",\"title\":\"Feed\",\"source\":\"%s\"},\"items\":[]}", fx->feedUrl);

	dav_put (fx->fs, DAV_COL, "");
	dav_put (fx->fs, DAV_COL "feed1/", "");
	dav_put (fx->fs, DAV_INDEX, index);
	dav_put (fx->fs, DAV_COL "feed1/node.json", nodeJson);
	dav_put (fx->fs, DAV_COL "feed1/state.json", "{}");

	fx_start (fx, "pass", "");
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_ACTIVE);
	g_assert_true (wait_for (folder_exists, fx, 5));
	g_assert_true (IS_FOLDER (node_from_id ("folder1")));
	g_assert_true (wait_for (feed_has_items, fx, 10));	// feed was really fetched from the test server
	g_assert_true (feedlist_find_node (fx->root, NODE_BY_URL, fx->feedUrl)->parent == node_from_id ("folder1"));
	fx_free (fx);
}

// Uploads are lazy (30s by default), so shorten the delay
static void
tc_local_feeds_uploaded (void)
{
	Fx *fx;
	g_autofree gchar *outline = NULL;

	webdav_source_set_sync_delays_ms (200, 200);
	fx = fx_new ();
	outline = g_strdup_printf ("<outline id=\"localfeed\" title=\"Local\" xmlUrl=\"%s\"/>", fx->feedUrl);
	fx_start (fx, "pass", outline);
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_ACTIVE);

	fx->waitKey = DAV_COL "localfeed/node.json";
	g_assert_true (wait_for (fs_has_wait_key, fx, 5));
	g_assert_true (g_hash_table_contains (fx->fs, DAV_COL "localfeed/"));
	g_assert_nonnull (strstr (g_hash_table_lookup (fx->fs, DAV_INDEX), "localfeed"));
	fx_free (fx);
}

static void
tc_wrong_password (void)
{
	Fx *fx = fx_new ();

	fx_start (fx, "wrong", "");
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_AUTH_FAILED);
	g_assert_false (g_hash_table_contains (fx->fs, DAV_COL));
	fx_free (fx);
}

static void
tc_mkcol_fails (void)
{
	Fx *fx = fx_new ();

	test_http_server_set_fail (fx->srv, "MKCOL", DAV_COL, 500);
	fx_start (fx, "pass", "");
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_false (fx->root->available);
	fx_free (fx);
}

// Known race: the bootstrap flow does not wait for the index upload before fetching the index
static void
tc_slow_index_upload (void)
{
	Fx *fx = fx_new ();

	test_http_server_set_delay (fx->srv, "PUT", DAV_INDEX, 500);
	fx_start (fx, "pass", "");
	g_assert_true (wait_for (login_done, fx, 5));
	fx->waitKey = DAV_INDEX;
	g_assert_true (wait_for (fs_has_wait_key, fx, 5));
	g_assert_cmpuint (fx->index404s, ==, 1);	// only the initial GET may find nothing
	fx_free (fx);
}

int
test_webdav_source (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	if (g_strv_contains ((const gchar **)argv, "--debug"))
		debug_set_flags (DEBUG_UPDATE | DEBUG_NET);

	g_test_add_func ("/webdav_source/empty-local-and-remote", tc_empty_local_and_remote);
	g_test_add_func ("/webdav_source/remote-feeds-imported", tc_remote_feeds_imported);
	g_test_add_func ("/webdav_source/local-feeds-uploaded", tc_local_feeds_uploaded);
	g_test_add_func ("/webdav_source/wrong-password", tc_wrong_password);
	g_test_add_func ("/webdav_source/mkcol-fails", tc_mkcol_fails);
	g_test_add_func ("/webdav_source/slow-index-upload", tc_slow_index_upload);

	return g_test_run ();
}
