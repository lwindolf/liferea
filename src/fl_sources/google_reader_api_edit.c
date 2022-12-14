/**
 * @file google_reader_api_edit.c  Google Reader API syncing support
 *
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2014-2015 Lars Windolf <lars.windolf@gmx.de>
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

#include "google_reader_api_edit.h"

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "json.h"
#include "update.h"
#include "subscription.h"

/**
 * A structure to indicate an edit to the node source remote feed "database".
 * These edits are put in a queue and processed in sequential order
 * so that the API endpoint does not end up processing the requests in an
 * unintended order.
 */
typedef struct GoogleReaderAction {
	/**
	 * The guid of the item to edit. This will be ignored if the
	 * edit is acting on an subscription rather than an item.
	 */
	gchar* guid;

	/**
	 * A MANDATORY feed url to containing the item, or the url of the
	 * subscription to edit.
	 *
	 * Note: google_reader_api_edit_remove_subscription sets feedUrl to the
	 * streamId of the item, e.g. feed/<url> in the case of Reedah or
	 * feed/<objectId> in the case of TheOldReader.
	 */
	gchar* feedUrl;

	/**
	 * The source type. Currently known types are "feed" and "user".
	 * "user" sources are used, for example, for items that are links (as
	 * opposed to posts) in broadcast-friends. The unique id of the source
	 * is of the form <feedUrlType>/<feedUrl>.
	 */
	gchar* feedUrlType;

	/**
         * An optional label id to use for an action (e.g. to add a feed label)
         * Must be of syntax "user/-/label/MyLabel"
         */
	gchar* label;

	/**
	 * A callback function on completion of the edit.
	 */
	void (*callback) (nodeSourcePtr source, struct GoogleReaderAction* edit, gboolean success);

	/**
	 * Get the streamId for the given node.
	 */
	gchar* (*get_stream_id_for_node) (nodePtr node);

	/**
	 * The type of this GoogleReaderAction.
	 */
	int actionType;

	/**
	 * The node source this action runs against (mandatory).
	 */
	nodeSourcePtr source;

	/**
	 * The action result data (available on callback)
	 */
	gchar *response;
} *GoogleReaderActionPtr;

enum {
	EDIT_ACTION_MARK_READ,
	EDIT_ACTION_MARK_UNREAD,
	EDIT_ACTION_TRACKING_MARK_UNREAD, /**< every UNREAD request, should be followed by tracking-kept-unread */
	EDIT_ACTION_MARK_STARRED,
	EDIT_ACTION_MARK_UNSTARRED,
	EDIT_ACTION_ADD_SUBSCRIPTION,
	EDIT_ACTION_REMOVE_SUBSCRIPTION,
	EDIT_ACTION_ADD_LABEL,
	EDIT_ACTION_REMOVE_LABEL
};

typedef struct GoogleReaderAction* editPtr;

typedef struct GoogleReaderActionCtxt {
	gchar			*nodeId;
	GoogleReaderActionPtr	action;
} *GoogleReaderActionCtxtPtr;

static void google_reader_api_edit_push (nodeSourcePtr source, GoogleReaderActionPtr action, gboolean head);

static GoogleReaderActionPtr
google_reader_api_action_new (int actionType)
{
	GoogleReaderActionPtr action = g_slice_new0 (struct GoogleReaderAction);
	action->actionType = actionType;
	return action;
}

static void
google_reader_api_action_free (GoogleReaderActionPtr action)
{
	g_free (action->guid);
	g_free (action->feedUrl);
	g_free (action->label);
	g_slice_free (struct GoogleReaderAction, action);
}

static GoogleReaderActionCtxtPtr
google_reader_api_action_context_new(nodeSourcePtr source, GoogleReaderActionPtr action)
{
	GoogleReaderActionCtxtPtr ctxt = g_slice_new0(struct GoogleReaderActionCtxt);
	ctxt->nodeId = g_strdup(source->root->id);
	ctxt->action = action;
	return ctxt;
}

static void
google_reader_api_action_context_free(GoogleReaderActionCtxtPtr ctxt)
{
	g_free(ctxt->nodeId);
	g_slice_free(struct GoogleReaderActionCtxt, ctxt);
}

static void
google_reader_api_edit_action_complete (const struct updateResult* const result, gpointer userdata, updateFlags flags)
{
	GoogleReaderActionCtxtPtr	editCtxt = (GoogleReaderActionCtxtPtr) userdata;
	GoogleReaderActionPtr		action = editCtxt->action;
	nodePtr				node = node_from_id (editCtxt->nodeId);
	gboolean			failed = FALSE;

	google_reader_api_action_context_free (editCtxt);

	if (!node) {
		google_reader_api_action_free (action);
		return; /* probably got deleted before this callback */
	}

	if (result->data == NULL) {
		failed = TRUE;
	} else {
		// FIXME: suboptimal check as some results are text, some XML, some JSON...
		if (!g_str_equal (result->data, "OK")) {
			if (node->source->api.json) {
				JsonParser *parser = json_parser_new ();

				if (!json_parser_load_from_data (parser, result->data, -1, NULL)) {
					debug0 (DEBUG_UPDATE, "Failed to parse JSON update");
					failed = TRUE;
				} else {
					const gchar *error = json_get_string (json_parser_get_root (parser), "error");
					if (error) {
						debug1 (DEBUG_UPDATE, "Request failed with error '%s'", error);
						failed = TRUE;
					}
				}
				// FIXME: also check for "errors" array

				g_object_unref (parser);
			} else {
				failed = TRUE;
			}
		}
	}

	if (action->callback) {
		action->response = result->data;
		action->callback (node->source, action, !failed);
	}

	google_reader_api_action_free (action);

	if (failed) {
		debug1 (DEBUG_UPDATE, "The edit action failed with result: %s\n", result->data);
		return; /** @todo start a timer for next processing */
	}

	/* process anything else waiting on the edit queue */
	google_reader_api_edit_process (node->source);
}

/* the following google_reader_api_* functions are simply functions that
   convert a GoogleReaderActionPtr to a UpdateRequest */

static void
google_reader_api_add_subscription (GoogleReaderActionPtr action, UpdateRequest *request, const gchar* token)
{
	update_request_set_source (request, action->source->api.add_subscription);
	gchar *s_escaped = g_uri_escape_string (action->feedUrl, NULL, TRUE);
	request->postdata = g_strdup_printf (action->source->api.add_subscription_post, s_escaped, token);
	g_free (s_escaped);
}

static void
google_reader_api_remove_subscription (GoogleReaderActionPtr action, UpdateRequest *request, const gchar* token)
{
	update_request_set_source (request, action->source->api.remove_subscription);
	gchar* s_escaped = g_uri_escape_string (action->feedUrl, NULL, TRUE);
	g_assert (!request->postdata);
	request->postdata = g_strdup_printf (action->source->api.remove_subscription_post, s_escaped, token);
	g_free (s_escaped);
}

static void
google_reader_api_edit_label (GoogleReaderActionPtr action, UpdateRequest *request, const gchar* token)
{
	update_request_set_source (request, action->source->api.edit_label);
	gchar* s_escaped = g_uri_escape_string (action->feedUrl, NULL, TRUE);
	gchar *a_escaped = g_uri_escape_string (action->label, NULL, TRUE);
	if (action->actionType == EDIT_ACTION_ADD_LABEL) {
		request->postdata = g_strdup_printf (action->source->api.edit_add_label_post, s_escaped, a_escaped, token);
	} else {
		request->postdata = g_strdup_printf (action->source->api.edit_remove_label_post, s_escaped, a_escaped, token);
	}
	g_free (a_escaped);
	g_free (s_escaped);
}

static void
google_reader_api_edit_tag (GoogleReaderActionPtr action, UpdateRequest *request, const gchar *token)
{
	update_request_set_source (request, action->source->api.edit_tag);

	const gchar* prefix = "feed";
	gchar* s_escaped = g_uri_escape_string (action->feedUrl, NULL, TRUE);
	gchar* a_escaped = NULL;
	gchar* i_escaped = g_uri_escape_string (action->guid, NULL, TRUE);;
	gchar* postdata = NULL;

	/*
	 * If the source of the item is a feed then the source *id* will be of
	 * the form tag:google.com,2005:reader/feed/http://foo.com/bar
	 * If the item is a shared link it is of the form
	 * tag:google.com,2005:reader/user/<sharer's-id>/source/com.google/link
	 * It is possible that there are items other thank link that has
	 * the ../user/.. id. The GR API requires the strings after ..:reader/
	 * while GoogleReaderAction only gives me after :reader/feed/ (or
	 * :reader/user/ as the case might be). I therefore need to guess
	 * the prefix ('feed/' or 'user/') from just this information.
	 */

	if (strstr(action->feedUrl, "://") == NULL)
		prefix = "user" ;

	if (action->actionType == EDIT_ACTION_MARK_UNREAD) {
		a_escaped = g_uri_escape_string (GOOGLE_READER_TAG_KEPT_UNREAD, NULL, TRUE);
		gchar *r_escaped = g_uri_escape_string (GOOGLE_READER_TAG_READ, NULL, TRUE);
		postdata = g_strdup_printf (action->source->api.edit_tag_ar_tag_post, i_escaped, prefix, s_escaped, a_escaped, r_escaped, token);
		g_free (r_escaped);
	}
	else if (action->actionType == EDIT_ACTION_MARK_READ) {
		a_escaped = g_uri_escape_string (GOOGLE_READER_TAG_READ, NULL, TRUE);
		postdata = g_strdup_printf (action->source->api.edit_tag_add_post, i_escaped, prefix, s_escaped, a_escaped, token);
	}
	else if (action->actionType == EDIT_ACTION_TRACKING_MARK_UNREAD) {
		a_escaped = g_uri_escape_string (GOOGLE_READER_TAG_TRACKING_KEPT_UNREAD, NULL, TRUE);
		postdata = g_strdup_printf (action->source->api.edit_tag_add_post, i_escaped, prefix, s_escaped, a_escaped, token);
	}
	else if (action->actionType == EDIT_ACTION_MARK_STARRED) {
		a_escaped = g_uri_escape_string (GOOGLE_READER_TAG_STARRED, NULL, TRUE) ;
		postdata = g_strdup_printf (action->source->api.edit_tag_add_post, i_escaped, prefix,
			s_escaped, a_escaped, token);
	}
	else if (action->actionType == EDIT_ACTION_MARK_UNSTARRED) {
		gchar* r_escaped = g_uri_escape_string(GOOGLE_READER_TAG_STARRED, NULL, TRUE);
		postdata = g_strdup_printf (action->source->api.edit_tag_remove_post, i_escaped, prefix,
			s_escaped, r_escaped, token);
	}

	else g_assert (FALSE);

	g_free (s_escaped);
	g_free (a_escaped);
	g_free (i_escaped);

	request->postdata = postdata;
}

static void
google_reader_api_edit_token_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	nodePtr		node;
	const gchar*	token;
	GoogleReaderActionPtr          action;
	UpdateRequest	*request;

	if (result->httpstatus != 200 || result->data == NULL) {
		/* FIXME: What is the behaviour that should go here? */
		return;
	}

	node = node_from_id ((gchar*) userdata);
	g_free (userdata);

	if (!node)
		return;

	token = result->data;

	if (!node->source || g_queue_is_empty (node->source->actionQueue))
		return;

	action = g_queue_peek_head (node->source->actionQueue);

	request = update_request_new (
		"NOT THE REAL URL",	// real URL will be set later based on action
		node->subscription->updateState,
		node->subscription->updateOptions
	);
	update_request_set_auth_value (request, node->source->authToken);

	if (action->actionType == EDIT_ACTION_MARK_READ ||
	    action->actionType == EDIT_ACTION_MARK_UNREAD ||
	    action->actionType == EDIT_ACTION_TRACKING_MARK_UNREAD ||
	    action->actionType == EDIT_ACTION_MARK_STARRED ||
	    action->actionType == EDIT_ACTION_MARK_UNSTARRED)
		google_reader_api_edit_tag (action, request, token);
	else if (action->actionType == EDIT_ACTION_ADD_SUBSCRIPTION)
		google_reader_api_add_subscription (action, request, token);
	else if (action->actionType == EDIT_ACTION_REMOVE_SUBSCRIPTION)
		google_reader_api_remove_subscription (action, request, token);
	else if (action->actionType == EDIT_ACTION_ADD_LABEL ||
		 action->actionType == EDIT_ACTION_REMOVE_LABEL)
		google_reader_api_edit_label (action, request, token);

	debug1 (DEBUG_UPDATE, "google_reader_api: postdata [%s]", request->postdata);
	update_execute_request (node->source, request, google_reader_api_edit_action_complete, google_reader_api_action_context_new(node->source, action), FEED_REQ_NO_FEED);

	action = g_queue_pop_head (node->source->actionQueue);
}

void
google_reader_api_edit_process (nodeSourcePtr source)
{
	UpdateRequest *request;

	g_assert (source);
	if (g_queue_is_empty (source->actionQueue))
		return;

	/*
 	* Google reader has a system of tokens. So first, I need to request a
 	* token from google, before I can make the actual edit request. The
 	* code here is the token code, the actual edit commands are in
 	* google_reader_api_edit_token_cb
	 */
	request = update_request_new (
		source->api.token,
		source->root->subscription->updateState,
		source->root->subscription->updateOptions
	);
	update_request_set_auth_value(request, source->authToken);

	update_execute_request (source, request, google_reader_api_edit_token_cb,
	                        g_strdup(source->root->id), FEED_REQ_NO_FEED);
}

static void
google_reader_api_edit_push_ (nodeSourcePtr source, GoogleReaderActionPtr action, gboolean head)
{
	g_assert (source->actionQueue);
	action->source = source;
	if (head)
		g_queue_push_head (source->actionQueue, action);
	else
		g_queue_push_tail (source->actionQueue, action);
}

static void
google_reader_api_edit_push (nodeSourcePtr source, GoogleReaderActionPtr action, gboolean head)
{
	g_assert (source);
	g_assert (source->actionQueue);
	google_reader_api_edit_push_ (source, action, head);

	/** @todo any flags I should specify? */
	if (source->loginState == NODE_SOURCE_STATE_NONE)
		subscription_update (source->root->subscription, NODE_SOURCE_UPDATE_ONLY_LOGIN);
	else if (source->loginState == NODE_SOURCE_STATE_ACTIVE)
		google_reader_api_edit_process (source);
}

static void
update_read_state_callback (nodeSourcePtr source, GoogleReaderActionPtr action, gboolean success)
{
	if (!success)
		debug0 (DEBUG_UPDATE, "Failed to change item state!");
}

void
google_reader_api_edit_mark_read (nodeSourcePtr source, const gchar *guid, const gchar *feedUrl, gboolean newStatus)
{
	GoogleReaderActionPtr action;

	action = google_reader_api_action_new (newStatus?EDIT_ACTION_MARK_READ:EDIT_ACTION_MARK_UNREAD);
	action->guid = g_strdup (guid);
	action->feedUrl = g_strdup (feedUrl);
	action->callback = update_read_state_callback;
	google_reader_api_edit_push (source, action, FALSE);

	if (newStatus == FALSE) {
		/*
		 * According to the Google Reader API, to mark an item unread,
		 * I also need to mark it as tracking-kept-unread in a separate
		 * network call.
		 */
		action = google_reader_api_action_new (EDIT_ACTION_TRACKING_MARK_UNREAD);
		action->guid = g_strdup (guid);
		action->feedUrl = g_strdup (feedUrl);
		google_reader_api_edit_push (source, action, FALSE);
	}
}

static void
update_starred_state_callback(nodeSourcePtr source, GoogleReaderActionPtr action, gboolean success)
{
	if (!success)
		debug0 (DEBUG_UPDATE, "Failed to change item state!");
}

void
google_reader_api_edit_mark_starred (nodeSourcePtr source, const gchar *guid, const gchar *feedUrl, gboolean newStatus)
{
	GoogleReaderActionPtr action = google_reader_api_action_new (newStatus?EDIT_ACTION_MARK_STARRED:EDIT_ACTION_MARK_UNSTARRED);

	action->guid = g_strdup (guid);
	action->feedUrl = g_strdup (feedUrl);
	action->callback = update_starred_state_callback;

	google_reader_api_edit_push (source, action, FALSE);
}

static void
google_reader_api_edit_add_subscription_complete (nodeSourcePtr source, GoogleReaderActionPtr action)
{
	/*
	 * It is possible that remote changed the name of the URL that
	 * was sent to it. In that case, we need to recover the URL
	 * from the list. But a node with the old URL has already
	 * been created. Allow the subscription update call to fix that.
	 */
	GSList* cur = source->root->children ;
	for(; cur; cur = g_slist_next (cur))  {
		nodePtr node = (nodePtr) cur->data;
		if (node->subscription) {
			if (g_str_equal (node->subscription->source, action->feedUrl)) {
				subscription_set_source (node->subscription, "");
				feedlist_node_added (node);
			}
		}
	}

	subscription_update (source->root->subscription, NODE_SOURCE_UPDATE_ONLY_LIST);
}

static void
google_reader_api_edit_add_subscription2_cb (nodeSourcePtr source, GoogleReaderActionPtr action, gboolean success)
{
	google_reader_api_edit_add_subscription_complete (source, action);
}

/* Subscription callback #1 (to set label (folder) before updating the feed list) */
static void
google_reader_api_edit_add_subscription_cb (nodeSourcePtr source, GoogleReaderActionPtr action, gboolean success)
{
	if (success) {
		if (action->label) {
			/* Extract returned new feed id (FIXME: is this only TheOldReader specific?) */
			const gchar *id = NULL;
			JsonParser *parser = json_parser_new ();

			if (!json_parser_load_from_data (parser, action->response, -1, NULL)) {
				debug0 (DEBUG_UPDATE, "Failed to parse JSON response");
			} else {
				id = json_get_string (json_parser_get_root (parser), "streamId");
				if (!id)
					debug0 (DEBUG_UPDATE, "ERROR: Found no 'streamId' in response. Cannot set folder label!");
			}

			if (id) {
				GoogleReaderActionPtr a = google_reader_api_action_new (EDIT_ACTION_ADD_LABEL);
				a->feedUrl = g_strdup (id);
				a->label = g_strdup (action->label);
				a->callback = google_reader_api_edit_add_subscription2_cb;
				google_reader_api_edit_push (source, a, TRUE);
			}
			g_object_unref (parser);
		} else {
			google_reader_api_edit_add_subscription_complete (source, action);
		}
	} else {
		debug0 (DEBUG_UPDATE, "Failed to subscribe");
		// FIXME: real error handling (dialog...)
	}
}

void
google_reader_api_edit_add_subscription (nodeSourcePtr source, const gchar* feedUrl, const gchar *label)
{
	GoogleReaderActionPtr action = google_reader_api_action_new (EDIT_ACTION_ADD_SUBSCRIPTION);
	action->feedUrl = g_strdup (feedUrl);
	action->label = g_strdup (label);
	action->callback = google_reader_api_edit_add_subscription_cb;
	google_reader_api_edit_push (source, action, TRUE);
}

static void
google_reader_api_edit_remove_callback (nodeSourcePtr source, GoogleReaderActionPtr action, gboolean success)
{
	if (success) {
		/*
		 * The node was removed from the feedlist, but could have
		 * returned because of an update before this edit request
		 * completed. No cleaner way to handle this.
		 */
		GSList* cur = source->root->children ;
		for(; cur; cur = g_slist_next (cur))  {
			nodePtr node = (nodePtr) cur->data ;
			g_autofree gchar *stream_id = action->get_stream_id_for_node (node);
			if (g_strcmp0 (stream_id, action->feedUrl) == 0) {
				feedlist_node_removed (node);
				return;
			}
		}
	} else
		debug0 (DEBUG_UPDATE, "Failed to remove subscription");
}

void
google_reader_api_edit_remove_subscription (nodeSourcePtr source, const gchar* feedUrl, gchar* (*get_stream_id_for_node) (nodePtr node))
{
	GoogleReaderActionPtr action = google_reader_api_action_new (EDIT_ACTION_REMOVE_SUBSCRIPTION);
	action->feedUrl = g_strdup (feedUrl);
	action->callback = google_reader_api_edit_remove_callback;
	action->get_stream_id_for_node = get_stream_id_for_node;
	google_reader_api_edit_push (source, action, TRUE);
}

void
google_reader_api_edit_add_label (nodeSourcePtr source, const gchar* feedUrl, const gchar* label)
{
	GoogleReaderActionPtr action = google_reader_api_action_new (EDIT_ACTION_ADD_LABEL);
	action->feedUrl = g_strdup (feedUrl);
	action->label = g_strdup (label);
	google_reader_api_edit_push (source, action, TRUE);
}

void
google_reader_api_edit_remove_label (nodeSourcePtr source, const gchar* feedUrl, const gchar* label)
{
	GoogleReaderActionPtr action = google_reader_api_action_new (EDIT_ACTION_REMOVE_LABEL);
	action->feedUrl = g_strdup (feedUrl);
	action->label = g_strdup (label);
	google_reader_api_edit_push (source, action, TRUE);
}

gboolean google_reader_api_edit_is_in_queue (nodeSourcePtr source, const gchar* guid)
{
	/* this is inefficient, but works for the time being */
	GList *cur = source->actionQueue->head;
	for(; cur; cur = g_list_next (cur)) {
		GoogleReaderActionPtr action = cur->data;
		if (action->guid && g_str_equal (action->guid, guid))
			return TRUE;
	}
	return FALSE;
}
