/**
 * @file comments.c comment feed handling
 *
 * Copyright (C) 2007-2026 Lars Windolf <lars.windolf@gmx.de>
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

#include "comments.h"
#include "common.h"
#include "db.h"
#include "debug.h"
#include "itemlist.h"
#include "json.h"
#include "node_providers/feed.h"
#include "metadata.h"
#include "net.h"
#include "node.h"
#include "update.h"

/* Comment feeds in Liferea are simple flat lists of items attached
   as metadata to a single item. They are persisted in the item metadata.

   Comment feeds are fetched once on demand when viewing an item. Once the
   comment feed fetch finished an item view reload presents the comments
   at the bottom.

   Comment feeds are not associated with any node, they are an ephemeral
   ad-hoc subscription.

   No special feed settings are supported for comments feeds.
 */

static gchar *
comments_to_json (GList *items)
{
	g_autoptr(JsonBuilder) b = json_builder_new ();

	json_builder_begin_object (b);
	json_builder_set_member_name (b, "items");
	json_builder_begin_array (b);
	for (GList *iter = items; iter; iter = g_list_next (iter)) {
		itemPtr item = (itemPtr)iter->data;
		json_builder_begin_object (b);
		json_builder_set_member_name (b, "title");
		json_builder_add_string_value (b, item->title);
		json_builder_set_member_name (b, "source");
		json_builder_add_string_value (b, item->source);
		// FIXME: add author from metadata
		json_builder_set_member_name (b, "time");
		json_builder_add_int_value (b, item->time);
		json_builder_set_member_name (b, "description");
		json_builder_add_string_value (b, item->description);
		json_builder_end_object (b);
	}
	json_builder_end_array (b);
	json_builder_set_member_name (b, "status");
	json_builder_add_string_value (b, "ok");
	json_builder_end_object (b);

	return json_dump (b);
}

static gboolean
comments_process_update_result (UpdateJob *job)
{
	UpdateResult		*result = job->result;
	feedParserCtxtPtr	ctxt;
	itemPtr			item;
	g_autofree gchar	*error = NULL;

	if (!(item = item_load (GPOINTER_TO_UINT (job->user_data))))
		return TRUE;	/* item was deleted since */

	if (304 == result->httpstatus) {
		g_warning ("Comment feed update returned 304 Not Modified, this should not happen!");
	} else if (result->data) {
		debug (DEBUG_UPDATE, "received update result for comment feed \"%s\"", result->source);

		/* parse the new downloaded feed using a ad-hoc subscription */
		subscriptionPtr subscription = subscription_new (result->source, NULL, NULL);
		ctxt = feed_parser_ctxt_new (subscription, result->data, result->size);

		if (!feed_parse (ctxt)) {
			// FIXME: use error from feed_parse() instead of generic error message
			error = g_strdup (ctxt->subscription->parseErrors->str);
		} else {
			debug (DEBUG_UPDATE, "parsing comment feed successful (%d comments downloaded)", g_list_length(ctxt->items));
			g_autofree gchar *json = comments_to_json (ctxt->items);
			metadata_list_set (&(item->metadata), "commentFeedJson", json);
			db_item_update (item);
			itemlist_update_item (item);
		}

		feed_parser_ctxt_free (ctxt);
		subscription_free (subscription);
	}

	if (!error && ((result->httpstatus < 200) || (result->httpstatus >= 400)))
		error = g_strdup (network_strerror (result->httpstatus));
	
	if (error) {
		debug (DEBUG_UPDATE, "comment feed update failed for item \"%s\" (comment URL: %s): %s", item->title, result->source, error);
		g_autofree gchar *json = g_strdup_printf("{ \"items\": [], \"status\": \"error\", \"error\": \"%s\" }", error);
		metadata_list_set (&(item->metadata), "commentFeedJson", json);
		db_item_update (item);
	}

	item_unload (item);

	return TRUE;
}

void
comments_refresh (itemPtr item)
{
	const gchar	*url, *json;

	json = metadata_list_get (item->metadata, "commentFeedJson");
	if (json)
		return;	/* we fetch only once */

	url = metadata_list_get (item->metadata, "commentFeedUri");
	if (!url)
		return;

	debug (DEBUG_UPDATE, "Updating comments for item \"%s\" (comment URL: %s)", item->title, url);

	// Set empty list to avoid multiple fetches
	metadata_list_set (&(item->metadata), "commentFeedJson", "{ \"items\": [], \"status\": \"fetching\" }");
	db_item_update (item);

	UpdateRequest *request = update_request_new (
		"GET",
		url,
		NULL,	// No special state
		NULL	// No special options
	);

	Node *node = node_from_id (item->nodeId);
	(void) update_job_new (
		node->subscription,
		request,
		comments_process_update_result,
		GUINT_TO_POINTER(item->id),
		UPDATE_REQUEST_PRIORITY_HIGH | UPDATE_REQUEST_NO_FEED
	);
}
