/*
 * @file subscription_type.h  subscription type interface
 *
 * Copyright (C) 2008-2020 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2008 Arnold Noronha <arnstein87@gmail.com>
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

#ifndef _SUBSCRIPTION_TYPE_H
#define _SUBSCRIPTION_TYPE_H

#include "subscription.h"

/*
 * Liferea supports different types of subscriptions that differ
 * in their updating behaviour and update state.
 */

/* subscription type interface */
typedef struct subscriptionType {

	/* Note: the default implementation of this interface is
	   provided by feed.c */

	/*
	 * Preparation callback for a update type. Allows a specific
	 * subscription type implementation to make changes to the
	 * already created update request (e.g. URI adaptions or
	 * setting cookies).
	 *
	 * This callback also allows the subscription type implementation
	 * to cancel a request (e.g. when it is clear that an update
	 * is not necessary due to some implicit node source state).
	 *
	 * @param subscription	the subscription that is being updated
	 * @param request	the request
	 *
	 * @returns FALSE if the request is to be aborted
	 */
	gboolean (*prepare_update_request)(subscriptionPtr subscription, UpdateRequest * request);

	/*
	 * Subscription type specific update result processing callback.
	 *
	 * @param subscription	the subscription that was updated
	 * @param result	the update result
	 * @param flags		the update flags
	 */
	void (*process_update_result)(subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags);

} *subscriptionTypePtr;

#define SUBSCRIPTION_TYPE(subscription)	(subscription->type)

#endif
