/**
 * @file feed.h  common feed handling interface
 *
 * Copyright (C) 2003-2021 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _FEED_H
#define _FEED_H

#include <libxml/parser.h>
#include <glib.h>

#include "node_type.h"
#include "subscription_type.h"

/*
 * The feed concept in Liferea comprises several standalone concepts:
 *
 * 1.) A "feed" is an XML/XML-like document to be parsed
 *     (-> see feed_parser.h)
 *
 * 2.) A "feed" is a node type that is visible in the feed list.
 *
 * 3.) A "feed" is a subscription type: a way of updating.
 *
 * The feed.h interface provides default methods for 2.) and 3.) that
 * are used per-default but might be overwritten by node source, node
 * type or subscription type specific implementations.
 */

/** Common structure to hold all information about a single feed. */
typedef struct feed {
	struct feedHandler *fhp;		/**< Feed format parsing handler. */

	/* feed cache state properties */
	gint		cacheLimit;		/**< Amount of cache to save: See the cache_limit enum */

	/* feed parsing state */
	gboolean	valid;			/**< FALSE if there was an error in xml_parse_feed() */
	GString		*parseErrors;		/**< Detailed textual description of parsing errors (e.g. library error handler output) */
	gint64		time;			/**< Feeds modified date */

	/* feed specific behaviour settings */
	gboolean	encAutoDownload;	/**< if TRUE do automatically download enclosures */
	gboolean	ignoreComments;		/**< if TRUE ignore comment feeds for this feed */
	gboolean	markAsRead;		/**< if TRUE downloaded items are automatically marked as read */
	gboolean	html5Extract;		/**< if TRUE try to fetch extra content via HTML5 / Google AMP */
	gboolean	alwaysShowInReduced;	/**< for newsbins only, if TRUE always show when using reduced feed list */
} *feedPtr;

/**
 * Create a new feed structure.
 *
 * @returns a new feed structure
 */
feedPtr feed_new(void);

/**
 * Serialization helper function for rendering purposes.
 *
 * @param node		the feed node to serialize
 * @param feedNode	XML node to add feed attributes to,
 *                      or NULL if a new XML document is to
 *                      be created
 *
 * @returns a new XML document (if feedNode was NULL)
 */
xmlDocPtr feed_to_xml(nodePtr node, xmlNodePtr xml);

// FIXME: doesn't seem to belong here (looks like a subscription type method)
/**
 * Returns the feed-specific maximum cache size.
 * If none is set it returns the global default
 * setting.
 *
 * @param node	the feed node
 *
 * @returns max item count
 */
guint feed_get_max_item_count(nodePtr node);

// FIXME: doesn't seem to belong here (looks like a subscription method)
/**
 * feed_enrich_item:
 * Tries to fetch extra content for the item description
 *
 * @subscription: the subscription
 * @item: the item
 */
void feed_enrich_item (subscriptionPtr subscription, itemPtr item);

/**
 * Returns the subscription type implementation for simple feed nodes.
 * This subscription type is used as the default subscription type.
 */
subscriptionTypePtr feed_get_subscription_type (void);

#define IS_FEED(node) (node->type == feed_get_node_type ())

/**
 * Returns the node type implementation for simple feed nodes.
 * This node type is the default node type for non-folder nodes.
 */
nodeTypePtr feed_get_node_type (void);

#endif
