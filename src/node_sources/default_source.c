/**
 * @file default_source.c  default static feed list source
 * 
 * Copyright (C) 2005-2026 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2005-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include <glib/gstdio.h>
#include <string.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "node_providers/folder.h"
#include "update.h"
#include "net_monitor.h"
#include "node_sources/default_source.h"
#include "node_source.h"

static guint autoUpdateTimer = 0;

static gboolean
default_source_auto_update (gpointer userdata)
{
	if (network_monitor_is_online ()) {
		debug (DEBUG_UPDATE, "default_source: auto update\n");
		node_foreach_child ((Node *)userdata, node_auto_update_subscription);
	} else {
		debug (DEBUG_UPDATE, "default_source: no update processing because we are offline!");
	}

	return TRUE;
}

void
default_source_start_updating (Node *root)
{
	/* 1. Check if we want an extra initial update run */
	gint startup_feed_action = 1;
	conf_get_int_value (STARTUP_FEED_ACTION, &startup_feed_action);
	if (0 == startup_feed_action) {
		debug (DEBUG_UPDATE, "default_source: initial update: updating all feeds");
		node_update_subscription (root, NULL /* flags*/);
	} else {
		debug (DEBUG_UPDATE, "default_source: initial update: disabled");
	}

	/* 2. start auto updating */
	autoUpdateTimer = g_timeout_add_seconds (10, default_source_auto_update, root);
}

static void
default_source_login (Node *root, updateFlags flags)
{
	/* no login on default_source */
}

static void
default_source_free (Node *root)
{
	if (autoUpdateTimer) {
		g_source_remove (autoUpdateTimer);
		autoUpdateTimer = 0;
	}
}

static Node *
default_source_add_subscription (Node *node, subscriptionPtr subscription)
{
	/* For the local feed list source subscriptions are always
	   feed subscriptions implemented by the feed node and 
	   subscription type... */
	Node *child = node_new ("feed");
	node_set_title (child, _("New Subscription"));
	node_set_subscription (child, subscription);	/* feed subscription type is implicit */
	feedlist_node_added (child);
	
	subscription_update (subscription, UPDATE_REQUEST_RESET_TITLE | UPDATE_REQUEST_PRIORITY_HIGH);
	return child;
}

static Node *
default_source_add_folder (Node *node, const gchar *title)
{
	/* For the local feed list source folders are always 
	   real folders implemented by the folder node type... */
	Node *child = node_new ("folder");
	node_set_title (child, title);
	feedlist_node_added (child);
	
	return child;
}

static void
default_source_remove_node (Node *node, Node *child)
{
	/* The default source can always immediately serve remove requests. */
	feedlist_node_removed (child);
}

nodeSourceTypePtr
default_source_get_type (void)
{
	static struct nodeSourceType nst = {
		.id			= "fl_default",
		.name			= "Static Feed List",
		.capabilities		= NODE_SOURCE_CAPABILITY_IS_ROOT |
					NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST |
					NODE_SOURCE_CAPABILITY_ADD_FEED |
					NODE_SOURCE_CAPABILITY_ADD_FOLDER |
					NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST,
		.source_login		= default_source_login,
		.source_free 		= default_source_free,
		.add_subscription	= default_source_add_subscription,
		.add_folder		= default_source_add_folder,
		.remove_node		= default_source_remove_node
	};
	nst.feedSubscriptionType = feed_get_subscription_type ();

	return &nst;
}
