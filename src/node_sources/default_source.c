/**
 * @file default_source.c  default static feed list source
 * 
 * Copyright (C) 2005-2014 Lars Windolf <lars.windolf@gmx.de>
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
#include "debug.h"
#include "export.h"
#include "node_providers/feed.h"
#include "feedlist.h"
#include "node_providers/folder.h"
#include "update.h"
#include "node_sources/default_source.h"
#include "node_source.h"

static void
default_source_auto_update (Node *node)
{	
	node_foreach_child (node, node_auto_update_subscription);
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

static struct nodeSourceType nst = {
	.id			= "fl_default",
	.name			= "Static Feed List",
	.capabilities		= NODE_SOURCE_CAPABILITY_IS_ROOT |
				  NODE_SOURCE_CAPABILITY_HIERARCHIC_FEEDLIST |
	                          NODE_SOURCE_CAPABILITY_ADD_FEED |
	                          NODE_SOURCE_CAPABILITY_ADD_FOLDER |
				  NODE_SOURCE_CAPABILITY_WRITABLE_FEEDLIST,
	.feedSubscriptionType	= NULL,
	.sourceSubscriptionType	= NULL,
	.source_type_init	= NULL,
	.source_type_deinit	= NULL,
	.source_new		= NULL,
	.source_delete		= NULL,
	.source_auto_update	= default_source_auto_update,
	.free 			= NULL,
	.add_subscription	= default_source_add_subscription,
	.add_folder		= default_source_add_folder,
	.remove_node		= default_source_remove_node,
	.convert_to_local	= NULL
};

nodeSourceTypePtr
default_source_get_type (void)
{
	nst.feedSubscriptionType = feed_get_subscription_type ();

	return &nst;
}
