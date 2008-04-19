/**
 * @file subscription_type.h  subscription type interface
 * 
 * Copyright (C) 2008 Lars Lindner <lars.lindner@gmail.com>
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

/**
 * Liferea supports different types of subscriptions that differ
 * in their updating behaviour and update state.
 */

/** subscription type interface */
typedef struct subscriptionType {

	/* For method documentation see the wrappers defined below! 
	   All methods are mandatory for each subscription type. */
	void	(*prepare_update_request)(subscriptionPtr subscription, const struct updateRequest * request);
	void 	(*process_update_result)(subscriptionPtr subscription, const struct updateResult * const result, updateFlags flags);
	
	/**
	 * Called to allow subscription type to clean up it's specific data.
	 * The subscription structure itself is destroyed after this call.
	 *
	 * @param subscription		the subscription
	 */
	void	(*free)			(subscriptionPtr subscription);
} *subscriptionTypePtr;

#define SUBSCRIPTION_TYPE(subscription)	(subscription->type)

#endif
