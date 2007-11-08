/**
 * @file subscription.h common subscription handling interface
 * 
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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
#ifndef _SUBSCRIPTION_H
#define _SUBSCRIPTION_H
 
#include <glib.h>
#include <libxml/parser.h>
#include "node.h"
#include "update.h"

/** Flags used in the request structure */
enum feed_request_flags {
	FEED_REQ_RESET_TITLE		= (1<<0),	/**< Feed's title should be reset to default upon update */
	FEED_REQ_RESET_UPDATE_INT	= (1<<1),	/**< Feed's update interval should be reset to default upon update */
	FEED_REQ_AUTO_DISCOVER		= (1<<2),	/**< Feed auto-discovery attempts should be made */
	
	FEED_REQ_PRIORITY_HIGH		= (1<<3),	/**< set to signal that this is an important user triggered request */
	FEED_REQ_DOWNLOAD_FAVICON	= (1<<4),	/**< set to make the favicon be updated after the feed is downloaded */
	FEED_REQ_AUTH_DIALOG		= (1<<5),	/**< set to make an auth request dialog to be created after 401 errors */
	FEED_REQ_ALLOW_RETRIES		= (1<<6),	/**< set to allow fetch retries on network errors */
	FEED_REQ_NO_PROXY		= (1<<7)	/**< sets no proxy flag */
};
 
/** Common structure to hold all information about a single subscription. */
typedef struct subscription {
	nodePtr		node;			/**< the feed list node the subscription is attached to */
	
	gchar		*source;		/**< current source, can be changed by redirects */
	gchar		*origSource;		/**< the source given when creating the subscription */
	updateOptionsPtr updateOptions;		/**< update options for the feed source */
	struct updateJob *updateJob;		/**< update request structure used when downloading the subscribed source */
	
	gint		updateInterval;		/**< user defined update interval in minutes */	
	guint		defaultInterval;	/**< optional update interval as specified by the feed */
	
	GSList		*metadata;		/**< metadata list assigned to this subscription */
	
	gchar		*updateError;		/**< textual description of processing errors */
	gchar		*httpError;		/**< textual description of HTTP protocol errors */
	gint		httpErrorCode;		/**< last HTTP error code */
	updateStatePtr	updateState;		/**< update states (etag, last modified, cookies, last polling times...) */

	gboolean	discontinued;		/**< flag to avoid updating after HTTP 410 */

	gchar		*filtercmd;		/**< feed filter command */
	gchar		*filterError;		/**< textual description of filter errors */
} *subscriptionPtr;

/**
 * Generic update result processing callback type.
 * This callback must not free the result structure. It will be
 * free'd by the download system after the callback returns.
 *
 * @param node		the subscriptions node
 * @param result	the update result
 * @param flags		update result processing flags
 */
typedef void (*subscription_update_cb) (nodePtr node, const struct updateResult * const result, guint32 flags);

/**
 * Create a new subscription structure.
 *
 * @param source	the subscription source URL (or NULL)
 * @param filter	a post processing filter (or NULL)
 * @param options	update options (or NULL)
 *
 * @returns the new subscription
 */
subscriptionPtr subscription_new (const gchar *source, const gchar *filter, updateOptionsPtr options);

/**
 * Imports a subscription from the given XML document.
 *
 * @param xml		xml node
 * @param trusted	TRUE when importing from internal XML document
 *
 * @returns a new subscription
 */
subscriptionPtr subscription_import (xmlNodePtr xml, gboolean trusted);

/**
 * Exports the given subscription to the given XML document.
 *
 * @param subscription	the subscription
 * @param xml		xml node
 * @param trusted	TRUE when exporting to internal XML document
 */
void subscription_export (subscriptionPtr subscription, xmlNodePtr xml, gboolean trusted);

/**
 * Checks wether a subscription is ready to be updated
 *
 * @param subscription	the subscription to check
 *
 * @returns TRUE if subscription can be updated
 */
gboolean subscription_can_be_updated(subscriptionPtr subscription);

/**
 * Triggers updating a subscription. Will download the 
 * the document indicated by the source URL of the subscription.
 * Will call the node type specific update callback to process
 * the downloaded data.
 *
 * @param subscription	the subscription
 * @param flags		update flags
 */
void subscription_update (subscriptionPtr subscription, guint flags);

/**
 * Like subscription_update() updates a given subscription, but
 * allows to specify another result processing callback then the
 * one defined by the node type of the subscription.
 *
 * @param subscription	the subscription
 * @param callback	the callback
 * @param flags		update flags
 */
void subscription_update_with_callback (subscriptionPtr subscription, subscription_update_cb callback, guint flags);

/**
 * Called when auto updating. Checks wether the subscription
 * needs to be updated (according to it's update interval) and
 * if necessary calls subscription_update().
 *
 * @param subscription	the subscription
 */
void subscription_auto_update (subscriptionPtr subscription);

/**
 * Get the update interval setting of a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the currently configured update interval (in seconds)
 */
gint subscription_get_update_interval(subscriptionPtr subscription);

/**
 * Set the update interval setting for the given subscription
 *
 * @param subscription	the subscription
 * @param interval	the new update interval (in seconds)
 */
void subscription_set_update_interval(subscriptionPtr subscription, gint interval);

/**
 * Get the default update interval setting of a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the default update interval (in seconds) or 0
 */
guint subscription_get_default_update_interval(subscriptionPtr subscription);

/**
 * Set the default update interval setting for the given subscription
 *
 * @param subscription	the subscription
 * @param interval	the default update interval (in seconds)
 */
void subscription_set_default_update_interval(subscriptionPtr subscription, guint interval);

/**
 * Reset the update counter for the given subscription.
 *
 * @param subscription	the subscription
 */
void subscription_reset_update_counter (subscriptionPtr subscription, GTimeVal *now);

/**
 * Allows to explicitely set the cookies for the next 
 * update request. This method needs to be called after
 * subscription_set_source() which always resets the cookies
 * property. Note: update requests handled by the default
 * subscription result processing callback will overwrite
 * the cookies again in case of permanent redirects, so it
 * is necessary to set cookies before each request if 
 * different cookies than the source specific ones are
 * necessary.
 *
 * @param subscription	the subscription
 * @param cookies	the cookies
 */
void subscription_set_cookies (subscriptionPtr subscription, const gchar *cookies);

/**
 * Get the source URL of a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the source URL
 */
const gchar * subscription_get_source(subscriptionPtr subscription);

/**
 * Set a new source URL for the given subscription
 *
 * @param subscription	the subscription
 * @param source	the new source URL
 */
void subscription_set_source(subscriptionPtr subscription, const gchar *source);

/**
 * Get the original subscription URL
 *
 * @param subscription	the subscription
 *
 * @returns the original source URL
 */
const gchar * subscription_get_orig_source(subscriptionPtr subscription);

/**
 * Set the original subscription URL
 *
 * @param subscription	the subscription
 * @param source	the new original source URL
 */ 
void subscription_set_orig_source(subscriptionPtr subscription, const gchar *source);

/**
 * Get the configured filter command for a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the filter command
 */
const gchar * subscription_get_filter(subscriptionPtr subscription);

/**
 * Set a new filter command for a given subscription
 *
 * @param subscription	the subscription
 * @param filter	the new filter command
 */
void subscription_set_filter(subscriptionPtr subscription, const gchar * filter);

/**
 * Updates the error status of the given subscription
 *
 * @param subscription	the subscription
 * @param httpstatus	the new HTTP status code
 * @param resultcode	the update result code
 * @param filterError	filter error string (or NULL)
 */
void subscription_update_error_status(subscriptionPtr subscription, gint httpstatus, gint resultcode, gchar *filterError);

/**
 * Frees the given subscription structure.
 *
 * @param subscription	the subscription
 */ 
void subscription_free(subscriptionPtr subscription);

#endif
