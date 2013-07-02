/**
 * @file aol_source_edit.c  AOL reader feed list source syncing support
 * 
 * Copyright (C) 2013 Lars Windolf <lars.lindner@gmail.com>
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
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
#include <gtk/gtk.h>
#include <string.h>
#include "debug.h"
#include "update.h"
#include "subscription.h"
#include "common.h"
#include "feedlist.h"


#include "aol_source.h"
#include "aol_source_edit.h"
#include "config.h"
#include <libxml/xmlwriter.h>
#include <libxml/xmlreader.h>
#include <glib/gstdio.h>
#include "xml.h"

/**
 * A structure to indicate an edit to the Google Reader "database".
 * These edits are put in a queue and processed in sequential order
 * so that google does not end up processing the requests in an 
 * unintended order.
 */
typedef struct AolSourceAction {
	/**
	 * The guid of the item to edit. This will be ignored if the 
	 * edit is acting on an subscription rather than an item.
	 */
	gchar* guid;

	/**
	 * A MANDATORY feed url to containing the item, or the url of the 
	 * subscription to edit. 
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
	 * A callback function on completion of the edit.
	 */
	void (*callback) (AolSourcePtr source, struct AolSourceAction* edit, gboolean success);

	/**
	 * The type of this AolSourceAction.
	 */
	int actionType ; 
} *AolSourceActionPtr ; 

enum { 
	EDIT_ACTION_MARK_READ,
	EDIT_ACTION_MARK_UNREAD,
	EDIT_ACTION_TRACKING_MARK_UNREAD, /**< every UNREAD request, should be followed by tracking-kept-unread */
	EDIT_ACTION_MARK_STARRED,
	EDIT_ACTION_MARK_UNSTARRED,
	EDIT_ACTION_ADD_SUBSCRIPTION,
	EDIT_ACTION_REMOVE_SUBSCRIPTION
} ;
		

typedef struct AolSourceAction* editPtr ;

typedef struct AolSourceActionCtxt { 
	gchar   *nodeId ;
	AolSourceActionPtr action; 
} *AolSourceActionCtxtPtr; 


static void aol_source_edit_push (AolSourcePtr source, AolSourceActionPtr action, gboolean head);


static AolSourceActionPtr 
aol_source_action_new (void)
{
	AolSourceActionPtr action = g_slice_new0 (struct AolSourceAction);
	return action;
}

static void 
aol_source_action_free (AolSourceActionPtr action)
{ 
	g_free (action->guid);
	g_free (action->feedUrl);
	g_slice_free (struct AolSourceAction, action);
}

static AolSourceActionCtxtPtr
aol_source_action_context_new(AolSourcePtr source, AolSourceActionPtr action)
{
	AolSourceActionCtxtPtr ctxt = g_slice_new0(struct AolSourceActionCtxt);
	ctxt->nodeId = g_strdup(source->root->id);
	ctxt->action = action;
	return ctxt;
}

static void
aol_source_action_context_free(AolSourceActionCtxtPtr ctxt)
{
	g_free(ctxt->nodeId);
	g_slice_free(struct AolSourceActionCtxt, ctxt);
}

static void
aol_source_edit_action_complete (const struct updateResult* const result, gpointer userdata, updateFlags flags) 
{ 
	AolSourceActionCtxtPtr     editCtxt = (AolSourceActionCtxtPtr) userdata; 
	nodePtr                       node = node_from_id (editCtxt->nodeId);
	AolSourcePtr               source; 
	AolSourceActionPtr         action = editCtxt->action ;
	
	aol_source_action_context_free (editCtxt);

	if (!node) {
		aol_source_action_free (action);
		return; /* probably got deleted before this callback */
	} 
	source = (AolSourcePtr) node->data;
		
	if (result->data == NULL || !g_str_equal (result->data, "OK")) {
		if (action->callback) 
			(*action->callback) (source, action, FALSE);
		debug1 (DEBUG_UPDATE, "The edit action failed with result: %s\n", result->data);
		aol_source_action_free (action);
		return; /** @todo start a timer for next processing */
	}
	
	if (action->callback)
		action->callback (source, action, TRUE);

	aol_source_action_free (action);

	/* process anything else waiting on the edit queue */
	aol_source_edit_process (source);
}

/* the following aol_source_api_* functions are simply funtions that 
   convert a AolSourceActionPtr to a updateRequestPtr */
 
static void
aol_source_api_add_subscription (AolSourceActionPtr action, updateRequestPtr request, const gchar* token) 
{
	update_request_set_source (request, AOL_READER_ADD_SUBSCRIPTION_URL);
	gchar* s_escaped = g_uri_escape_string (action->feedUrl, NULL, TRUE) ;
	gchar* postdata = g_strdup_printf (AOL_READER_ADD_SUBSCRIPTION_POST, s_escaped, token);
	g_free (s_escaped);

	debug1 (DEBUG_UPDATE, "aol_source: postdata [%s]", postdata);
	request->postdata = postdata ;
}

static void
aol_source_api_remove_subscription (AolSourceActionPtr action, updateRequestPtr request, const gchar* token) 
{
	update_request_set_source (request, AOL_READER_REMOVE_SUBSCRIPTION_URL);
	gchar* s_escaped = g_uri_escape_string (action->feedUrl, NULL, TRUE);
	g_assert (!request->postdata);
	request->postdata = g_strdup_printf (AOL_READER_REMOVE_SUBSCRIPTION_POST, s_escaped, token);
	g_free (s_escaped);
}

static void 
aol_source_api_edit_tag (AolSourceActionPtr action, updateRequestPtr request, const gchar*token) 
{
	update_request_set_source (request, AOL_READER_EDIT_TAG_URL); 

	const gchar* prefix = "feed" ; 
	gchar* s_escaped = g_uri_escape_string (action->feedUrl, NULL, TRUE);
	gchar* a_escaped = NULL ;
	gchar* i_escaped = g_uri_escape_string (action->guid, NULL, TRUE);
	gchar* postdata = NULL ;

	/*
	 * If the source of the item is a feed then the source *id* will be of
	 * the form tag:google.com,2005:reader/feed/http://foo.com/bar
	 * If the item is a shared link it is of the form
	 * tag:google.com,2005:reader/user/<sharer's-id>/source/com.google/link
	 * It is possible that there are items other thank link that has
	 * the ../user/.. id. The GR API requires the strings after ..:reader/
	 * while AolSourceAction only gives me after :reader/feed/ (or 
	 * :reader/user/ as the case might be). I therefore need to guess
	 * the prefix ('feed/' or 'user/') from just this information. 
	 */

	if (strstr(action->feedUrl, "://") == NULL) 
		prefix = "user" ;

	if (action->actionType == EDIT_ACTION_MARK_UNREAD) {
		a_escaped = g_uri_escape_string (AOL_READER_TAG_KEPT_UNREAD, NULL, TRUE);
		gchar *r_escaped = g_uri_escape_string (AOL_READER_TAG_READ, NULL, TRUE);
		postdata = g_strdup_printf (AOL_READER_EDIT_TAG_AR_TAG, i_escaped, prefix, s_escaped, a_escaped, r_escaped, token);
		g_free (r_escaped);
	}
	else if (action->actionType == EDIT_ACTION_MARK_READ) { 
		a_escaped = g_uri_escape_string (AOL_READER_TAG_READ, NULL, TRUE);
		postdata = g_strdup_printf (AOL_READER_EDIT_TAG_ADD_TAG, i_escaped, prefix, s_escaped, a_escaped, token);
	}
	else if (action->actionType == EDIT_ACTION_TRACKING_MARK_UNREAD) {
		a_escaped = g_uri_escape_string (AOL_READER_TAG_TRACKING_KEPT_UNREAD, NULL, TRUE);
		postdata = g_strdup_printf (AOL_READER_EDIT_TAG_ADD_TAG, i_escaped, prefix, s_escaped, a_escaped, token);
	}  
	else if (action->actionType == EDIT_ACTION_MARK_STARRED) { 
		a_escaped = g_uri_escape_string (AOL_READER_TAG_STARRED, NULL, TRUE) ;
		postdata = g_strdup_printf (
			AOL_READER_EDIT_TAG_ADD_TAG, i_escaped, prefix,
			s_escaped, a_escaped, token);
	}
	else if (action->actionType == EDIT_ACTION_MARK_UNSTARRED) {
		gchar* r_escaped = g_uri_escape_string(AOL_READER_TAG_STARRED, NULL, TRUE);
		postdata = g_strdup_printf (
			AOL_READER_EDIT_TAG_REMOVE_TAG, i_escaped, prefix,
			s_escaped, r_escaped, token);
	}
	
	else g_assert (FALSE);
	
	g_free (s_escaped);
	g_free (a_escaped); 
	g_free (i_escaped);
	
	debug1 (DEBUG_UPDATE, "aol_source: postdata [%s]", postdata);


	request->postdata = postdata;
}

static void
aol_source_edit_token_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{ 
	nodePtr          node;
	AolSourcePtr  source;
	const gchar*     token;
	AolSourceActionPtr          action;
	updateRequestPtr request; 

	if (result->httpstatus != 200 || result->data == NULL) { 
		/* FIXME: What is the behaviour that should go here? */
		return;
	}

	node = node_from_id ((gchar*) userdata);
	g_free (userdata);
	
	if (!node) {
		return;
	}
	source = (AolSourcePtr) node->data;


	token = result->data; 

	if (!source || g_queue_is_empty (source->actionQueue))
		return;

	action = g_queue_peek_head (source->actionQueue);

	request = update_request_new ();
	request->updateState = update_state_copy (source->root->subscription->updateState);
	request->options = update_options_copy (source->root->subscription->updateOptions) ;
	update_request_set_auth_value (request, source->authHeaderValue);

	if (action->actionType == EDIT_ACTION_MARK_READ || 
	    action->actionType == EDIT_ACTION_MARK_UNREAD || 
	    action->actionType == EDIT_ACTION_TRACKING_MARK_UNREAD ||
	    action->actionType == EDIT_ACTION_MARK_STARRED || 
	    action->actionType == EDIT_ACTION_MARK_UNSTARRED) 
		aol_source_api_edit_tag (action, request, token);
	else if (action->actionType == EDIT_ACTION_ADD_SUBSCRIPTION ) 
		aol_source_api_add_subscription (action, request, token);
	else if (action->actionType == EDIT_ACTION_REMOVE_SUBSCRIPTION )
		aol_source_api_remove_subscription (action, request, token) ;

	update_execute_request (source, request, aol_source_edit_action_complete, aol_source_action_context_new(source, action), 0);

	action = g_queue_pop_head (source->actionQueue);
}

void
aol_source_edit_process (AolSourcePtr source)
{ 
	updateRequestPtr request; 
	
	g_assert (source);
	if (g_queue_is_empty (source->actionQueue))
		return;
	
	/*
 	* Google reader has a system of tokens. So first, I need to request a 
 	* token from google, before I can make the actual edit request. The
 	* code here is the token code, the actual edit commands are in 
 	* aol_source_edit_token_cb
	 */
	request = update_request_new ();
	request->updateState = update_state_copy (source->root->subscription->updateState);
	request->options = update_options_copy (source->root->subscription->updateOptions);
	request->source = g_strdup (AOL_READER_TOKEN_URL);
	update_request_set_auth_value(request, source->authHeaderValue);

	update_execute_request (source, request, aol_source_edit_token_cb, 
	                        g_strdup(source->root->id), 0);
}

static void
aol_source_edit_push_ (AolSourcePtr source, AolSourceActionPtr action, gboolean head)
{ 
	g_assert (source->actionQueue);
	if (head) g_queue_push_head (source->actionQueue, action);
	else      g_queue_push_tail (source->actionQueue, action);
}

static void 
aol_source_edit_push (AolSourcePtr source, AolSourceActionPtr action, gboolean head)
{
	g_assert (source);
	nodePtr root = source->root;
	aol_source_edit_push_ (source, action, head);

	/** @todo any flags I should specify? */
	if (source->loginState == AOL_SOURCE_STATE_NONE) 
		subscription_update(root->subscription, AOL_SOURCE_UPDATE_ONLY_LOGIN);
	else if ( source->loginState == AOL_SOURCE_STATE_ACTIVE) 
		aol_source_edit_process (source);
}

static void 
update_read_state_callback (AolSourcePtr source, AolSourceActionPtr action, gboolean success) 
{
	if (success) {
		// FIXME: call item_read_state_changed (item, newState);
	} else {
		debug0 (DEBUG_UPDATE, "Failed to change item state!\n");
	}
}

void
aol_source_edit_mark_read (AolSourcePtr source, const gchar *guid, const gchar *feedUrl,	gboolean newStatus)
{
	AolSourceActionPtr action = aol_source_action_new ();

	action->guid = g_strdup (guid);
	action->feedUrl = g_strdup (feedUrl);
	action->actionType = newStatus ? EDIT_ACTION_MARK_READ :
	                           EDIT_ACTION_MARK_UNREAD;
	action->callback = update_read_state_callback;
	
	aol_source_edit_push (source, action, FALSE);

	if (newStatus == FALSE) { 
		/*
		 * According to the Google Reader API, to mark an item unread, 
		 * I also need to mark it as tracking-kept-unread in a separate
		 * network call.
		 */
		action = aol_source_action_new ();
		action->guid = g_strdup (guid);
		action->feedUrl = g_strdup (feedUrl);
		action->actionType = EDIT_ACTION_TRACKING_MARK_UNREAD;
		aol_source_edit_push (source, action, FALSE);
	}
}

static void
update_starred_state_callback(AolSourcePtr source, AolSourceActionPtr action, gboolean success) 
{
	if (success) {
		// FIXME: call item_flag_changed (item, newState);
	} else {
		debug0 (DEBUG_UPDATE, "Failed to change item state!\n");
	}
}

void
aol_source_edit_mark_starred (AolSourcePtr source, const gchar *guid, const gchar *feedUrl, gboolean newStatus)
{
	AolSourceActionPtr action = aol_source_action_new ();

	action->guid = g_strdup (guid);
	action->feedUrl = g_strdup (feedUrl);
	action->actionType = newStatus ? EDIT_ACTION_MARK_STARRED : EDIT_ACTION_MARK_UNSTARRED;
	action->callback = update_starred_state_callback;
	
	aol_source_edit_push (source, action, FALSE);
}

static void 
update_subscription_list_callback(AolSourcePtr source, AolSourceActionPtr action, gboolean success) 
{
	if (success) { 
		/*
		 * It is possible that Google changed the name of the URL that
		 * was sent to it. In that case, I need to recover the URL 
		 * from the list. But a node with the old URL has already 
		 * been created. Allow the subscription update call to fix that.
		 */
		GSList* cur = source->root->children ;
		for(; cur; cur = g_slist_next (cur))  {
			nodePtr node = (nodePtr) cur->data ; 
			if (g_str_equal (node->subscription->source, action->feedUrl)) {
				subscription_set_source (node->subscription, "");
				feedlist_node_added (node);
			}
		}
		
		debug0 (DEBUG_UPDATE, "Subscription list was updated successful\n");
		subscription_update (source->root->subscription, AOL_SOURCE_UPDATE_ONLY_LIST);
	} else 
		debug0 (DEBUG_UPDATE, "Failed to update subscriptions\n");
}

void 
aol_source_edit_add_subscription (AolSourcePtr source, const gchar* feedUrl)
{
	AolSourceActionPtr action = aol_source_action_new () ;
	action->actionType = EDIT_ACTION_ADD_SUBSCRIPTION; 
	action->feedUrl = g_strdup (feedUrl);
	action->callback = update_subscription_list_callback;
	aol_source_edit_push (source, action, TRUE);
}

static void
aol_source_edit_remove_callback (AolSourcePtr source, AolSourceActionPtr action, gboolean success)
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
			if (g_str_equal (node->subscription->source, action->feedUrl)) {
				feedlist_node_removed (node);
				return;
			}
		}
	} else
		debug0 (DEBUG_UPDATE, "Failed to remove subscription");
}

void aol_source_edit_remove_subscription (AolSourcePtr source, const gchar* feedUrl) 
{
	AolSourceActionPtr action = aol_source_action_new (); 
	action->actionType = EDIT_ACTION_REMOVE_SUBSCRIPTION;
	action->feedUrl = g_strdup (feedUrl);
	action->callback = aol_source_edit_remove_callback;
	aol_source_edit_push (source, action, TRUE);
}

gboolean aol_source_edit_is_in_queue (AolSourcePtr source, const gchar* guid) 
{
	/* this is inefficient, but works for the time being */
	GList *cur = source->actionQueue->head; 
	for(; cur; cur = g_list_next (cur)) { 
		AolSourceActionPtr action = cur->data; 
		if (action->guid && g_str_equal (action->guid, guid))
			return TRUE;
	}
	return FALSE;
}
