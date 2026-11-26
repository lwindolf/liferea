/**
 * @file theoldreader_source.c  Test cases for the TheOldReader node source
 *
 * TheOldReader is a one-way sync: the remote subscription list always wins.
 * So there are no upload tests, only login handling and merging of remote state.
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
#include "node_sources/theoldreader_source.h"
#include "subscription.h"
#include "update_job_queue.h"
#include "xml.h"
#include "tests/test_http_server.h"

#define AUTH_TOKEN	"testtoken"
#define EMPTY_OPML	"<?xml version=\"1.0\"?><opml version=\"1.0\"><head/><body>%s</body></opml>"

/* Test fixture: server + TheOldReader node source (client) */

typedef struct {
	TestHttpServer	*srv;
	Node		*root;
	GHashTable	*remote;	/* fake remote subscriptions: feed id -> folder label ("" for none) */
	gboolean	markRead;	/* mark all entries of all feeds as read */
} Fx;

/* Fake TheOldReader backend */

static void
login_cb (TestHttpServer *s, SoupServerMessage *msg, const gchar *path, gpointer data)
{
	SoupMessageBody *body = soup_server_message_get_request_body (msg);
	g_autofree gchar *post = g_strndup (body->data, body->length);
	const gchar *response = "SID=x\nLSID=y\nAuth=" AUTH_TOKEN "\n";

	if (!strstr (post, "Email=user&Passwd=pass")) {
		soup_server_message_set_status (msg, SOUP_STATUS_FORBIDDEN, NULL);
		soup_server_message_set_response (msg, "text/plain", SOUP_MEMORY_STATIC, "Error=BadAuthentication\n", 24);
		return;
	}
	soup_server_message_set_response (msg, "text/plain", SOUP_MEMORY_STATIC, response, strlen (response));
	soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
}

static gchar *
feed_url (Fx *fx, const gchar *id)
{
	g_autofree gchar *path = g_strdup_printf ("/site/%s", id);

	return test_http_server_get_url (fx->srv, path);
}

static gchar *
subscription_list (Fx *fx)
{
	GHashTableIter iter;
	gpointer id, label;
	GString *json = g_string_new ("{\"subscriptions\":[");

	g_hash_table_iter_init (&iter, fx->remote);
	while (g_hash_table_iter_next (&iter, &id, &label)) {
		g_autofree gchar *url = feed_url (fx, id);
		g_autofree gchar *categories = g_strdup_printf ("[{\"id\":\"user/-/label/%s\",\"label\":\"%s\"}]", (gchar *)label, (gchar *)label);

		g_string_append_printf (json, "%s{\"id\":\"feed/%s\",\"title\":\"Feed %s\",\"categories\":%s,\"url\":\"%s\"}",
			json->str[json->len - 1] == '[' ? "" : ",", (gchar *)id, (gchar *)id,
			*(gchar *)label ? categories : "[]", url);
	}
	g_string_append (json, "]}");
	return g_string_free (json, FALSE);
}

static void
reader_cb (TestHttpServer *s, SoupServerMessage *msg, const gchar *path, gpointer data)
{
	Fx *fx = data;
	const gchar *auth = soup_message_headers_get_one (soup_server_message_get_request_headers (msg), "Authorization");

	if (g_strcmp0 (auth, "GoogleLogin auth=" AUTH_TOKEN)) {
		soup_server_message_set_status (msg, SOUP_STATUS_UNAUTHORIZED, NULL);
		return;
	}

	if (strstr (path, "/subscription/list")) {
		g_autofree gchar *json = subscription_list (fx);

		soup_server_message_set_response (msg, "application/json", SOUP_MEMORY_COPY, json, strlen (json));
	} else if (g_str_has_prefix (path, "/reader/atom/feed/")) {
		const gchar *id = path + strlen ("/reader/atom/feed/");
		g_autofree gchar *atom = g_strdup_printf (
			"<?xml version=\"1.0\"?><feed xmlns=\"http://www.w3.org/2005/Atom\"><id>urn:%s</id><title>Feed %s</title>"
			"<updated>2026-01-01T00:00:00Z</updated><entry><id>urn:%s:1</id><title>One</title>"
			"<updated>2026-01-01T00:00:00Z</updated><link href=\"http://localhost/1\"/>%s</entry></feed>",
			id, id, id, fx->markRead ? "<category term=\"user/-/state/com.google/read\" label=\"read\"/>" : "");

		soup_server_message_set_response (msg, "application/atom+xml", SOUP_MEMORY_COPY, atom, strlen (atom));
	} else {
		soup_server_message_set_status (msg, SOUP_STATUS_NOT_FOUND, NULL);
		return;
	}
	soup_server_message_set_status (msg, SOUP_STATUS_OK, NULL);
}

static void
remote_set (Fx *fx, const gchar *id, const gchar *folder)
{
	g_hash_table_insert (fx->remote, g_strdup (id), g_strdup (folder ? folder : ""));
}

/* Test fixture */

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
	fx->remote = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	test_http_server_add_handler (fx->srv, "/accounts/", login_cb, fx);
	test_http_server_add_handler (fx->srv, "/reader/", reader_cb, fx);
	return fx;
}

// Creates the source like the "new source" dialog does and starts the login.
// The OPML outlines are the local feeds the source starts with.
static void
fx_start (Fx *fx, const gchar *password, const gchar *outlines)
{
	nodeSourceTypePtr type = theoldreader_source_get_type ();
	g_autofree gchar *server = test_http_server_get_url (fx->srv, "");
	g_autofree gchar *url = g_strconcat (server, "/reader", NULL);
	g_autofree gchar *opml = g_strdup_printf (EMPTY_OPML, outlines);

	// Talk to the test server instead of theoldreader.com
	type->serverUrl = g_steal_pointer (&server);

	fx->root = node_new ("source");

	g_autofree gchar *basename = g_strdup_printf ("feedlist_%s.opml", fx->root->id);
	g_autofree gchar *filename = common_create_data_filename (basename);
	g_file_set_contents (filename, opml, -1, NULL);

	node_source_new (fx->root, type, url);
	node_set_title (fx->root, "test");
	subscription_set_auth_info (fx->root->subscription, "user", password);
	feedlist_node_added (fx->root);
	node_source_update (fx->root, 0);
}

static void
fx_free (Fx *fx)
{
	// Folder node ids are only unique per source type, so leftovers would be reused by the next test
	if (fx->root)
		feedlist_node_removed (fx->root);
	test_http_server_free (fx->srv);
	g_hash_table_destroy (fx->remote);
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

// Waits until the client stopped talking to the server, so a following update is not skipped as "already being updated"
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

static Node *
find_feed (Fx *fx, const gchar *id)
{
	g_autofree gchar *url = feed_url (fx, id);

	return feedlist_find_node (fx->root, NODE_BY_URL, url);
}

static gboolean
feed1_exists (Fx *fx)
{
	return find_feed (fx, "feed1") != NULL;
}

static gboolean
feed1_gone (Fx *fx)
{
	return !feed1_exists (fx);
}

static gboolean
feed1_has_items (Fx *fx)
{
	Node *feed = find_feed (fx, "feed1");

	return feed && db_itemset_get_item_count (feed->id) > 0;
}

static gboolean
feed1_has_unread (Fx *fx)
{
	Node *feed = find_feed (fx, "feed1");

	return feed && db_itemset_get_unread_count (feed->id) > 0;
}

static gboolean
feed1_all_read (Fx *fx)
{
	return feed1_has_items (fx) && !feed1_has_unread (fx);
}

static gboolean
feed1_in_myfolder (Fx *fx)
{
	Node *feed = find_feed (fx, "feed1");

	return feed && IS_FOLDER (feed->parent) && !g_strcmp0 (feed->parent->title, "myfolder");
}

static gboolean
feed1_in_otherfolder (Fx *fx)
{
	Node *feed = find_feed (fx, "feed1");

	return feed && IS_FOLDER (feed->parent) && !g_strcmp0 (feed->parent->title, "otherfolder");
}

static gboolean
feed1_at_root (Fx *fx)
{
	Node *feed = find_feed (fx, "feed1");

	return feed && feed->parent == fx->root;
}

// Logs in, imports the remote feed "feed1" and waits for it to be fetched
static void
fx_synced (Fx *fx, const gchar *folder)
{
	remote_set (fx, "feed1", folder);
	fx_start (fx, "pass", "");
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_ACTIVE);
	g_assert_true (wait_for (feed1_has_items, fx, 10));
	wait_idle (fx);
}

/* Test cases */

static void
tc_login (void)
{
	Fx *fx = fx_new ();

	fx_start (fx, "pass", "");
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_ACTIVE);
	g_assert_cmpstr (fx->root->source->authToken, ==, "GoogleLogin auth=" AUTH_TOKEN);
	g_assert_cmpuint (test_http_server_request_count (fx->srv, "POST /accounts/ClientLogin"), ==, 1);

	// An empty remote list is fine
	g_assert_true (fx->root->available);
	g_assert_null (fx->root->children);
	fx_free (fx);
}

static void
tc_wrong_password (void)
{
	Fx *fx = fx_new ();

	remote_set (fx, "feed1", NULL);
	fx_start (fx, "wrong", "");
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_AUTH_FAILED);
	g_assert_false (fx->root->available);
	g_assert_null (fx->root->source->authToken);
	g_assert_cmpuint (test_http_server_request_count (fx->srv, "GET /reader/"), ==, 0);
	g_assert_null (find_feed (fx, "feed1"));
	fx_free (fx);
}

static void
tc_remote_feeds_imported (void)
{
	Fx *fx = fx_new ();

	fx_synced (fx, "myfolder");
	g_assert_true (feed1_in_myfolder (fx));
	g_assert_cmpstr (find_feed (fx, "feed1")->title, ==, "Feed feed1");
	g_assert_cmpuint (db_itemset_get_item_count (find_feed (fx, "feed1")->id), ==, 1);
	g_assert_true (feed1_has_unread (fx));
	fx_free (fx);
}

static void
tc_remote_feed_without_folder (void)
{
	Fx *fx = fx_new ();

	fx_synced (fx, NULL);
	g_assert_true (feed1_at_root (fx));
	fx_free (fx);
}

static void
tc_subscription_list_fails (void)
{
	Fx *fx = fx_new ();

	remote_set (fx, "feed1", NULL);
	test_http_server_set_fail (fx->srv, "GET", "/reader/api/0//subscription/list", 500);
	fx_start (fx, "pass", "");
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_cmpint (fx->root->source->loginState, ==, NODE_SOURCE_STATE_ACTIVE);
	g_assert_true (wait_for (feed1_gone, fx, 1));
	g_assert_null (find_feed (fx, "feed1"));
	fx_free (fx);
}

// Remote read state wins over the local one
static void
tc_remote_read_state (void)
{
	Fx *fx = fx_new ();

	fx->markRead = TRUE;
	fx_synced (fx, NULL);
	g_assert_true (wait_for (feed1_all_read, fx, 5));
	fx_free (fx);
}

static void
tc_remote_feed_added (void)
{
	Fx *fx = fx_new ();

	fx_start (fx, "pass", "");
	g_assert_true (wait_for (login_done, fx, 5));
	wait_idle (fx);
	g_assert_null (find_feed (fx, "feed1"));

	remote_set (fx, "feed1", "myfolder");
	node_source_update (fx->root, 0);
	g_assert_true (wait_for (feed1_has_items, fx, 10));
	g_assert_true (feed1_in_myfolder (fx));
	fx_free (fx);
}

static void
tc_remote_feed_moved (void)
{
	Fx *fx = fx_new ();

	fx_synced (fx, "myfolder");
	g_assert_true (feed1_in_myfolder (fx));

	remote_set (fx, "feed1", "otherfolder");
	node_source_update (fx->root, 0);
	g_assert_true (wait_for (feed1_in_otherfolder, fx, 5));

	// Moved out of the last folder, so it ends up in the source root
	remote_set (fx, "feed1", NULL);
	node_source_update (fx->root, 0);
	g_assert_true (wait_for (feed1_at_root, fx, 5));
	fx_free (fx);
}

// Folders that lost their last feed are dropped together with it
static void
tc_remote_feed_removed (void)
{
	Fx *fx = fx_new ();

	fx_synced (fx, "myfolder");

	g_hash_table_remove (fx->remote, "feed1");
	remote_set (fx, "feed2", NULL);
	node_source_update (fx->root, 0);

	g_assert_true (wait_for (feed1_gone, fx, 5));
	g_assert_nonnull (find_feed (fx, "feed2"));
	g_assert_null (feedlist_find_node (fx->root, NODE_BY_URL, "myfolder"));
	fx_free (fx);
}

// There is no upload: a local feed unknown to the remote side is dropped, not pushed
static void
tc_local_feed_dropped (void)
{
	Fx *fx = fx_new ();
	g_autofree gchar *url = feed_url (fx, "localfeed");
	g_autofree gchar *outline = g_strdup_printf ("<outline id=\"localfeed\" title=\"Local\" xmlUrl=\"%s\"/>", url);

	remote_set (fx, "feed1", NULL);
	fx_start (fx, "pass", outline);
	g_assert_true (wait_for (login_done, fx, 5));
	g_assert_true (wait_for (feed1_has_items, fx, 10));
	g_assert_null (find_feed (fx, "localfeed"));
	g_assert_cmpuint (test_http_server_request_count (fx->srv, "POST /reader/"), ==, 0);
	fx_free (fx);
}

int
test_theoldreader_source (int argc, char *argv[])
{
	g_test_init (&argc, &argv, NULL);

	if (g_strv_contains ((const gchar **)argv, "--debug"))
		debug_set_flags (DEBUG_UPDATE | DEBUG_NET);

	g_test_add_func ("/theoldreader_source/login", tc_login);
	g_test_add_func ("/theoldreader_source/wrong-password", tc_wrong_password);
	g_test_add_func ("/theoldreader_source/remote-feeds-imported", tc_remote_feeds_imported);
	g_test_add_func ("/theoldreader_source/remote-feed-without-folder", tc_remote_feed_without_folder);
	g_test_add_func ("/theoldreader_source/subscription-list-fails", tc_subscription_list_fails);
	g_test_add_func ("/theoldreader_source/remote-read-state", tc_remote_read_state);
	g_test_add_func ("/theoldreader_source/remote-feed-added", tc_remote_feed_added);
	g_test_add_func ("/theoldreader_source/remote-feed-moved", tc_remote_feed_moved);
	g_test_add_func ("/theoldreader_source/remote-feed-removed", tc_remote_feed_removed);
	g_test_add_func ("/theoldreader_source/local-feed-dropped", tc_local_feed_dropped);

	return g_test_run ();
}
