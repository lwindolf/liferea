/**
 * @file ttrss_source.h tt-rss feed list source support
 * 
 * Copyright (C) 2010 Lars Lindner <lars.lindner@gmail.com>
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
 
#ifndef _TTRSS_SOURCE_H
#define _TTRSS_SOURCE_H

#include "fl_sources/node_source.h"

/**
 * A nodeSource specific for tt-rss
 */
typedef struct ttrssSource {
	nodePtr	        root;	/**< the root node in the feed list */
	gchar           *session_id; /**< the tt-rss session id */
	GQueue          *actionQueue;
	int             loginState; /**< The current login state */

	/**
	 * A map from a subscription source to a timestamp when it was last 
	 * updated (provided by Google).
	 */
	GHashTable      *lastTimestampMap; 

	/**
	 * A timestamp when the last Quick update took place.
	 */
	GTimeVal        lastQuickUpdate;
} *ttrssSourcePtr;

 
enum { 
	TTRSS_SOURCE_STATE_NONE = 0,
	TTRSS_SOURCE_STATE_IN_PROGRESS,
	TTRSS_SOURCE_STATE_ACTIVE
};

enum  { 
	/**
	 * Update only the subscription list, and not each node underneath it.
	 * Note: Uses higher 16 bits to avoid conflict.
	 */
	TTRSS_SOURCE_UPDATE_ONLY_LIST = (1<<16),
	/**
	 * Only login, do not do any updates. 
	 */
	TTRSS_SOURCE_UPDATE_ONLY_LOGIN = (1<<17)
};

/**
 * tt-rss API URL's
 *
 * http://tt-rss.org/redmine/wiki/tt-rss/JsonApiReference
 */

/**
 * Tiny Tiny RSS Login API
 *
 * @param user		The tt-rss account id
 * @param passwd	The tt-rss account password
 *
 * @returns {"session_id":"xxx"} or {"error":"xxx"}
 */ 
#define TTRSS_LOGIN_URL "%s/api/?op=login&user=%s&password=%s" 

/**
 * Fetch tt-rss feed list.
 *
 * @param sid	session id
 *
 * @returns JSON feed list
 */
#define TTRSS_SUBSCRIPTION_LIST_URL "%s/api/?op=getFeeds&sid=%s"

/**
 * Interval (in seconds) for doing a Quick Update. 
 */
#define TTRSS_SOURCE_QUICK_UPDATE_INTERVAL 600  /* 10 minutes */

/**
 * Returns ttss source type implementation info.
 */
nodeSourceTypePtr ttrss_source_get_type (void);

void ttrss_source_login (ttrssSourcePtr source, guint32 flags);

extern struct subscriptionType ttrssSourceFeedSubscriptionType;
extern struct subscriptionType ttrssSourceSubscriptionType;

#endif
