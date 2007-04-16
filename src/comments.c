/**
 * @file comments.c comment feed handling
 * 
 * Copyright (C) 2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <string.h>
#include "comments.h"
#include "common.h"
#include "db.h"
#include "debug.h"
#include "feed.h"
#include "itemview.h"
#include "metadata.h"
#include "support.h"
#include "update.h"

/* Comment feeds in Liferea are simple flat list of items attached
   to a single item. Each item that has a comment feed URL in its 
   metadata list gets its comment feed updates as soon as the user
   displays the item in 3 pane mode.
   
   Although rendered differently items and comment items are handled
   in the same way. */

static GHashTable	*commentFeeds = NULL;

typedef struct commentFeed 
{
	gulong		itemId;			/**< parent item id */
	gchar		*id;			/**< id of the items comments feed (or NULL) */
	gchar		*error;			/**< description of error if comments download failed (or NULL)*/

	struct request	*updateRequest;		/**< update request structure used when downloading comments */
	struct updateState *updateState;	/**< update states (etag, last modified, cookies, last polling times...) used when downloading comments */
} *commentFeedPtr;

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

/**
 * Update reques processing callback.
 */
static void
comments_process_update_result (struct request *request) 
{
	feedParserCtxtPtr	ctxt;
	commentFeedPtr		commentFeed = (commentFeedPtr)request->user_data;
	itemPtr			item;
	nodePtr			node;

	debug_enter ("comments_process_update_result");
	
g_print("updating result processing for item comments...\n");
	item = item_load (commentFeed->itemId);
	g_return_if_fail (item != NULL);
	
	/* note this is to update the feed URL on permanent redirects */
	if(!strcmp(request->source, metadata_list_get(item->metadata, "commentFeedUri"))) {
	
		debug2(DEBUG_UPDATE, "updating comment feed URL from \"%s\" to \"%s\"", 
		                     metadata_list_get(item->metadata, "commentFeedUri"), 
				     request->source);
				     
		metadata_list_set(&(item->metadata), "commentFeedUri", request->source);
	}
	
	if(401 == request->httpstatus) { /* unauthorized */
		commentFeed->error = g_strdup(_("Authorization Error"));
	} else if(410 == request->httpstatus) { /* gone */
		// FIXME: how to prevent further updates?
	} else if(304 == request->httpstatus) {
		debug1(DEBUG_UPDATE, "comment feed \"%s\" did not change", request->source);
	} else if(request->data) {
		debug1(DEBUG_UPDATE, "received update result for comment feed \"%s\"", request->source);

		/* parse the new downloaded feed into feed and itemSet */
		node = node_new();
		ctxt = feed_create_parser_ctxt();
		ctxt->subscription = subscription_new(request->source, NULL, NULL);
		ctxt->feed = feed_new();
		node_set_type(node, feed_get_node_type());
		node_set_data(node, ctxt->feed);		
		node_set_subscription(node, ctxt->subscription);
		ctxt->data = request->data;
		ctxt->dataLength = request->size;
		feed_parse(ctxt);

		if(ctxt->failed) {
			debug0(DEBUG_UPDATE, "parsing comment feed failed!");
		} else {
			itemSetPtr comments;
			
			debug1(DEBUG_UPDATE, "parsing comment feed successful (%d comments downloaded)", g_list_length(ctxt->items));		
			comments = db_itemset_load(commentFeed->id);
			itemset_merge_items(comments, ctxt->items);
			itemset_free(comments);
		}
				
		node_free(ctxt->subscription->node);
		subscription_free(ctxt->subscription);
		feed_free_parser_ctxt(ctxt);
	}
	
	/* update error message */
	g_free(commentFeed->error);
	commentFeed->error = NULL;
	
	if(!(request->httpstatus >= 200) && (request->httpstatus < 400)) {
		const gchar * tmp;
		
		/* first specific codes (guarantees tmp to be set) */
		tmp = common_http_error_to_str(request->httpstatus);

		/* second netio errors */
		if(common_netio_error_to_str(request->returncode))
			tmp = common_netio_error_to_str(request->returncode);
			
		commentFeed->error = g_strdup(tmp);
	}	

	/* clean up request */
	g_free(request->options);
	commentFeed->updateRequest = NULL; 

	/* rerender item */
	itemview_update_item(item); 
	itemview_update();
	
	item_unload (item);
		
	debug_exit("comments_process_update_result");
}

void
comments_refresh (itemPtr item) 
{ 
	commentFeedPtr	commentFeed;
	struct request	*request;
	const gchar	*url;
	
	url = metadata_list_get (item->metadata, "commentFeedUri");
	if (url) 
	{
g_print("updating item comments...\n");	
		debug2 (DEBUG_UPDATE, "Updating comments for item \"%s\" (comment URL: %s)", item->title, url);

		// FIXME: restore update state from DB?		
		commentFeed = comment_feed_from_id (item->commentFeedId);
		if (!commentFeed)
		{
			g_assert (NULL == item->commentFeedId);
			item->commentFeedId = node_new_id ();
				
			commentFeed = g_new0 (struct commentFeed, 1);
			commentFeed->id = g_strdup (item->commentFeedId);
			commentFeed->itemId = item->id;
			commentFeed->updateState = update_state_new ();

			if (!commentFeeds)
				commentFeeds = g_hash_table_new (g_str_hash, g_str_equal);
			g_hash_table_insert (commentFeeds, commentFeed->id, commentFeed);
		}

		request = update_request_new (commentFeed);
		request->user_data = commentFeed;
		request->options = g_new0 (struct updateOptions, 1);	// FIXME: use copy of parent subscription options
		request->callback = comments_process_update_result;
		request->source = g_strdup (url);
		request->priority = 1;
		update_execute_request (request);

		itemview_update_item (item); 
		itemview_update ();
	}
}

void
comments_to_xml (xmlNodePtr parentNode,
                 const gchar *id)
{
	xmlNodePtr	commentsNode;
	commentFeedPtr	commentFeed;
	itemSetPtr	itemSet;
	GList		*iter;

	commentsNode = xmlNewChild (parentNode, NULL, "comments", NULL);
	
	commentFeed = comment_feed_from_id (id);
	g_return_if_fail (commentFeed != NULL);
	
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
		
	xmlNewTextChild (commentsNode, NULL, "updateState", 
	                 (commentFeed->updateRequest)?"updating":"ok");
		
	if (commentFeed->error)
		xmlNewTextChild (commentsNode, NULL, "updateError", commentFeed->error);
			
	itemset_free (itemSet);
}

void
comments_remove (const gchar *id)
{
	commentFeedPtr commentFeed;

	db_itemset_remove_all (id);
	
	commentFeed = comment_feed_from_id (id);
	g_return_if_fail (commentFeed != NULL);
	
	if (commentFeed->updateRequest)
		update_cancel_requests (commentFeed);
	if (commentFeed->updateState)
		update_state_free (commentFeed->updateState);
	
	g_free (commentFeed->id);
	g_free (commentFeed);
}
