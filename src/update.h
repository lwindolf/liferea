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

#include <time.h>
#include <glib.h>

struct request;

/**
 *  This callback should not free the request structure. It will be
 *  freed by the download system after the callback returns.
 */
typedef void (*request_cb)(struct request *request);

/** 
 * The feed request structure is used in two places, on the one
 * hand to put update requests into the request and result queue 
 * between GUI and update thread and on the other hand to 
 * persistently store HTTP status information written by
 * the SnowNews netio.c code which is displayed in the GUI. 
 */

struct request {
	
	/* Set by requester */
	gchar *source; /**< Location of the source. If it starts with
				   '|', it is a command. If it contains "://",
				   then it is parsed as a URL, otherwise it is a
				   filename. Eventually, everything should be a
				   URL. Use file:// and exec:// */
	gchar *filtercmd; /**< Command will filter output of URL */
	request_cb callback; /**< Function to be called after retreival */
	gpointer user_data; /**< Accessed by the callback. Would contain a feedPtr for the feed_process_request_result callbacks. */
	guint32 flags; /**< Flags to be passed to the callback */
	
	/* Set by download system*/
	int httpstatus; /**< last HTTP status. Set to 200 for any valid command, file access, etc.... Set to 0 for unknown */
	GTimeVal lastmodified; /**< Time of last modification. Stored in UTC? */
	gchar *data;
	size_t size;
};

/** Initialises the download subsystem, including its thread(s). */
void download_init(); 

/** 
 * Creates a new request structure and sets the feed the
 * request belongs to with fp. If there is no assigned
 * feed fp may be NULL.
 *
 * @param fp	feed pointer
 * @return pointer to new request structure
 */
gpointer download_request_new();

/**
 * Used to free a request structure. Frees all members, including data.
 *
 * @param request	pointer to a request structure
 */
void download_request_free(struct request *request);

/**
 * Sets the online status according to mode.
 *
 * @param mode	TRUE for online, FALSE for offline
 */ 
void download_set_online(gboolean mode);

/**
 * Queries the online status.
 *
 * @return TRUE if online
 */
gboolean download_is_online(void);

/**
 * Function to pass a request to the update request
 * processing thread. This request will be queued if;
 * other requests are pending. If it was processed 
 * successfully a result will be added to the result 
 * queue.
 *
 * @param new_request	pointer to a request structure
 */
void download_queue(struct request *new_request);

/**
 * Process a download request, and pass it to the URL handler, if
 * needed. This should not be used very often because it will block.
 */

void download_process(struct request *request);
#endif
