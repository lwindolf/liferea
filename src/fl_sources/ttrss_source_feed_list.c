/**
 * @file ttrss_source_feed_list.c  tt-rss feed list handling routines.
 * 
 * Copyright (C) 2010  Lars Lindner <lars.lindner@gmail.com>
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


#include "ttrss_source_feed_list.h"

#include <glib.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"

#include "fl_sources/ttrss_source.h"

/* source subscription type implementation */

static void
ttrss_subscription_cb (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	//ttrssSourcePtr	source = (ttrssSourcePtr) subscription->node->data;
	
	if (result->data) {
		g_warning ("FIXME: ttrss_subscription_cb(): Implement me!");
	} else {
		subscription->node->available = FALSE;
		debug0 (DEBUG_UPDATE, "ttrss_subscription_cb(): ERROR: failed to get subscription list!\n");
	}

	if (!(flags & TTRSS_SOURCE_UPDATE_ONLY_LIST))
		node_foreach_child_data (subscription->node, node_update_subscription, GUINT_TO_POINTER (0));
}

/** functions for an efficient updating mechanism */

/*static void
ttrss_source_quick_update_cb (const struct updateResult* const result, gpointer userdata, updateFlags flasg) 
{
	ttrssSourcePtr source = (ttrssSourcePtr) userdata;

	if (!result->data) { 
		debug0 (DEBUG_UPDATE, "TtRssSource: Unable to get unread counts, this update is aborted.");
		return;
	}
	g_warning ("FIXME: ttrss_source_quick_update_cb(): Implement me!");
}*/

gboolean
ttrss_source_quick_update(ttrssSourcePtr source) 
{
	g_warning ("FIXME: ttrss_source_quick_update(): Implement me!");
	
/*	updateRequestPtr request = update_request_new ();
	request->updateState = update_state_copy (source->root->subscription->updateState);
	request->options = update_options_copy (source->root->subscription->updateOptions);
	update_request_set_source (request, TTRSS_READER_UNREAD_COUNTS_URL);
	update_request_set_auth_value(request, source->authHeaderValue);

	update_execute_request (source, request, ttrss_source_quick_update_cb,
				source, 0);
*/
	return TRUE;
}

static void
ttrss_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags)
{
	ttrss_subscription_cb (subscription, result, flags);
}

static gboolean
ttrss_subscription_prepare_update_request (subscriptionPtr subscription, struct updateRequest *request)
{
	ttrssSourcePtr	source = (ttrssSourcePtr) subscription->node->data;

	g_assert (source);
	if (source->loginState == TTRSS_SOURCE_STATE_NONE) {
		debug0 (DEBUG_UPDATE, "TtRssSource: login");
		ttrss_source_login (source, 0);
		return FALSE;
	}
	debug1 (DEBUG_UPDATE, "updating tt-rss subscription (node id %s)", subscription->node->id);
	
	g_warning ("FIXME: update tt-rss feed list!");
	// FIXME: update_request_set_source (request, TTRSS_SUBSCRIPTION_LIST_URL);
	
	// FIXME: update_request_set_auth_value (request, source->authHeaderValue);
	
	return TRUE;
}

/* OPML subscription type definition */

struct subscriptionType ttrssSourceSubscriptionType = {
	ttrss_subscription_prepare_update_request,
	ttrss_subscription_process_update_result
};
