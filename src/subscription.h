/**
 * @file subscription.h  common subscription handling interface
 *
 * Copyright (C) 2003-2026 Lars Windolf <lars.windolf@gmx.de>
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

/** Caching property constants */
enum cache_limit {
	/* Values > 0 are used to specify certain limits */
	CACHE_DISABLE = 0,
	CACHE_DEFAULT = -1,
	CACHE_UNLIMITED = -2,
};

/** Subscription fetching error types */
typedef enum fetchError {
	FETCH_ERROR_NONE     = 0,
	FETCH_ERROR_AUTH     = 1 << 0,
	FETCH_ERROR_NET      = 1 << 1,
	FETCH_ERROR_DISCOVER = 1 << 2,
	FETCH_ERROR_XML      = 1 << 3	/*<< historically named "XML", but should be used for all parsing issues */
	/* when adding stuff here, extend xstl/feed.xml.in also! */
} fetchError;

/** Common structure to hold all information about a single subscription. */
typedef struct subscription {
	Node		*node;			/*<< the feed list node the subscription is attached to */
	struct subscriptionType *type;		/*<< the subscription type */

	// state
	gchar		*source;		/*<< current source, can be changed by redirects */
	gchar		*origSource;		/*<< the source given when creating the subscription */
	updateOptionsPtr updateOptions;		/*<< update options for the feed source */
	UpdateJob 	*updateJob;		/*<< update request structure used when downloading the subscribed source */

	GSList		*metadata;		/*<< metadata list assigned to this subscription */

	// network state
	fetchError	error;			/*<< Fetch error code (used for user-facing UI to differentiate subscription update processing phases) */
	gchar		*updateError;		/*<< textual description of processing errors */
	gchar		*httpError;		/*<< textual description of HTTP protocol errors */
	gint		httpErrorCode;		/*<< last HTTP error code */
	updateStatePtr	updateState;		/*<< update states (etag, last modified, cookies, last polling times...) */

	guint		autoDiscoveryTries;	/*<< counter to break auto discovery redirect circles */

	gboolean	activeAuth;		/*<< TRUE if authentication in progress */

	gboolean	discontinued;		/*<< flag to avoid updating after HTTP 410 */

	// parsing state
	gboolean	valid;			/**< FALSE if there was an error in xml_parse_feed() */
	GString		*parseErrors;		/**< Detailed textual description of parsing errors (e.g. library error handler output) */
	gint64		time;			/**< Feeds modified date */
	struct feedHandler *fhp;		/**< Feed format parsing handler. */

	// settings
	gint		updateInterval;		/*<< user defined update interval in minutes (-1 for global default, -2 for no auto update) */

	// feed-only settings
	gchar		*filtercmd;		/*<< feed filter command */
	gchar		*filterError;		/*<< textual description of filter errors */
	gint		cacheLimit;		/**< Amount of cache to save: See the cache_limit enum */

	gboolean	encAutoDownload;	/**< if TRUE do automatically download enclosures */
	gboolean	ignoreComments;		/**< if TRUE ignore comment feeds for this feed */
	gboolean	markAsRead;		/**< if TRUE downloaded items are automatically marked as read */
	gboolean	html5Extract;		/**< if TRUE try to fetch extra content via HTML5 / Google AMP */
	gboolean	alwaysShowInReduced;	/**< for newsbins only, if TRUE always show when using reduced feed list */
	gboolean	loadItemLink;		/*<< if TRUE do automatically load the item link into the HTML pane */	
} *subscriptionPtr;

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
 * Called when auto updating. Checks whether the subscription
 * needs to be updated (according to it's update interval) and
 * if necessary calls subscription_update().
 *
 * @param subscription	the subscription
 * @param flags		update flags
 */
void subscription_auto_update (subscriptionPtr subscription, updateFlags flags);

/**
 * Cancels a currently running subscription update. This is to
 * be called when removing subscriptions or retriggering the update
 * upon user request.
 *
 * @param subscription	the subscription
 */
void subscription_cancel_update (subscriptionPtr subscription);

/**
 * subscription_can_update_now:
 * @param subscription	the subscription
 *
 * Determine whether the user can update the subscription using manual update
 * according to the update interval indicated by the feed. This is the case when
 * lastUpdate + defaultUpdateInterval is in the past.
 * 
 * This is a pre-scheduling check (called by subscription_auto_update, in contrast
 * to subscription_can_be_updated which is run post-scheduling).
 * 
 * Does NOT check the auto update interval (from feed properties and preferences)!
 * Does NOT provide an UI indication of the check result.
 * 
 * Returns: TRUE if the subscription can be updated, FALSE otherwise
 */
gboolean subscription_can_update_now (subscriptionPtr subscription);

/**
 * subscription_can_be_updated:
 * @param subscription	the subscription
 * @param flags		update flags
 *
 * Determine whether the subscription can be updated at this moment.
 * This is a post-scheduling check (called by subscription_update), 
 * i.e., it considers whether an update job is already running or if 
 * the subscription is discontinued. If the check fails a proper error
 * is presented in the feed details and for single subscription updates 
 * as a toast notification.
 * 
 * Does NOT check any update intervals (feed, properties or preferences)
 *
 * Returns: TRUE if the subscription can be updated, FALSE otherwise
 */
gboolean subscription_can_be_updated (subscriptionPtr subscription, guint flags);

/**
 * subscription_get_effective_update_interval:
 * @param subscription	the subscription
 *
 * Get the effective update interval for a given subscription.
 * This takes into account the subscription's own update interval,
 * its default update interval, and the global default update interval.
 * 
 * Returns: the effective update interval (in minutes) or 0
 */
guint subscription_get_effective_update_interval (subscriptionPtr subscription);

/**
 * Get the update interval setting of a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the currently configured update interval (in minutes), -1 (use global default) or -2 (no auto update)
 */
gint subscription_get_update_interval(subscriptionPtr subscription);

/**
 * Set the update interval setting for the given subscription
 *
 * @param subscription	the subscription
 * @param interval	the new update interval (in minutes), -1 (use global default) or -2 (no auto update)
 */
void subscription_set_update_interval(subscriptionPtr subscription, gint interval);

/**
 * Reset the update counter for the given subscription.
 *
 * @param subscription	the subscription
 */
void subscription_reset_update_counter (subscriptionPtr subscription, guint64 *now);

void subscription_update_favicon (subscriptionPtr subscription);

/**
 * Sets the blogroll URL for a given subscription.
 *
 * @param subscription	the subscription
 * @param blogroll	the new blogroll URL (can be relative)
 */
void subscription_set_blogroll (subscriptionPtr subscription, const gchar *blogroll);

/**
 * subscription_set_discontinued:
 * 
 * Change discontinued state of subscription.
 * 
 * @param subscription	the subscription
 * @param newState 	the new state
 */
void subscription_set_discontinued (subscriptionPtr subscription, gboolean newState);

/**
 * Get the source URL of a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the source URL
 */
const gchar * subscription_get_source (subscriptionPtr subscription);

/**
 * Set a new source URL for the given subscription
 *
 * @param subscription	the subscription
 * @param source	the new source URL
 */
void subscription_set_source (subscriptionPtr subscription, const gchar *source);

/**
 * Returns the homepage URL of the given subscription.
 *
 * @param subscription	the subscription
 *
 * @returns the homepage URL or NULL
 */
const gchar * subscription_get_homepage (subscriptionPtr subscription);

/**
 * Set the homepage URL of the given subscription. If the passed
 * URL is a relative one it will be expanded using the
 * given base URL.
 *
 * @param subscription	the subscription
 * @param url		the new HTML URL
 */
void subscription_set_homepage (subscriptionPtr subscription, const gchar *url);

/**
 * Get the configured filter command for a given subscription
 *
 * @param subscription	the subscription
 *
 * @returns the filter command
 */
const gchar * subscription_get_filter (subscriptionPtr subscription);

/**
 * Set a new filter command for a given subscription
 *
 * @param subscription	the subscription
 * @param filter	the new filter command
 */
void subscription_set_filter (subscriptionPtr subscription, const gchar * filter);

/**
 * Set authentication information for a given subscription
 *
 * @param subscription	the subscription
 * @param username	the user name
 * @param password	the password
 */
void subscription_set_auth_info (subscriptionPtr subscription, const gchar *username, const gchar *password);

/**
 * Returns the subscription-specific maximum cache size.
 * If none is set it returns the global default setting.
 *
 * @param subscription	the subscription
 *
 * @returns max item count
 */
guint subscription_get_max_item_count (subscriptionPtr subscription);

/**
 * subscription_enrich_item: (skip)
 * Tries to fetch extra content for the item description
 *
 * @subscription: the subscription
 * @item: the item
 */
void subscription_enrich_item (subscriptionPtr subscription, itemPtr item);

/**
 * Frees the given subscription structure.
 *
 * @param subscription	the subscription
 */
void subscription_free (subscriptionPtr subscription);

#endif
