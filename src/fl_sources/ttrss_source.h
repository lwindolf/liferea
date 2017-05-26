/**
 * @file ttrss_source.h  Tiny Tiny RSS feed list source support
 * 
 * Copyright (C) 2010-2014 Lars Windolf <lars.windolf@gmx.de>
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

#include <glib.h>

#include "fl_sources/node_source.h"

/**
 * A nodeSource specific for tt-rss
 */
typedef struct ttrssSource {
	nodePtr		root;			/**< the root node in the feed list */
	gchar		*session_id;		/**< the current session id */
	const gchar	*url;			/**< the API base URL */
	gint		apiLevel;		/**< The API level reported by the instance (or 0) */
	GHashTable	*categories;		/**< Lookup hash for TTRSS feed id to TTRSS category id */
	GHashTable	*folderToCategory;	/**< Lookup hash for folder node id to TTRSS category id */
} *ttrssSourcePtr;

/**
 * TinyTinyRSS JSON API is documented here:
 *
 * http://tt-rss.org/redmine/projects/tt-rss/wiki/JsonApiReference
 */

#define TTRSS_URL "%s/api/"

/**
 * TinyTinyRSS Login API
 *
 * @param user		The tt-rss account id
 * @param passwd	The tt-rss account password
 *
 * @returns {"session_id":"xxx"} or {"error":"xxx"}
 */ 
#define TTRSS_JSON_LOGIN "{\"op\":\"login\", \"user\":\"%s\", \"password\":\"%s\"}" 

/**
 * Fetch TinyTinyRSS feed list.
 *
 * @param sid	session id
 *
 * @returns JSON feed list
 */
#define TTRSS_JSON_SUBSCRIPTION_LIST "{\"op\":\"getFeeds\", \"sid\":\"%s\", \"cat_id\":\"-3\", \"include_nested\":\"true\"}"

/**
 * Add a subscription to TinyTinyRSS
 *
 * @param sid		session id
 * @param feed_url	URL
 * @param category_id	category id (or 0)
 * @param login		user name
 * @param password	password
 */
#define TTRSS_JSON_SUBSCRIBE "{\"op\":\"subscribeToFeed\", \"sid\":\"%s\", \"feed_url\":\"%s\", \"category_id\":%d, \"login\":\"%s\", \"password\":\"%s\"}"

/**
 * Removes a subscription from TinyTinyRSS
 *
 * @param sid		session id
 * @param feed_id	TinyTinyRSS feed id
 */
#define TTRSS_JSON_UNSUBSCRIBE "{\"op\":\"unsubscribeFeed\", \"sid\":\"%s\", \"feed_id\":\"%s\"}"

/**
 * Fetch tt-rss categories list (default is fetching it tree like)
 *
 * @returns JSON categories list
 */
#define TTRSS_JSON_CATEGORIES_LIST "{\"op\":\"getFeedTree\", \"sid\":\"%s\", \"include_empty\":\"true\"}"

/**
 * Fetch TinyTinyRSS headlines for a given feed.
 *
 * @param sid		session id
 * @param feed_id	tt-rss feed id
 * @param limit		feed cache size
 *
 * @returns JSON feed list
 */
#define TTRSS_JSON_HEADLINES "{\"op\":\"getHeadlines\", \"sid\":\"%s\", \"feed_id\":\"%s\", \"limit\":\"%d\", \"show_content\":\"true\", \"view_mode\":\"all_articles\", \"include_attachments\":\"true\"}"

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
 * Returns ttss source type implementation info.
 */
nodeSourceTypePtr ttrss_source_get_type (void);

void ttrss_source_login (ttrssSourcePtr source, guint32 flags);

#endif
