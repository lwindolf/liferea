/**
 * @file google_source.h Google Reader feed list source support
 * 
 * Copyright (C) 2007-2022 Lars Windolf <lars.windolf@gmx.de>
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
 
#ifndef _GOOGLE_SOURCE_H
#define _GOOGLE_SOURCE_H

#include "fl_sources/node_source.h"

/**
 * A nodeSource specific for Google Reader
 */
typedef struct GoogleSource {
	nodePtr		root;		/**< the root node in the feed list */
	GQueue		*actionQueue;
	
	GHashTable	*folderToCategory;	/**< Lookup hash for folder node id to Google Reader category id */
} *GoogleSourcePtr;

enum {
	GOOGLE_SOURCE_STATE_NONE = 0,	/**< no authentication tried so far */
	GOOGLE_SOURCE_STATE_IN_PROGRESS,	/**< authentication in progress */
	GOOGLE_SOURCE_STATE_ACTIVE,	/**< authentication succeeded */
	GOOGLE_SOURCE_STATE_NO_AUTH,	/**< authentication has failed */
	GOOGLE_SOURCE_STATE_MIGRATE,	/**< source will be migrated, do not do anything anymore! */
}; 

enum { 
	/**
	 * Update only the subscription list, and not each node underneath it.
	 * Note: Uses higher 16 bits to avoid conflict.
	 */
	GOOGLE_SOURCE_UPDATE_ONLY_LIST = (1<<16),
	/**
	 * Only login, do not do any updates. 
	 */
	GOOGLE_SOURCE_UPDATE_ONLY_LOGIN = (1<<17)
};

/**
 * Number of auth failures after which we stop bothering the user while
 * auto-updating until he manually updates again.
 */
#define GOOGLE_SOURCE_MAX_AUTH_FAILURES		3

/**
 * Acts like a feed, indicating all the posts shared by the Google Reader
 * friends. Does not take any params, but the Authorization header needs to be set.
 */
#define GOOGLE_READER_BROADCAST_FRIENDS_URL "http://www.google.com/reader/atom/user/-/state/com.google/broadcast-friends"

/** A set of tags (states) defined by Google reader */

#define GOOGLE_READER_TAG_KEPT_UNREAD          "user/-/state/com.google/kept-unread"
#define GOOGLE_READER_TAG_READ                 "user/-/state/com.google/read"
#define GOOGLE_READER_TAG_TRACKING_KEPT_UNREAD "user/-/state/com.google/tracking-kept-unread"
#define GOOGLE_READER_TAG_STARRED              "user/-/state/com.google/starred"

/** Interval (in seconds) for doing a Quick Update: 10min */
#define GOOGLE_SOURCE_QUICK_UPDATE_INTERVAL 600

/**
 * @returns Google Reader source type implementation info.
 */
nodeSourceTypePtr google_source_get_type (void);

extern struct subscriptionType googleSourceFeedSubscriptionType;
extern struct subscriptionType googleSourceOpmlSubscriptionType;

/**
 * Find a child node with the given feed source URL.
 *
 * @param gsource	GoogleSource
 * @param source	a feed source URL to search
 *
 * @returns a node (or NULL)
 */
nodePtr google_source_get_node_from_source (GoogleSourcePtr gsource, const gchar* source);

/**
 * Migrate a google source child-node from a Liferea 1.4 style read-only
 * google source nodes.
 *
 * @param node The node to migrate (not the nodeSource!)
 */
void google_source_migrate_node (nodePtr node);

/**
 * Perform login for the given Google source.
 *
 * @param gsource	a GoogleSource
 * @param flags		network request flags
 */
void google_source_login (GoogleSourcePtr gsource, guint32 flags);

#endif

