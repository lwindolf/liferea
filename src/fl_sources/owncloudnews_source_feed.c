/**
 * Update each feed.
 * Pull new items and merge with the existing ones if any.
 */
#include "fl_sources/owncloudnews_source.h"

#include <glib.h>
#include <debug.h>
#include <metadata.h>
#include <feedlist.h>
#include <xml.h>
#include <itemlist.h>
#include <common.h>

#include "json.h"
#include "subscription_type.h"

static gboolean prepare_update_request (
	subscriptionPtr subscription, struct updateRequest *request)
{
	nodePtr root = node_source_root_from_node (subscription->node);
	owncloudnewsSourcePtr source = (owncloudnewsSourcePtr) root->data;
	const gchar *feed_id;

	debug0 (DEBUG_UPDATE, "ownCloud News preparing feed subscription for update.");

	feed_id = metadata_list_get (subscription->metadata, "owncloudnews-feed-id");
	if (!feed_id) {
		g_warning ("Dropping ownCloud News feed without id! (%s)", subscription->node->title);
		feedlist_node_removed (subscription->node);
		return FALSE;
	}

	// https://github.com/owncloud/news/wiki/Items-1.2#get-items
	gchar *source_uri = g_strdup_printf (
		OWNCLOUDNEWS_ITEMS_URL, source->root->subscription->source);
	// TODO: find a better way of adding HTTP GET data
	// TODO: don't pull read items in subsequent requests
	// TODO: paginate requests?
	gchar *query_parameter = g_strdup_printf (
		"feed=0&id=%s&getRead=true", feed_id);
	source_uri = g_strjoin ("?", source_uri, query_parameter);

	request->options = update_options_copy (subscription->updateOptions);
	update_request_set_source(request, source_uri);
	update_request_set_auth_value (
		request,
		metadata_list_get (root->subscription->metadata, "owncloudnews-auth-token")
	);
	g_free (source_uri);
	g_free (query_parameter);

	return TRUE;
}

static void process_update_result (
	subscriptionPtr subscription,
	const struct updateResult *const result,
	updateFlags flags
) {
	debug2 (
		DEBUG_UPDATE,
		"Processing update result for an ownCloud New subscription (%s)."
			" Raw data received from the server: %s",
		subscription->source, result->data
	);
	if (result->data && result->httpstatus == 200) {
		JsonParser	*parser = json_parser_new ();

		if (json_parser_load_from_data (parser, result->data, -1, NULL)) {
			JsonArray	*array = json_node_get_array (json_get_node (json_parser_get_root (parser), "items"));
			JsonNode	*attachments;
			GList		*elements = json_array_get_elements (array);
			GList		*iter = elements;
			GList		*items = NULL;
			/*
			   We expect to get something like this

			 {
				  "items": [
					{
					  "id": 3443,
					  "guid": "http://grulja.wordpress.com/?p=76",
					  "guidHash": "3059047a572cd9cd5d0bf645faffd077",
					  "url": "http://grulja.wordpress.com/2013/04/29/plasma-nm-after-the-solid-sprint/",
					  "title": "Plasma-nm after the solid sprint",
					  "author": "Jan Grulich (grulja)",
					  "pubDate": 1367270544,
					  "body": "<p>At first I have to say...</p>",
					  "enclosureMime": null,
					  "enclosureLink": null,
					  "feedId": 67,
					  "unread": true,
					  "starred": false,
					  "lastModified": 1367273003
					}, // etc
				  ]
				}
			 */


			while (iter) {
				JsonNode *node = (JsonNode *)iter->data;
				itemPtr item = item_new ();
				const gchar *content;
				gchar *xhtml;

				item->metadata = metadata_list_append (
					item->metadata,
					"owncloudnews-item-id",
					g_strdup_printf ("%i", json_get_int (node, "id"))
				);
				item_set_title (item, json_get_string (node, "title"));
				item_set_source (item, json_get_string (node, "guid"));
				item_set_id (item, json_get_string (node, "guidHash"));
				item->validGuid = TRUE;

				content = json_get_string (node, "body");
				xhtml = xhtml_extract_from_string (content, NULL);
				item_set_description (item, xhtml);
				xmlFree (xhtml);

				item->time = json_get_int (node, "lastModified");
				item->readStatus = !json_get_bool (node, "unread");
				item->flagStatus = json_get_bool (node, "starred");
				item->parentNodeId = g_strdup_printf("%i", json_get_int (node, "feedId"));

				// TODO: extract enclosures

				items = g_list_append (items, (gpointer)item);

				iter = g_list_next (iter);
			}

			g_list_free (elements);

			/* merge against feed cache */
			if (items) {
				itemSetPtr itemSet = node_get_itemset (subscription->node);
				subscription->node->newCount = itemset_merge_items (
					itemSet, items, FALSE, FALSE);
				itemlist_merge_itemset (itemSet);
				itemset_free (itemSet);
			}

			subscription->node->available = TRUE;
		} else {
			subscription->node->available = FALSE;

			g_string_append (
				((feedPtr)subscription->node->data)->parseErrors,
				_("Could not parse JSON returned by ownCloud News API!")
			);
		}

		g_object_unref (parser);
	} else {
		subscription->node->available = FALSE;
	}
}


struct subscriptionType owncloudnewsSourceFeedSubscriptionType = {
	prepare_update_request,
	process_update_result
};
