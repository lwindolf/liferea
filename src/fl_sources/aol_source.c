/**
 * @file aol_source.c  AOL reader feed list source support
 * 
 * Copyright (C) 2007-2014 Lars Windolf <lars.lindner@gmail.com>
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
 * Copyright (C) 2011 Peter Oliver
 * Copyright (C) 2011 Sergey Snitsaruk <narren96c@gmail.com>
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

#include "fl_sources/aol_source.h"

#include <glib.h>
#include <gtk/gtk.h>
#include <string.h>
#include <libxml/xpath.h>

#include "common.h"
#include "debug.h"
#include "feedlist.h"
#include "item_state.h"
#include "metadata.h"
#include "node.h"
#include "subscription.h"
#include "update.h"
#include "xml.h"
#include "ui/auth_dialog.h"
#include "ui/liferea_dialog.h"
#include "ui/liferea_htmlview.h"
#include "fl_sources/node_source.h"
#include "fl_sources/opml_source.h"
#include "fl_sources/aol_source_edit.h"
#include "fl_sources/aol_source_opml.h"

/** default AOL reader subscription list update interval = once a day */
#define AOL_SOURCE_UPDATE_INTERVAL 60*60*24

/** create a AOL source with given node as root */ 
static AolSourcePtr
aol_source_new (nodePtr node) 
{
	AolSourcePtr source = g_new0 (struct AolSource, 1) ;
	source->root = node; 
	source->actionQueue = g_queue_new (); 
	source->loginState = AOL_SOURCE_STATE_NONE; 
	source->lastTimestampMap = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
	
	return source;
}

static void
aol_source_free (AolSourcePtr source) 
{
	if (!source)
		return;

	update_job_cancel_by_owner (source);
	
	g_free (source->authHeaderValue);
	g_queue_free (source->actionQueue) ;
	g_hash_table_unref (source->lastTimestampMap);
	g_free (source);
}

void
aol_source_set_state (AolSourcePtr source, gint state)
{
	debug3 (DEBUG_UPDATE, "AOL source '%s' now in state %d (was %d)", source->root->id, state, source->loginState);
	source->loginState = state;
}

static void
aol_source_login_cb (const struct updateResult * const result, gpointer userdata, updateFlags flags)
{
	AolSourcePtr	source = (AolSourcePtr) userdata;
	gchar		*tmp = NULL;
	subscriptionPtr subscription = source->root->subscription;
		
	debug1 (DEBUG_UPDATE, "google login processing... %s", result->data);
	
	g_assert (!source->authHeaderValue);
	
	if (result->data && result->httpstatus == 200)
		tmp = strstr (result->data, "Auth=");
		
	if (tmp) {
		gchar *ttmp = tmp; 
		tmp = strchr (tmp, '\n');
		if (tmp)
			*tmp = '\0';
		source->authHeaderValue = g_strdup_printf ("GoogleLogin auth=%s", ttmp + 5);

		debug1 (DEBUG_UPDATE, "AOL reader Auth token found: %s", source->authHeaderValue);

		aol_source_set_state (source, AOL_SOURCE_STATE_ACTIVE);
		source->authFailures = 0;

		/* now that we are authenticated trigger updating to start data retrieval */
		if (!(flags & AOL_SOURCE_UPDATE_ONLY_LOGIN))
			subscription_update (subscription, flags);

		/* process any edits waiting in queue */
		aol_source_edit_process (source);

	} else {
		debug0 (DEBUG_UPDATE, "AOL reader login failed! no Auth token found in result!");
		subscription->node->available = FALSE;

		g_free (subscription->updateError);
		subscription->updateError = g_strdup (_("AOL Reader login failed!"));
		source->authFailures++;

		if (source->authFailures < AOL_SOURCE_MAX_AUTH_FAILURES)
			aol_source_set_state (source, AOL_SOURCE_STATE_NONE);
		else
			aol_source_set_state (source, AOL_SOURCE_STATE_NO_AUTH);
		
		auth_dialog_new (subscription, flags);
	}
}

/**
 * Perform a login to AOL Reader with OAuth2, if the login completes the 
 * AolSource will have a valid Auth token and will have loginStatus 
 * AOL_SOURCE_LOGIN_ACTIVE.
 */
void
aol_source_login (AolSourcePtr source, guint32 flags) 
{
	GtkWidget	*window;
	LifereaHtmlView *htmlview;

	/* There will be session expirations due to OAuth2, FIXME: handle them with refresh token! */
	if (source->loginState != AOL_SOURCE_STATE_NONE)
		debug1(DEBUG_UPDATE, "Logging in while login state is %d\n", source->loginState);

	aol_source_set_state (source, AOL_SOURCE_STATE_IN_PROGRESS);

	window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title (GTK_WINDOW (window), _("AOL Reader Login"));
	gtk_window_set_default_size (GTK_WINDOW (window), 640, 480);
	htmlview = liferea_htmlview_new (FALSE);

	// FIXME: Redirect doesn't work yet
	liferea_htmlview_launch_URL_internal (htmlview, "https://api.screenname.aol.com/auth/authorize?client_id=:li1CKi3GIZzVBPyw&scope=reader&response_type=token&redirect_uri=liferea-aol-oauth2://");
	gtk_container_add (GTK_CONTAINER (window), liferea_htmlview_get_widget (htmlview));
	gtk_widget_show_all (window);
}

/* node source type implementation */

static void
aol_source_update (nodePtr node)
{
	AolSourcePtr source = (AolSourcePtr) node->data;

	/* Reset AOL_SOURCE_STATE_NO_AUTH as this is a manual
	   user interaction and no auto-update so we can query
	   for credentials again. */
	if (source->loginState == AOL_SOURCE_STATE_NO_AUTH)
		aol_source_set_state (source, AOL_SOURCE_STATE_NONE);

	subscription_update (node->subscription, 0);  // FIXME: 0 ?
}

static void
aol_source_auto_update (nodePtr node)
{
	GTimeVal	now;
	AolSourcePtr source = (AolSourcePtr) node->data;

	if (source->loginState == AOL_SOURCE_STATE_NONE) {
		aol_source_update (node);
		return;
	}

	if (source->loginState == AOL_SOURCE_STATE_IN_PROGRESS) 
		return; /* the update will start automatically anyway */

	g_get_current_time (&now);
	
	/* do daily updates for the feed list and feed updates according to the default interval */
	if (node->subscription->updateState->lastPoll.tv_sec + AOL_SOURCE_UPDATE_INTERVAL <= now.tv_sec) {
		subscription_update (node->subscription, 0);
		g_get_current_time (&source->lastQuickUpdate);
	}
	else if (source->lastQuickUpdate.tv_sec + AOL_SOURCE_QUICK_UPDATE_INTERVAL <= now.tv_sec) {
		aol_source_opml_quick_update (source);
		aol_source_edit_process (source);
		g_get_current_time (&source->lastQuickUpdate);
	}
}

static void
aol_source_init (void) { }

static void aol_source_deinit (void) { }

static void
aol_source_import_node (nodePtr node)
{
	GSList *iter; 
	for (iter = node->children; iter; iter = g_slist_next(iter)) {
		nodePtr subnode = iter->data;
		if (subnode->subscription)
			subnode->subscription->type = &aolSourceFeedSubscriptionType; 
		if (subnode->type->capabilities
		    & NODE_CAPABILITY_SUBFOLDERS)
			aol_source_import_node (subnode);
	}
}

static void
aol_source_import (nodePtr node)
{
	opml_source_import (node);
	
	node->subscription->updateInterval = -1;
	node->subscription->type = &aolSourceOpmlSubscriptionType;
	if (!node->data)
		node->data = (gpointer) aol_source_new (node);

	aol_source_import_node (node);
}

static void
aol_source_export (nodePtr node)
{
	opml_source_export (node);
}

static gchar *
aol_source_get_feedlist (nodePtr node)
{
	return opml_source_get_feedlist (node);
}

static void 
aol_source_remove (nodePtr node)
{ 
	opml_source_remove (node);
}

static nodePtr
aol_source_add_subscription (nodePtr node, subscriptionPtr subscription) 
{ 
	debug_enter ("aol_source_add_subscription");
	nodePtr child = node_new (feed_get_node_type ());

	debug0 (DEBUG_UPDATE, "AolSource: Adding a new subscription"); 
	node_set_data (child, feed_new ());

	node_set_subscription (child, subscription);
	child->subscription->type = &aolSourceFeedSubscriptionType;
	
	node_set_title (child, _("New Subscription"));

	aol_source_edit_add_subscription (node_source_root_from_node (node)->data, subscription->source);
	
	debug_exit ("aol_source_add_subscription");
	
	return child;
}

static void
aol_source_remove_node (nodePtr node, nodePtr child) 
{ 
	gchar           *src; 
	AolSourcePtr source = node->data;
	
	if (child == node) { 
		feedlist_node_removed (child);
		return; 
	}

	src = g_strdup (child->subscription->source);

	feedlist_node_removed (child);

	/* propagate the removal only if there aren't other copies */
	if (!aol_source_opml_get_node_by_source (source, src)) 
		aol_source_edit_remove_subscription (source, src);
	
	g_free (source);
}

/* GUI callbacks */

static void
aol_source_get_account_info (void)
{
	/* We do not need credentials as this will be handled by OAuth2 during login */
	nodePtr		node;
	subscriptionPtr	subscription;

	subscription = subscription_new ("http://reader.aol.com/", NULL, NULL);
	node = node_new (node_source_get_node_type ());
	node_set_title (node, "AOL Reader");
	node_source_new (node, aol_source_get_type ());
	node_set_subscription (node, subscription);
	subscription->type = &aolSourceOpmlSubscriptionType ; 

	node->data = aol_source_new (node);
	feedlist_node_added (node);
	aol_source_update (node);
}

static void
aol_source_cleanup (nodePtr node)
{
	AolSourcePtr reader = (AolSourcePtr) node->data;
	aol_source_free(reader);
	node->data = NULL ;
}

static void 
aol_source_item_set_flag (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr root = node_source_root_from_node (node);
	aol_source_edit_mark_starred ((AolSourcePtr)root->data, item->sourceId, node->subscription->source, newStatus);
	item_flag_state_changed (item, newStatus);
}

static void
aol_source_item_mark_read (nodePtr node, itemPtr item, gboolean newStatus)
{
	nodePtr root = node_source_root_from_node (node);
	aol_source_edit_mark_read ((AolSourcePtr)root->data, item->sourceId, node->subscription->source, newStatus);
	item_read_state_changed (item, newStatus);
}

/**
 * Convert all subscriptions of a google source to local feeds
 *
 * @param node The node to migrate (not the nodeSource!)
 */
static void
aol_source_convert_to_local (nodePtr node)
{
	AolSourcePtr source = node->data; 

	aol_source_set_state (source, AOL_SOURCE_STATE_MIGRATE);
}

/* node source type definition */

static struct nodeSourceType nst = {
	.id                  = "fl_aol",
	.name                = N_("AOL Reader"),
	.description         = N_("Integrate the feed list of your AOL Reader account. Liferea will "
	                          "present your AOL Reader subscriptions, and will synchronize your feed list and reading lists."),
	.capabilities        = NODE_SOURCE_CAPABILITY_DYNAMIC_CREATION | 
	                       NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST |
	                       NODE_SOURCE_CAPABILITY_ADD_FEED |
	                       NODE_SOURCE_CAPABILITY_ITEM_STATE_SYNC |
	                       NODE_SOURCE_CAPABILITY_CONVERT_TO_LOCAL,
	.source_type_init    = aol_source_init,
	.source_type_deinit  = aol_source_deinit,
	.source_new          = aol_source_get_account_info,
	.source_delete       = aol_source_remove,
	.source_import       = aol_source_import,
	.source_export       = aol_source_export,
	.source_get_feedlist = aol_source_get_feedlist,
	.source_update       = aol_source_update,
	.source_auto_update  = aol_source_auto_update,
	.free                = aol_source_cleanup,
	.item_set_flag       = aol_source_item_set_flag,
	.item_mark_read      = aol_source_item_mark_read,
	.add_folder          = NULL, 
	.add_subscription    = aol_source_add_subscription,
	.remove_node         = aol_source_remove_node,
	.convert_to_local    = aol_source_convert_to_local
};

nodeSourceTypePtr
aol_source_get_type (void)
{
	return &nst;
}
