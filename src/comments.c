/**
 * @file comments.c comment feed handling
 *
 * Copyright (C) 2007-2020 Lars Windolf <lars.windolf@gmx.de>
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
#include "feed.h"
#include "metadata.h"
#include "net.h"
#include "net_monitor.h"
#include "update.h"
#include "ui/itemview.h"

/* Comment feeds in Liferea are simple flat lists of items attached
   to a single item. Each item that has a comment feed URL in its
   metadata list gets its comment feed updated as soon as the user
   triggers rendering of the item in 3 pane mode.

   Although rendered differently items and comment items are handled
   in the same way. */

static GHashTable	*commentFeeds = NULL;

typedef struct commentFeed
{
	gulong		itemId;			/**< parent item id */
	gchar		*id;			/**< id of the items comments feed (or NULL) */
	gchar		*error;			/**< description of error if comments download failed (or NULL)*/

	struct updateJob *updateJob;		/**< update job structure used when downloading comments */
	updateStatePtr	updateState;		/**< update states (etag, last modified, cookies, last polling times...) used when downloading comments */
} *commentFeedPtr;

static void
comment_feed_free (commentFeedPtr commentFeed)
{
	if (commentFeed->updateJob)
		update_job_cancel_by_owner (commentFeed);
	if (commentFeed->updateState)
		update_state_free (commentFeed->updateState);

	g_free (commentFeed->error);
	g_free (commentFeed->id);
	g_free (commentFeed);
}

static void
comment_feed_free_cb (gpointer key, gpointer value, gpointer user_data)
{
	comment_feed_free (value);
}

void
comments_deinit (void)
{
	if (commentFeeds) {
		g_hash_table_foreach (commentFeeds, comment_feed_free_cb, NULL);
		g_hash_table_destroy (commentFeeds);
		commentFeeds = NULL;
	}
}

/**
 * Hash lookup to find comment feeds with the given id.
 * Returns the comment feed (or NULL).
 */
static commentFeedPtr
comment_feed_from_id (const gchar *id)
{
	if (!commentFeeds)
		return NULL;

	return (commentFeedPtr) g_hash_table_lookup (commentFeeds, id);
}

static void
comments_process_update_result (const struct updateResult * const result, gpointer user_data, updateFlags flags)
{
	feedParserCtxtPtr	ctxt;
	commentFeedPtr		commentFeed = (commentFeedPtr)user_data;
	itemPtr			item;
	nodePtr			node;

	debug_enter ("comments_process_update_result");

	if(!(item = item_load (commentFeed->itemId)))
		return;		/* item was deleted since */

	/* note this is to update the feed URL on permanent redirects */
	if (result->source && !g_strcmp0 (result->source, metadata_list_get (item->metadata, "commentFeedUri"))) {

		debug2 (DEBUG_UPDATE, "updating comment feed URL from \"%s\" to \"%s\"",
		                      metadata_list_get (item->metadata, "commentFeedUri"),
				      result->source);

		metadata_list_set (&(item->metadata), "commentFeedUri", result->source);
	}

	if (401 == result->httpstatus) { /* unauthorized */
		commentFeed->error = g_strdup (_("Authorization Error"));
	} else if (410 == result->httpstatus) { /* gone */
		metadata_list_set (&item->metadata, "commentFeedGone", "true");
	} else if (304 == result->httpstatus) {
		debug1(DEBUG_UPDATE, "comment feed \"%s\" did not change", result->source);
	} else if (result->data) {
		debug1(DEBUG_UPDATE, "received update result for comment feed \"%s\"", result->source);

		/* parse the new downloaded feed into fake node, subscription and feed */
		node = node_new (feed_get_node_type ());
		node_set_data (node, feed_new ());
		node_set_subscription (node, subscription_new (result->source, NULL, NULL));
		ctxt = feed_parser_ctxt_new (node->subscription, result->data, result->size);

		if (!feed_parse (ctxt)) {
			debug0 (DEBUG_UPDATE, "parsing comment feed failed!");
		} else {
			itemSetPtr	comments;
			GList		*iter;

			/* before merging mark all downloaded items as comments */
			iter = ctxt->items;
			while (iter) {
				itemPtr comment = (itemPtr) iter->data;
				comment->isComment = TRUE;
				comment->parentItemId = commentFeed->itemId;
				comment->parentNodeId = g_strdup (item->nodeId);
				iter = g_list_next (iter);
			}

			debug1 (DEBUG_UPDATE, "parsing comment feed successful (%d comments downloaded)", g_list_length(ctxt->items));
			comments = db_itemset_load (commentFeed->id);
			itemset_merge_items (comments, ctxt->items, ctxt->feed->valid, FALSE);
			itemset_free (comments);

			/* No comment feed truncating as comment items are automatically
			   dropped when the parent items are removed from cache. */
		}

		node_free (ctxt->subscription->node);
		feed_parser_ctxt_free (ctxt);
	}

	/* update error message */
	g_free (commentFeed->error);
	commentFeed->error = NULL;

	if ((result->httpstatus < 200) || (result->httpstatus >= 400))
		commentFeed->error = g_strdup (network_strerror (result->httpstatus));

	/* clean up... */
	commentFeed->updateJob = NULL;
	update_state_free (commentFeed->updateState);
	commentFeed->updateState = update_state_copy (result->updateState);

	/* rerender item with new comments */
	itemview_update_item (item);
	itemview_update ();

	item_unload (item);

	debug_exit ("comments_process_update_result");
}

void
comments_refresh (itemPtr item)
{
	commentFeedPtr	commentFeed = NULL;
	UpdateRequest	*request;
	const gchar	*url;

	if (!network_monitor_is_online ())
		return;

	if (metadata_list_get (item->metadata, "commentFeedGone")) {
		debug0 (DEBUG_UPDATE, "Comment feed returned HTTP 410. Not updating anymore!");
		return;
	}

	url = metadata_list_get (item->metadata, "commentFeedUri");
	if (url) {
		debug2 (DEBUG_UPDATE, "Updating comments for item \"%s\" (comment URL: %s)", item->title, url);

		// FIXME: restore update state from DB?

		if (item->commentFeedId) {
			commentFeed = comment_feed_from_id (item->commentFeedId);
		} else {
			item->commentFeedId = node_new_id ();
			db_item_update (item);
		}

		if (!commentFeed) {
			commentFeed = g_new0 (struct commentFeed, 1);
			commentFeed->id = g_strdup (item->commentFeedId);
			commentFeed->itemId = item->id;

			if (!commentFeeds)
				commentFeeds = g_hash_table_new (g_str_hash, g_str_equal);
			g_hash_table_insert (commentFeeds, commentFeed->id, commentFeed);
		}

		request = update_request_new (
			url,
			commentFeed->updateState,
			NULL	// FIXME: use copy of parent subscription options
		);

		commentFeed->updateJob = update_execute_request (commentFeed, request, comments_process_update_result, commentFeed, FEED_REQ_PRIORITY_HIGH | FEED_REQ_NO_FEED);

		/* Item view refresh to change link from "Update" to "Updating..." */
		itemview_update_item (item);
		itemview_update ();
	}
}

void
comments_to_xml (xmlNodePtr parentNode, const gchar *id)
{
	xmlNodePtr	commentsNode;
	commentFeedPtr	commentFeed;
	itemSetPtr	itemSet;
	GList		*iter;

	commentFeed = comment_feed_from_id (id);
	if (!commentFeed)
		return;

	commentsNode = xmlNewChild (parentNode, NULL, BAD_CAST "comments", NULL);

	itemSet = db_itemset_load (id);
	g_return_if_fail (itemSet != NULL);

	iter = itemSet->ids;
	while (iter)
	{
		itemPtr comment = item_load (GPOINTER_TO_UINT (iter->data));
		item_to_xml (comment, commentsNode);
		item_unload (comment);
		iter = g_list_next (iter);
	}

	xmlNewTextChild (commentsNode, NULL, BAD_CAST "updateState", BAD_CAST ((commentFeed->updateJob)?"updating":"ok"));

	if (commentFeed->error)
		xmlNewTextChild (commentsNode, NULL, BAD_CAST "updateError", BAD_CAST commentFeed->error);

	itemset_free (itemSet);
}
