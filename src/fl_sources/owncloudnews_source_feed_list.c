#include <glib.h>

#include "db.h"
#include "debug.h"
#include "feed.h"
#include "feedlist.h"
#include "folder.h"
#include "json.h"
#include "metadata.h"
#include "opml_source.h"
#include "subscription_type.h"
#include "fl_sources/owncloudnews_source.h"
#include "fl_sources/owncloudnews_source_feed_list.h"

static gboolean prepare_update_request (
	subscriptionPtr subscription, struct updateRequest *request)
{
	debug0(DEBUG_UPDATE, "Preparing the update request for ownCloud News "
		"source feed list.");
	debug1 (DEBUG_UPDATE, "ownCloud News updating subscription (node id %s)",
	        subscription->node->id);

	gchar *status_uri = g_strdup_printf (OWNCLOUDNEWS_STATUS_URL,
	                                     subscription->source);
	update_request_set_source (request, status_uri);
	update_request_set_auth_value (
		request,
		metadata_list_get (subscription->metadata, "owncloudnews-auth-token" )
	);
	g_free (status_uri);
	return TRUE;
}

static void process_update_result (subscriptionPtr subscription,
                                   const struct updateResult * const result,
                                   updateFlags flags)
{
	debug1 (
		DEBUG_UPDATE,
		"Processing update result for ownCloud News. Raw data received from"
			" the server: %s", result->data
	);

	if (result->data && result->httpstatus == 200) {
		JsonParser *parser = json_parser_new ();
		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			/**
			 * We're expecting something like this:
			 *  {
			 * 		"version":"6.0.5",
			 * 		"warnings":{
			 * 			"improperlyConfiguredCron": false
			 * 		}
			 * 	}
			 * If "improperlyConfiguredCron" is `true`, we must inform the user
			 * about it and stop further parsing.
			 */
			JsonNode *warnings = json_get_node (
				json_parser_get_root (parser), "warnings");
			JsonNode *improperlyConfiguredCron = json_get_node (
				warnings, "improperlyConfiguredCron");
			GValue cronError = {};
			json_node_get_value (improperlyConfiguredCron, &cronError);

			if (g_value_get_boolean (&cronError) == TRUE) {
				debug0 (
					DEBUG_UPDATE,
					"Processing update result for ownCloud News. "
						"The News App updater is improperly configured. "
						"The server returned `improperlyConfiguredCron` warning."
				);

				/**
				 * TODO: Show the user the following message:
				 * The News App updater is improperly configured and
				 * you will lose updates.
				 * See http://DOMAIN/index.php/apps/news for
				 * instructions on how to fix it.
				 */
				subscription->node->available = FALSE;
			} else {
				// We're good to go - fetch folders now.
				// TODO: make simultaneous requests get folders and feeds and
				//       then do processing, rather than fetching folders first
				//       and then fetching feeds.
				update_folders (subscription);
			}
		} else {
			// TODO Show an error message to the user
			subscription->node->available = FALSE;
		}
		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
		// FIXME: show an error message to the user
		debug0 (
			DEBUG_UPDATE,
			"Processing update result for ownCloud News. "
				"Failed to get the News app status! "
				"Either the result doesn't have data or the http status was "
				"not 200."
		);
	}
}

static void update_folders (subscriptionPtr subscription)
{
	updateRequestPtr request = update_request_new ();
	gchar *source_uri;

	request->options = update_options_copy (subscription->updateOptions);

	// update folders
	source_uri = g_strdup_printf (
		OWNCLOUDNEWS_FOLDERS_URL, subscription->source);
	update_request_set_source(request, source_uri);
	update_request_set_auth_value (
		request,
		metadata_list_get (subscription->metadata, "owncloudnews-auth-token" )
	);
	subscription->updateJob = update_execute_request(
		subscription, request, update_folders_cb,
		subscription, 0);

	g_free(source_uri);
}

static void update_folders_cb (
	const struct updateResult * const result,
	gpointer user_data, guint32 flags)
{
	subscriptionPtr subscription = (subscriptionPtr) user_data;
	owncloudnewsSourcePtr source = (owncloudnewsSourcePtr) subscription->node->data;

	debug1 (DEBUG_UPDATE,
			"Processing update result for ownCloud News. "
				"Raw data for folders: %s", result->data);

	if (result->data && result->httpstatus == 200) {
		if (owncloudnews_source_create_folder_from_api_response (source, result->data) == TRUE) {
			// TODO: remove old folders
//			node_foreach_child_data (
//				source->root, check_node_for_removal, array);

			// Save new subscription tree to OPML cache file
			opml_source_export (subscription->node);
			subscription->node->available = TRUE;

			// update feeds
			updateRequestPtr request = update_request_new ();
			gchar *source_uri = g_strdup_printf (
				OWNCLOUDNEWS_FEEDS_URL, source->root->subscription->source);

			request->options = update_options_copy (subscription->updateOptions);
			update_request_set_source(request, source_uri);
			update_request_set_auth_value (
				request,
				metadata_list_get (subscription->metadata, "owncloudnews-auth-token" )
			);
			subscription->updateJob = update_execute_request(
				subscription, request, update_feeds_cb, subscription, 0);
		} else {
			subscription->node->available = FALSE;
		}
	}
}

static void update_feeds_cb (
	const struct updateResult * const result,
	gpointer user_data, guint32 flags)
{
	subscriptionPtr subscription = (subscriptionPtr) user_data;
	owncloudnewsSourcePtr source = (owncloudnewsSourcePtr) subscription->node->data;

	debug1 (DEBUG_UPDATE, "update_feeds_cb(): %s", result->data);

	subscription->updateJob = NULL;

	if (result->data && result->httpstatus == 200) {
		JsonParser *parser = json_parser_new();
		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonNode	*feeds = json_get_node (json_parser_get_root (parser), "feeds");
			JsonArray	*array;
			GList		*iter, *elements;
			/**
			 * We are expecting something like this (if no folder, then the folderId will be 0):
			 * {
			 * "feeds": [
			 * 	{
			 * 		"added": 1429825273,
			 * 		"faviconLink: "",
			 * 		"folderId": 7,
			 * 		"id": 1,
			 * 		"link": "http://blog.ometer.com/",
			 * 		"ordering": 0,
			 * 		"pinned": false,
			 * 		"title": "Havoc's Blog",
			 * 		"unreadCount": 1,
			 * 		"url": "http://blog.ometer.com/feed"
			 * 	},
			 * 	...
			 * ],
			 * "newestItemId": 123,
			 * "starredCount": 1
			 * }
			 */
			// TODO: keep track of the newest item and starred count?
			if (!feeds || (JSON_NODE_TYPE (feeds) != JSON_NODE_ARRAY)) {
				debug0 (DEBUG_UPDATE, "update_feeds_cb(): Failed to get the feed list!");
				subscription->node->available = FALSE;
				return;
			}

			array = json_node_get_array (feeds);
			elements = iter = json_array_get_elements (array);
			while (iter) {
				JsonNode *node = (JsonNode *)iter->data;
				gint folder_id = (gint) json_get_int (node, "folderId");
				debug1(DEBUG_UPDATE, "***********FOLDER ID %d", folder_id);
				gchar *folder_id_str = NULL;

				if (folder_id > 0)
					folder_id_str = g_strdup_printf ("%d", folder_id);

				debug1(DEBUG_UPDATE, "------------FOLDER ID %s", folder_id_str);

				merge_feed (
					source,
					json_get_string (node, "url"),
					json_get_string (node, "title"),
					json_get_int (node, "id"),
					// NULL because all folder should already be present
					node_source_find_or_create_folder (
						source->root, folder_id_str, NULL)
				);
				iter = g_list_next (iter);
			}
			g_list_free (elements);

			// TODO: remove old feeds

			opml_source_export (subscription->node);
			subscription->node->available = TRUE;

			// TODO fetch all unread articles if this is initial sync?
		}
	}
}

static void merge_feed (
	owncloudnewsSourcePtr source, const gchar *url, const gchar *title,
	gint64 id, nodePtr folder
) {
	nodePtr		node;
	gchar		*feed_id;

	// check if node to be merged already exists
	node = feedlist_find_node (source->root, NODE_BY_URL, url);

	if (!node) {
		debug2 (DEBUG_UPDATE, "adding %s (%s)", title, url);
		node = node_new (feed_get_node_type ());
		node_set_title (node, title);
		node_set_data (node, feed_new ());

		node_set_subscription (node, subscription_new (url, NULL, NULL));
		node->subscription->type = source->root->source->type->feedSubscriptionType;

		feed_id = g_strdup_printf ("%" G_GINT64_FORMAT, id);
		metadata_list_set (
			&node->subscription->metadata,
						   "owncloudnews-feed-id", feed_id);
		g_free (feed_id);

		node_set_parent (node, folder ? folder:source->root, -1);
		feedlist_node_imported (node);

		subscription_update (node->subscription,
							 FEED_REQ_RESET_TITLE | FEED_REQ_PRIORITY_HIGH);
		subscription_update_favicon (node->subscription);

		// Important: we must not loose the feed id!
		db_subscription_update (node->subscription);
	} else {
		node_source_update_folder (node, folder);
	}
}

struct subscriptionType owncloudnewsSourceSubscriptionType = {
	prepare_update_request,
	process_update_result
};
