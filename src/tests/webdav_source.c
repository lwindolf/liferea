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
	const gchar	*needle;	/* text to wait for in index.json in index_has_needle() */
	GHashTable	*index;		/* index entries of the fake remote: id -> JSON object */
	gint64		mtime;		/* last mtime handed out for remote changes */
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
	} else if (!g_strcmp0 (method, "DELETE")) {
		GHashTableIter iter;
		gpointer key;

		status = g_hash_table_contains (fs, file) || g_hash_table_contains (fs, col) ? 204 : 404;
		g_hash_table_iter_init (&iter, fs);
		while (g_hash_table_iter_next (&iter, &key, NULL))
			if (!g_strcmp0 (key, file) || g_str_has_prefix (key, col))
				g_hash_table_iter_remove (&iter);
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

/* Fake remote side: changes made by "another client", published with dav_publish() */

static void
dav_folder (Fx *fx, const gchar *id, const gchar *parent, const gchar *title)
{
	g_autofree gchar *parentJson = parent ? g_strdup_printf ("\"parent\":\"%s\",", parent) : g_strdup ("");

	g_hash_table_insert (fx->index, g_strdup (id),
		g_strdup_printf ("{\"type\":\"folder\",\"id\":\"%s\",%s\"title\":\"%s\"}", id, parentJson, title));
}

// Adds or changes a feed. Each call bumps the mtimes, so clients pick up the change.
static void
dav_feed (Fx *fx, const gchar *id, const gchar *parent)
{
	g_autofree gchar *parentJson = parent ? g_strdup_printf ("\"parent\":\"%s\",", parent) : g_strdup ("");
	g_autofree gchar *dir = g_strdup_printf (DAV_COL "%s/", id);
	g_autofree gchar *nodeJson = g_strdup_printf ("{\"node\":{\"id\":\"%s\",\"title\":\"Feed\",\"source\":\"%s\"},\"items\":[]}", id, fx->feedUrl);
	g_autofree gchar *nodePath = g_strconcat (dir, "node.json", NULL);
	g_autofree gchar *statePath = g_strconcat (dir, "state.json", NULL);

	fx->mtime++;
	dav_put (fx->fs, dir, "");
	dav_put (fx->fs, nodePath, nodeJson);
	dav_put (fx->fs, statePath, "{}");
	g_hash_table_insert (fx->index, g_strdup (id), g_strdup_printf (
		"{\"type\":\"feed\",\"id\":\"%s\",%s\"title\":\"Feed\",\"source\":\"%s\",\"feed_mtime\":%" G_GINT64_FORMAT ",\"state_mtime\":%" G_GINT64_FORMAT "}",
		id, parentJson, fx->feedUrl, fx->mtime, fx->mtime));
}

// Removes a feed or folder
static void
dav_remove (Fx *fx, const gchar *id)
{
	g_autofree gchar *dir = g_strdup_printf (DAV_COL "%s/", id);
	GHashTableIter iter;
	gpointer key;

	g_hash_table_remove (fx->index, id);
	g_hash_table_iter_init (&iter, fx->fs);
	while (g_hash_table_iter_next (&iter, &key, NULL))
		if (g_str_has_prefix (key, dir))
			g_hash_table_iter_remove (&iter);
}

static void
dav_publish (Fx *fx)
{
	GHashTableIter iter;
	gpointer entry;
	GString *json = g_string_new ("{\"nodes\":[");

	g_hash_table_iter_init (&iter, fx->index);
	while (g_hash_table_iter_next (&iter, NULL, &entry))
		g_string_append_printf (json, "%s%s", json->str[json->len - 1] == '[' ? "" : ",", (const gchar *)entry);
	g_string_append (json, "]}");
	dav_put (fx->fs, DAV_INDEX, json->str);
	g_string_free (json, TRUE);
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

	// Lazy uploads are delayed by 30s by default
	webdav_source_set_sync_delays_ms (200, 200);

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
	fx->index = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	fx->mtime = g_get_real_time () / G_USEC_PER_SEC + 1000;	// newer than anything the client uploads
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
	g_hash_table_destroy (fx->index);
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

// Waits until the client stopped talking to the server, so remote changes cannot race with its uploads
static void
wait_idle (Fx *fx)
{
	gint64 end = g_get_monotonic_time () + 5 * G_USEC_PER_SEC;
	gint64 quietSince = g_get_monotonic_time ();
	guint count = test_http_server_request_count (fx->srv, "");

	while (g_get_monotonic_time () < end && g_get_monotonic_time () - quietSince < 300 * 1000) {
		guint now = test_http_server_request_count (fx->srv, "");

		if (now != count) {
			count = now;
			quietSince = g_get_monotonic_time ();
		}
		g_main_context_iteration (NULL, FALSE);
		g_usleep (5000);
	}
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
index_has_needle (Fx *fx)
{
	const gchar *index = g_hash_table_lookup (fx->fs, DAV_INDEX);

	return index && strstr (index, fx->needle);
}

static gboolean
index_lacks_needle (Fx *fx)
{
	return !index_has_needle (fx);
}

static gboolean
fs_lacks_wait_key (Fx *fx)
{
	return !fs_has_wait_key (fx);
}

static Node *
find_node_by_id (Node *parent, const gchar *id)
{
	for (GSList *l = parent->children; l; l = g_slist_next (l)) {
		Node *node = l->data, *found;

		if (!g_strcmp0 (node->id, id))
			return node;
		if ((found = find_node_by_id (node, id)))
			return found;
	}
	return NULL;
}

// Logs in and waits for the initial sync to settle
static void
fx_ready (Fx *fx)
{
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_ACTIVE);
	fx->waitKey = DAV_INDEX;
	g_assert_true (wait_for (fs_has_wait_key, fx, 5));
	wait_idle (fx);
}

static gboolean
feed_exists (Fx *fx)
{
	return feedlist_find_node (fx->root, NODE_BY_URL, fx->feedUrl) != NULL;
}

static gboolean
feed_gone (Fx *fx)
{
	return !feed_exists (fx);
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

static void
tc_local_feeds_uploaded (void)
{
	Fx *fx;
	g_autofree gchar *outline = NULL;

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

// Regression: the bootstrap flow must wait for the index upload before the 1st update fetches the index
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

// Starts with a synced remote that has two folders and a feed in the first one.
// Test cases use distinct ids as nodes of earlier test cases are still around.
static void
fx_start_synced (Fx *fx, const gchar *folder1, const gchar *folder2, const gchar *feed)
{
	dav_put (fx->fs, DAV_COL, "");
	dav_folder (fx, folder1, NULL, "Folder 1");
	dav_folder (fx, folder2, NULL, "Folder 2");
	dav_feed (fx, feed, folder1);
	dav_publish (fx);

	fx_start (fx, "pass", "");
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_ACTIVE);
	g_assert_true (wait_for (feed_has_items, fx, 10));
	g_assert_true (feedlist_find_node (fx->root, NODE_BY_URL, fx->feedUrl)->parent == node_from_id (folder1));
	wait_idle (fx);
}

static gboolean
feed_in_folder2 (Fx *fx)
{
	Node *feed = feedlist_find_node (fx->root, NODE_BY_URL, fx->feedUrl);

	return feed && feed->parent == node_from_id (fx->needle);
}

// Another client moves a feed into another folder
static void
tc_remote_feed_moved (void)
{
	Fx *fx = fx_new ();

	fx_start_synced (fx, "mvr-f1", "mvr-f2", "mvr-feed");

	dav_feed (fx, "mvr-feed", "mvr-f2");
	dav_publish (fx);
	node_source_update (fx->root);

	fx->needle = "mvr-f2";
	g_assert_true (wait_for (feed_in_folder2, fx, 5));
	fx_free (fx);
}

// The user moves a feed into another folder, which has to be reflected in the remote index
static void
tc_local_feed_moved (void)
{
	Fx *fx = fx_new ();
	Node *feed;
	const gchar *index;

	fx_start_synced (fx, "mvl-f1", "mvl-f2", "mvl-feed");
	g_assert_nonnull (strstr (g_hash_table_lookup (fx->fs, DAV_INDEX), "\"parent\":\"mvl-f1\""));

	feed = feedlist_find_node (fx->root, NODE_BY_URL, fx->feedUrl);
	feedlist_node_was_moved (feed, node_from_id ("mvl-f2"), -1, TRUE);

	fx->needle = "\"parent\":\"mvl-f2\"";
	g_assert_true (wait_for (index_has_needle, fx, 5));
	index = g_hash_table_lookup (fx->fs, DAV_INDEX);
	g_assert_null (strstr (index, "\"parent\":\"mvl-f1\""));
	fx_free (fx);
}

// Another client removes a feed
static void
tc_remote_feed_removed (void)
{
	Fx *fx = fx_new ();

	fx_start_synced (fx, "rmr-f1", "rmr-f2", "rmr-feed");

	dav_remove (fx, "rmr-feed");
	dav_publish (fx);
	node_source_update (fx->root);

	g_assert_true (wait_for (feed_gone, fx, 5));
	wait_idle (fx);
	fx->needle = "rmr-feed";
	g_assert_true (index_lacks_needle (fx));	// must not be uploaded again
	g_assert_nonnull (node_from_id ("rmr-f1"));	// folders were not touched
	fx_free (fx);
}

// Another client removes a folder including its feed
static void
tc_remote_folder_removed (void)
{
	Fx *fx = fx_new ();

	fx_start_synced (fx, "rmfr-f1", "rmfr-f2", "rmfr-feed");

	dav_remove (fx, "rmfr-feed");
	dav_remove (fx, "rmfr-f1");
	dav_publish (fx);
	node_source_update (fx->root);

	g_assert_true (wait_for (feed_gone, fx, 5));
	g_assert_null (node_from_id ("rmfr-f1"));
	g_assert_nonnull (node_from_id ("rmfr-f2"));
	wait_idle (fx);
	fx->needle = "rmfr-f1";
	g_assert_true (index_lacks_needle (fx));
	fx_free (fx);
}

// The user removes a feed, which has to disappear from the remote and must not come back
static void
tc_local_feed_removed (void)
{
	Fx *fx = fx_new ();
	g_autofree gchar *dir = g_strdup (DAV_COL "rml-feed/");

	fx_start_synced (fx, "rml-f1", "rml-f2", "rml-feed");
	g_assert_true (g_hash_table_contains (fx->fs, dir));

	feedlist_remove_node (feedlist_find_node (fx->root, NODE_BY_URL, fx->feedUrl));

	fx->needle = "rml-feed";
	g_assert_true (wait_for (index_lacks_needle, fx, 5));
	fx->waitKey = dir;
	g_assert_true (wait_for (fs_lacks_wait_key, fx, 5));

	node_source_update (fx->root);
	wait_idle (fx);
	g_assert_true (feed_gone (fx));
	fx_free (fx);
}

// The user removes a folder including its feed
static void
tc_local_folder_removed (void)
{
	Fx *fx = fx_new ();
	g_autofree gchar *dir = g_strdup (DAV_COL "rmfl-feed/");

	fx_start_synced (fx, "rmfl-f1", "rmfl-f2", "rmfl-feed");

	feedlist_remove_node (node_from_id ("rmfl-f1"));

	fx->needle = "rmfl-f1";
	g_assert_true (wait_for (index_lacks_needle, fx, 5));
	fx->needle = "rmfl-feed";
	g_assert_true (wait_for (index_lacks_needle, fx, 5));
	fx->waitKey = dir;
	g_assert_true (wait_for (fs_lacks_wait_key, fx, 5));

	node_source_update (fx->root);
	wait_idle (fx);
	g_assert_true (feed_gone (fx));
	g_assert_null (node_from_id ("rmfl-f1"));
	g_assert_nonnull (node_from_id ("rmfl-f2"));
	fx_free (fx);
}

// Another client adds a feed
static void
tc_remote_feed_added (void)
{
	Fx *fx = fx_new ();

	fx_start (fx, "pass", "");
	fx_ready (fx);
	g_assert_null (feedlist_find_node (fx->root, NODE_BY_URL, fx->feedUrl));

	dav_feed (fx, "feed1", NULL);
	dav_publish (fx);
	node_source_update (fx->root);

	g_assert_true (wait_for (feed_has_items, fx, 5));
	g_assert_true (feedlist_find_node (fx->root, NODE_BY_URL, fx->feedUrl)->parent == fx->root);
	fx_free (fx);
}

// The user adds a feed, which has to show up in the remote collection and index
static void
tc_local_feed_added (void)
{
	Fx *fx = fx_new ();
	g_autofree gchar *nodePath = NULL, *statePath = NULL;
	Node *feed;

	fx_start (fx, "pass", "");
	fx_ready (fx);

	feedlist_set_selected (fx->root);
	feed = node_source_add_subscription (fx->root, subscription_new (fx->feedUrl, NULL, NULL));
	g_assert_nonnull (feed);
	g_assert_true (feed->parent == fx->root);

	nodePath = g_strdup_printf (DAV_COL "%s/node.json", feed->id);
	statePath = g_strdup_printf (DAV_COL "%s/state.json", feed->id);
	fx->waitKey = nodePath;
	g_assert_true (wait_for (fs_has_wait_key, fx, 5));
	fx->waitKey = statePath;
	g_assert_true (wait_for (fs_has_wait_key, fx, 5));
	fx->needle = feed->id;
	g_assert_true (wait_for (index_has_needle, fx, 5));
	g_assert_nonnull (strstr (g_hash_table_lookup (fx->fs, nodePath), fx->feedUrl));
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
	g_test_add_func ("/webdav_source/remote-feed-added", tc_remote_feed_added);
	g_test_add_func ("/webdav_source/local-feed-added", tc_local_feed_added);
	g_test_add_func ("/webdav_source/remote-feed-moved", tc_remote_feed_moved);
	g_test_add_func ("/webdav_source/local-feed-moved", tc_local_feed_moved);
	g_test_add_func ("/webdav_source/remote-feed-removed", tc_remote_feed_removed);
	g_test_add_func ("/webdav_source/remote-folder-removed", tc_remote_folder_removed);
	g_test_add_func ("/webdav_source/local-feed-removed", tc_local_feed_removed);
	g_test_add_func ("/webdav_source/local-folder-removed", tc_local_folder_removed);

	return g_test_run ();
}
