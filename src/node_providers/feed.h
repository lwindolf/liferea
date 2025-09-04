/**
 * @file feed.h  common feed handling interface
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

#ifndef _FEED_H
#define _FEED_H

#include <libxml/parser.h>
#include <glib.h>

#include "node_provider.h"
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
 * provider or subscription type specific implementations.
 */

/**
 * feed_to_xml: (skip)
 * Serialization helper function for rendering purposes.
 *
 * @param node		the feed node to serialize
 * @param feedNode	XML node to add feed attributes to,
 *                      or NULL if a new XML document is to
 *                      be created
 *
 * @returns a new XML document (if feedNode *was NULL)
 */
xmlDocPtr feed_to_xml(Node *node, xmlNodePtr xml);

/**
 * Returns the subscription type implementation for simple feed nodes.
 * This subscription type is used as the default subscription type.
 */
subscriptionTypePtr feed_get_subscription_type (void);

/**
 * feed_get_provider: (skip)
 */
nodeProviderPtr feed_get_provider (void);

#endif
