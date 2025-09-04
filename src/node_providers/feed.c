/**
 * @file feed.c  feed node and subscription type
 *
 * Copyright (C) 2003-2025 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "node_providers/feed.h"

#include <string.h>

#include "conf.h"
#include "common.h"
#include "db.h"
#include "debug.h"
#include "favicon.h"
#include "feedlist.h"
#include "html.h"
#include "itemlist.h"
#include "metadata.h"
#include "node.h"
#include "update.h"
#include "xml.h"
#include "ui/icons.h"
#include "ui/liferea_shell.h"
#include "ui/subscription_dialog.h"
#include "ui/feed_list_view.h"

static void
feed_import (Node *node, Node *parent, xmlNodePtr xml, gboolean trusted)
{
	xmlChar		*title;

	node_set_subscription (node, subscription_import (xml, trusted));

	title = xmlGetProp (xml, BAD_CAST"title");
	if (!title || !xmlStrcmp (title, BAD_CAST"")) {
		if (title)
			xmlFree (title);
		title = xmlGetProp (xml, BAD_CAST"text");
	}

	node_set_title (node, (gchar *)title);
	xmlFree (title);

	if (node->subscription)
		debug (DEBUG_CACHE, "import feed: title=%s source=%s interval=%d",
		        node_get_title (node),
	        	subscription_get_source (node->subscription),
		        subscription_get_update_interval (node->subscription));
}

static void
feed_export (Node *node, xmlNodePtr xml, gboolean trusted)
{
	subscription_export (node->subscription, xml, trusted);
}

/* implementation of subscription type interface */

static void
feed_process_update_result (subscriptionPtr subscription, const UpdateResult * const result, updateFlags flags)
{
	feedParserCtxtPtr	ctxt;
	Node			*node = subscription->node;


	ctxt = feed_parser_ctxt_new (subscription, result->data, result->size);

	/* try to parse the feed */
	if (!feed_parse (ctxt)) {
		/* No feed found, display an error */
		node->available = FALSE;

	} else if (!ctxt->subscription->fhp) {
		/* There's a feed but no handler. This means autodiscovery
		 * found a feed, but we still need to download it.
		 * An update should be in progress that will process it */
	} else {
		/* Feed found, process it */
		itemSetPtr	itemSet;

		node->available = TRUE;

		/* merge the resulting items into the node's item set */
		itemSet = node_get_itemset (node);
		node->newCount = itemset_merge_items (itemSet, ctxt->items, ctxt->subscription->valid, ctxt->subscription->markAsRead);
		if (node->newCount)
			itemlist_merge_itemset (itemSet);
		itemset_free (itemSet);

		/* restore user defined properties if necessary */
		if ((flags & UPDATE_REQUEST_RESET_TITLE) && ctxt->title)
			node_set_title (node, ctxt->title);
	}

	feed_parser_ctxt_free (ctxt);

	// FIXME: this should not be here, but in subscription.c
	if (FETCH_ERROR_NONE != subscription->error)
		node->available = FALSE;

}

static gboolean
feed_prepare_update_request (subscriptionPtr subscription, UpdateRequest *request)
{
	/* Nothing to do. Feeds require no subscription extra handling. */

	return TRUE;
}

/* implementation of the node type interface */

static itemSetPtr
feed_load (Node *node)
{
	return db_itemset_load(node->id);
}

static void
feed_save (Node *node)
{
	/* Nothing to do. Feeds do not have any UI states */
}

static void
feed_update_counters (Node *node)
{
	node->itemCount = db_itemset_get_item_count (node->id);
	node->unreadCount = db_itemset_get_unread_count (node->id);
}

static void
feed_remove (Node *node)
{
	favicon_remove_from_cache (node->id);
	db_subscription_remove (node->id);
}

static gboolean
feed_add (void)
{
	subscription_dialog_new ();
	return TRUE;
}

static void
feed_properties (Node *node)
{
	subscription_prop_dialog_new (node->subscription);
}

static void
feed_free (Node *node)
{
}

subscriptionTypePtr
feed_get_subscription_type (void)
{
	static struct subscriptionType sti = {
		feed_prepare_update_request,
		feed_process_update_result
	};

	return &sti;
}

nodeProviderPtr
feed_get_provider (void)
{
	static struct nodeProvider nti = {
		NODE_CAPABILITY_SHOW_UNREAD_COUNT |
		NODE_CAPABILITY_UPDATE |
		NODE_CAPABILITY_UPDATE_FAVICON |
		NODE_CAPABILITY_EXPORT |
		NODE_CAPABILITY_EXPORT_ITEMS,
		"feed",		/* not used, feed format ids are used instead */
		ICON_DEFAULT,
		feed_import,
		feed_export,
		feed_load,
		feed_save,
		feed_update_counters,
		feed_remove,
		feed_add,
		feed_properties,
		feed_free
	};

	return &nti;
}
