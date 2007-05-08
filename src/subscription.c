/**
 * @file subscription.c common subscription handling
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

#include <string.h>
#include "common.h"
#include "db.h"
#include "debug.h"
#include "favicon.h"
#include "feedlist.h"
#include "subscription.h"
#include "support.h"
#include "net/cookies.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_node.h"

#define FEED_PROTOCOL_PREFIX "feed://"

subscriptionPtr
subscription_new (const gchar *source,
                  const gchar *filter,
                  updateOptionsPtr options)
{
	subscriptionPtr	subscription;
	
	subscription = g_new0 (struct subscription, 1);
	subscription->updateOptions = options;
	
	if (!subscription->updateOptions)
		subscription->updateOptions = g_new0 (struct updateOptions, 1);
		
	subscription->updateState = g_new0 (struct updateState, 1);	
	subscription->updateInterval = -1;
	subscription->defaultInterval = -1;
	
	if (source) {
		gchar *tmp, *uri = g_strdup (source);
		g_strstrip (uri);	/* strip confusing whitespaces */
		
		/* strip feed protocol prefix */
		tmp = uri;
		if (tmp == strstr (tmp, FEED_PROTOCOL_PREFIX))
			tmp += strlen (FEED_PROTOCOL_PREFIX);
			
		subscription_set_source (subscription, tmp);
		g_free (uri);
	}
	
	if (filter)
		subscription_set_filter (subscription, filter);
	
	return subscription;
}

/* Checks wether updating a feed makes sense. */
gboolean
subscription_can_be_updated (nodePtr node)
{
	subscriptionPtr	subscription = node->subscription;

	if (node->updateRequest) {
		ui_mainwindow_set_status_bar (_("Subscription \"%s\" is already being updated!"), node_get_title (node));
		return FALSE;
	}
	
	if (subscription->discontinued) {
		ui_mainwindow_set_status_bar (_("The subscription \"%s\" was discontinued. Liferea won't update it anymore!"), node_get_title (node));
		return FALSE;
	}

	if (!subscription_get_source (subscription)) {
		g_warning ("Feed source is NULL! This should never happen - cannot update!");
		return FALSE;
	}
	return TRUE;
}	

void
subscription_reset_update_counter (subscriptionPtr subscription) 
{
	g_get_current_time (&subscription->updateState->lastPoll);
	db_update_state_save (subscription->node->id, subscription->updateState);
	debug1 (DEBUG_UPDATE, "Resetting last poll counter to %ld.\n", subscription->updateState->lastPoll.tv_sec);
}

void
subscription_prepare_request (subscriptionPtr subscription,
                              struct request *request,
                              guint flags)
{
	debug1 (DEBUG_UPDATE, "preparing request for \"%s\"\n", subscription_get_source (subscription));

	subscription_reset_update_counter (subscription);

	/* prepare request url (strdup because it might be
  	   changed on permanent HTTP redirection in netio.c) */
	request->source = g_strdup (subscription_get_source (subscription));
	request->updateState = subscription->updateState;
	request->flags = flags;
	request->priority = (flags & FEED_REQ_PRIORITY_HIGH)? 1 : 0;
	request->allowRetries = (flags & FEED_REQ_ALLOW_RETRIES)? 1 : 0;
	if (subscription_get_filter (subscription))
		request->filtercmd = g_strdup (subscription_get_filter (subscription));
}

gint
subscription_get_update_interval (subscriptionPtr subscription)
{
	return subscription->updateInterval;
}

void
subscription_set_update_interval (subscriptionPtr subscription, gint interval)
{	
	if (0 == interval) {
		interval = -1;	/* This is evil, I know, but when this method
				   is called to set the update interval to 0
				   we mean "never updating". The updating logic
				   expects -1 for "never updating" and 0 for
				   updating according to the global update
				   interval... */
	}
	subscription->updateInterval = interval;
	feedlist_schedule_save ();
}

guint
subscription_get_default_update_interval (subscriptionPtr subscription)
{
	return subscription->defaultInterval;
}

void
subscription_set_default_update_interval (subscriptionPtr subscription, guint interval)
{
	subscription->defaultInterval = interval;
}

const gchar *
subscription_get_orig_source (subscriptionPtr subscription)
{
	return subscription->origSource; 
}

const gchar *
subscription_get_source (subscriptionPtr subscription)
{
	return subscription->source;
}

const gchar *
subscription_get_filter (subscriptionPtr subscription)
{
	return subscription->filtercmd;
}

void
subscription_set_orig_source (subscriptionPtr subscription, const gchar *source)
{
	g_free (subscription->origSource);
	subscription->origSource = g_strchomp (g_strdup (source));
	feedlist_schedule_save ();
}

void
subscription_set_source (subscriptionPtr subscription, const gchar *source)
{
	g_free (subscription->source);
	subscription->source = g_strchomp (g_strdup (source));
	feedlist_schedule_save ();
	
	g_free (subscription->updateState->cookies);
	if ('|' != source[0])
		/* check if we've got matching cookies ... */
		subscription->updateState->cookies = cookies_find_matching (source);
	else 
		subscription->updateState->cookies = NULL;
	
	if (NULL == subscription_get_orig_source (subscription))
		subscription_set_orig_source (subscription, source);
}

void
subscription_set_filter (subscriptionPtr subscription, const gchar *filter)
{
	g_free (subscription->filtercmd);
	subscription->filtercmd = g_strdup (filter);
	feedlist_schedule_save ();
}

/**
 * Creates a new error description according to the passed
 * HTTP status and the feeds parser errors. If the HTTP
 * status is a success status and no parser errors occurred
 * no error messages is created.
 *
 * @param feed		feed
 * @param httpstatus	HTTP status
 * @param resultcode	the update code's return code (see update.h)
 */
void
subscription_update_error_status (subscriptionPtr subscription,
                                  gint httpstatus,
                                  gint resultcode,
                                  gchar *filterError)
{
	const gchar	*errmsg = NULL;
	gboolean	errorFound = FALSE;

	if (subscription->filterError)
		g_free (subscription->filterError);
	if (subscription->httpError)
		g_free (subscription->httpError);
	if (subscription->updateError)
		g_free (subscription->updateError);
		
	subscription->filterError = g_strdup (filterError);
	subscription->updateError = NULL;
	subscription->httpError = NULL;
	subscription->httpErrorCode = httpstatus;
	
	if (((httpstatus >= 200) && (httpstatus < 400)) && /* HTTP codes starting with 2 and 3 mean no error */
	    (NULL == subscription->filterError))
		return;
	
	if ((200 != httpstatus) || (resultcode != NET_ERR_OK)) {	
		/* first specific codes (guarantees tmp to be set) */
		errmsg = common_http_error_to_str (httpstatus);

		/* second netio errors */
		if (common_netio_error_to_str (resultcode))
			errmsg = common_netio_error_to_str (resultcode);

		errorFound = TRUE;
		subscription->httpError = g_strdup (errmsg);
	}
	
	/* if none of the above error descriptions matched... */
	if (!errorFound)
		subscription->updateError = g_strdup (_("There was a problem while reading this subscription. Please check the URL and console output."));
}

static void
subscription_favicon_downloaded (gpointer user_data)
{
	nodePtr	node = (nodePtr)user_data;

	node_set_icon (node, favicon_load_from_cache (node->id));
	ui_node_update (node->id);
	db_update_state_save (node->id, node->subscription->updateState);
}

void
subscription_update_favicon (subscriptionPtr subscription)
{	
	debug1 (DEBUG_UPDATE, "trying to download favicon.ico for \"%s\"\n", node_get_title (subscription->node));
	ui_mainwindow_set_status_bar (_("Updating favicon for \"%s\""), node_get_title (subscription->node));
	g_get_current_time (&subscription->updateState->lastFaviconPoll);
	favicon_download (subscription->node->id,
	                  node_get_base_url (subscription->node),
			  subscription_get_source (subscription),
			  subscription->updateOptions,
	                  subscription_favicon_downloaded, 
			  (gpointer)subscription->node);
}

void
subscription_free (subscriptionPtr subscription)
{
	g_free (subscription->updateError);
	g_free (subscription->filterError);
	g_free (subscription->httpError);
	g_free (subscription->source);
	g_free (subscription->origSource);
	g_free (subscription->filtercmd);

	g_free (subscription->updateOptions);
	update_state_free (subscription->updateState);
	
	g_free (subscription);
}
