/**
 * @file update.h  feed update request processing
 *
 * Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#ifndef _UPDATE_H
#define _UPDATE_H

#include <glib.h>
#include "feed.h"

/** 
 * The feed request structure is used in two places, on the one
 * hand to put update requests into the request and result queue 
 * between GUI and update thread and on the other hand to 
 * persistently store HTTP status information written by
 * the SnowNews netio.c code which is displayed in the GUI. 
 */

struct feed_request {
	
	char * 	feedurl;		/**< Non hashified URL set from the requestion function (Or a command, etc....) */
	char * filtercmd;		/**< Command to run to convert the input into a valid feed */
	char * 	lastmodified; 		/**< Content of header as sent by the server. */
	int 	lasthttpstatus;		/**< last HTTP status :-)*/
	char *	authinfo;		/**< HTTP authinfo string. */
	char *	servauth;		/**< Server supplied authorization header. */
	char *	cookies;		/**< Login cookies for this feed. */
	int 	problem;		/**< Set if there was a problem downloading the feed. */
	char *	data;			/**< newly downloaded feed data to be parsed */
	gboolean updateRequested; 	/**< Lock used in order to insure that a feed is being updated only once */
	feedPtr	fp;			/**< pointer to feed structure which is to be updated */
	int size;				/**< length of received data*/
};

extern GAsyncQueue      *results;

/** 
 * Creates a new request structure and sets the feed the
 * request belongs to with fp. If there is no assigned
 * feed fp may be NULL.
 *
 * @param fp	feed pointer
 * @return pointer to new request structure
 */
gpointer update_request_new(feedPtr fp);

/**
 * Used to free a request structure.
 *
 * @param request	pointer to a request structure
 */
void update_request_free(gpointer request);

/**
 * Initializes and starts the update request processing
 * thread. Should be called only once! 
 */
void update_thread_init(void);

/**
 * Sets the online status according to mode.
 *
 * @param mode	TRUE for online, FALSE for offline
 */ 
void update_thread_set_online(gboolean mode);

/**
 * Queries the online status.
 *
 * @return TRUE if online
 */
gboolean update_thread_is_online(void);

/**
 * Function to pass a request to the update request
 * processing thread. This request will be queued if;
 * other requests are pending. If it was processed 
 * successfully a result will be added to the result 
 * queue.
 *
 * @param new_request	pointer to a request structure
 */
void update_thread_add_request(struct feed_request *new_request);

/**
 * Function to wait for an update request result. This
 * function blocks until a result is available.
 *
 * @return pointer to a request structure
 */
struct feed_request * update_thread_get_result(void);

#endif
