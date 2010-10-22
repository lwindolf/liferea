/**
 * @file ttrss_source_feed.c  tt-rss feed subscription routines
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

#include <glib.h>
#include <string.h>

#include "common.h"
#include "debug.h"

#include "feedlist.h"
#include "ttrss_source.h"
#include "subscription.h"
#include "node.h"
#include "metadata.h"
#include "db.h"
#include "item_state.h"

static void
ttrss_feed_subscription_process_update_result (subscriptionPtr subscription, const struct updateResult* const result, updateFlags flags)
{
	g_warning ("FIXME: ttrss_feed_subscription_prepare_update_result(): Implement me!");
}

static gboolean
ttrss_feed_subscription_prepare_update_request (subscriptionPtr subscription, 
                                                 struct updateRequest *request)
{
	g_warning ("FIXME: ttrss_feed_subscription_prepare_update_request(): Implement me!");
	return TRUE;
}

struct subscriptionType ttrssSourceFeedSubscriptionType = {
	ttrss_feed_subscription_prepare_update_request,
	ttrss_feed_subscription_process_update_result
};

