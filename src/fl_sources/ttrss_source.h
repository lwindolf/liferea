/**
 * @file ttrss_source.h tt-rss feed list source support
 * 
 * Copyright (C) 2010-2013 Lars Windolf <lars.lindner@gmail.com>
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
	nodePtr		root;		/**< the root node in the feed list */
	gchar		*session_id;	/**< the tt-rss session id */
	GQueue		*actionQueue;
	gint		apiLevel;	/**< The API level reported by the instance (or 0) */
	gint		loginState;	/**< The current login state */
	gint		authFailures;	/**< Number of authentication failures */
	gboolean	selfUpdating;	/**< True if remote updating daemon is running */
} *ttrssSourcePtr;
 
enum { 
	TTRSS_SOURCE_STATE_NONE = 0,		/**< no authentication tried so far */
	TTRSS_SOURCE_STATE_IN_PROGRESS,		/**< authentication in progress */
	TTRSS_SOURCE_STATE_ACTIVE,		/**< authentication succeeded */
	TTRSS_SOURCE_STATE_NO_AUTH		/**< authentication has failed */
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
 * Number of auth failures after which we stop bothering the user while
 * auto-updating until he manually updates again.
 */
#define TTRSS_SOURCE_MAX_AUTH_FAILURES		3

/**
 * TinyTinyRSS JSON API is documented here:
 *
 * http://tt-rss.org/redmine/projects/tt-rss/wiki/JsonApiReference
 */

#define TTRSS_URL "%s/api/"

/**
 * Tiny Tiny RSS Login API
 *
 * @param user		The tt-rss account id
 * @param passwd	The tt-rss account password
 *
 * @returns {"session_id":"xxx"} or {"error":"xxx"}
 */ 
#define TTRSS_JSON_LOGIN "{\"op\":\"login\", \"user\":\"%s\", \"password\":\"%s\"}" 

/**
 * Fetch tt-rss feed list.
 *
 * @param sid	session id
 *
 * @returns JSON feed list
 */
#define TTRSS_JSON_SUBSCRIPTION_LIST "{\"op\":\"getFeeds\", \"sid\":\"%s\", \"cat_id\":\"-3\", \"include_nested\":\"true\"}"

/**
 * Fetch tt-rss headlines for a given feed.
 *
 * @param sid		session id
 * @param feed_id	tt-rss feed id
 * @param limit		feed cache size
 *
 * @returns JSON feed list
 */
#define TTRSS_JSON_HEADLINES "{\"op\":\"getHeadlines\", \"sid\":\"%s\", \"feed_id\":\"%s\", \"limit\":\"%d\", \"show_content\":\"true\", \"view_mode\":\"all_articles\"}"

/**
 * Toggle item flag state.
 *
 * @param sid		session id
 * @param item_id	tt-rss item id
 * @param mode		0 = unflagged, 1 = flagged
 */
#define TTRSS_JSON_UPDATE_ITEM_FLAG "{\"op\":\"updateArticle\", \"sid\":\"%s\", \"article_ids\":\"%s\", \"mode\":\"%d\", \"field\":\"0\"}"

/**
 * Toggle item read state.
 *
 * @param sid		session id
 * @param item_id	tt-rss item id
 * @param mode		0 = read, 1 = unread
 */
#define TTRSS_JSON_UPDATE_ITEM_UNREAD "{\"op\":\"updateArticle\", \"sid\":\"%s\", \"article_ids\":\"%s\", \"mode\":\"%d\", \"field\":\"2\"}"

/**
 * Determine server settings.
 *
 * @param sid		session id
 */
#define TTRSS_JSON_GET_CONFIG "{\"op\":\"getConfig\", \"sid\":\"%s\"}"

/**
 * Returns ttss source type implementation info.
 */
nodeSourceTypePtr ttrss_source_get_type (void);

void ttrss_source_login (ttrssSourcePtr source, guint32 flags);

extern struct subscriptionType ttrssSourceFeedSubscriptionType;
extern struct subscriptionType ttrssSourceSubscriptionType;

#endif
