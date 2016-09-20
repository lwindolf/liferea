#ifndef _OWNCLOUDNEWS_SOURCE_FEED_LIST
#define _OWNCLOUDNEWS_SOURCE_FEED_LIST

#include "fl_sources/owncloudnews_source.h"

/**
 * @brief Prepare source update request
 *
 * Get the web app status.
 *
 * @param subscription subscriptionPtr
 * @param *request updateRequest
 * @return gboolean
 */
static gboolean prepare_update_request (
	subscriptionPtr subscription, struct updateRequest *request);

/**
 * @brief Fetch folders, feeds, and items if the ownCloud News app is working
 * properly. Otherwise show an error message
 *
 * Process the initial update request which tells us if something is wrong
 * with the News apps. If there are no errors, then go ahead and fetch folders,
 * feeds, and items.
 *
 * @param subscription
 * @param result
 * @param flags
 * @return void
 */
static void process_update_result (
	subscriptionPtr subscription, const struct updateResult * const result,
	updateFlags flags);

/**
 * TODO: document
 */
static void merge_feed (
	owncloudnewsSourcePtr source, const gchar *url,
	const gchar *title, gint64 id, nodePtr folder
);

/**
 * TODO: document well
 * Update feeds from the API results
 */
static void update_feeds_cb (
	const struct updateResult * const result,
	gpointer user_data, guint32 flags);

/**
 * TODO: document well
 * Update folders from the API results
 * Make a request to get feeds from the API
 */
static void update_folders_cb (
	const struct updateResult * const result,
	gpointer user_data, guint32 flags);

/**
 * TODO: document
 */
static void update_folders (subscriptionPtr subscription);


#endif //_OWNCLOUDNEWS_SOURCE_FEED_LIST_H

