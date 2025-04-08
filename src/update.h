/**
 * @file update_request.h  generic update request processing
 *
 * Copyright (C) 2003-2024 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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
#include <glib-object.h>

/* Update requests do represent feed updates, favicon and enclosure
   downloads. A request can be started synchronously or asynchronously.
   In the latter case it can be cancelled at any time. If the processing
   of a update request is done the request callback will be triggered.

   A request always has an update state. This is to support
   the different bandwidth saving methods. Update states kept per
   session in the Node objects and are persisted in the database.

   For proxy support and authentication an update request can set
   optional update options.
*/

typedef enum {
	UPDATE_REQUEST_RESET_TITLE   = (1<<0),	/*<< Feed's title should be reset to default upon update */
	UPDATE_REQUEST_PRIORITY_HIGH = (1<<1),	/*<< set to signal that this is an important user triggered request */
	UPDATE_REQUEST_NO_FEED       = (1<<2)	/*<< Requesting something not a feed (just for statistics) */
} updateFlags;

/* defines update options to be passed to an update request */
typedef struct updateOptions {
	gchar		*username;	/*<< username for HTTP auth */
	gchar		*password;	/*<< password for HTTP auth */
	gboolean	dontUseProxy;	/*<< no proxy flag */
} *updateOptionsPtr;

/* defines all state data an updatable object (e.g. a feed) needs */
typedef struct updateState {
	glong		lastModified;		/*<< Last modified string as sent by the server */
	gint64  	lastPoll;		/*<< time at which the feed was last updated */
	gint64 		lastFaviconPoll;	/*<< time at which the feeds favicon was last updated */
	gchar		*cookies;		/*<< cookies to be used */
	gchar		*etag;			/*<< ETag sent by the server */
	gint		maxAgeMinutes;		/*<< default update interval, greatest value sourced from HTTP and XML */
	gint		synFrequency;		/*<< syn:updateFrequency */
	gint		synPeriod;		/*<< syn:updatePeriod */
	gint		timeToLive;		/*<< ttl */
} *updateStatePtr;

G_BEGIN_DECLS

#define UPDATE_REQUEST_TYPE (update_request_get_type ())
G_DECLARE_FINAL_TYPE (UpdateRequest, update_request, UPDATE, REQUEST, GObject)

struct _UpdateRequest {
	GObject		parent;

	gchar 		*source;	/*<< Location of the source. If it starts with
					     '|', it is a command. If it contains "://",
					     then it is parsed as a URL, otherwise it is a
					     filename. */
	gchar           *postdata;      /*<< HTTP POST request data (NULL for non-POST requests) */
	gchar           *authValue;     /*<< Custom value for Authorization: header */
	updateOptionsPtr options;	/*<< Update options for the request */
	gchar		*filtercmd;	/*<< Command will filter output of URL */
	updateStatePtr	updateState;	/*<< Update state of the requested object (etags, last modified...) */
	gboolean	allowCommands;	/*<< Allow this requests to run commands */
};

/* structure to store state fo running command feeds */
typedef struct updateCommandState {
	GPid		pid;		/*<< child PID */
	guint		timeout_id;	/*<< glib event source id for the timeout event */
	guint		io_watch_id;	/*<< glib event source id for stdout */
	guint		child_watch_id;	/*<< glib event source id for child termination */
	gint		fd;		/*<< fd for child stdout */
	GIOChannel	*stdout_ch;	/*<< child stdout as a channel */
} updateCommandState;

/**
 * update_state_new: (skip)
 * 
 * Create new update state
 */
updateStatePtr update_state_new (void);

/**
 * update_state_copy: (skip)
 * 
 * Copy update state
 */
updateStatePtr update_state_copy (updateStatePtr state);

glong update_state_get_lastmodified (updateStatePtr state);
void update_state_set_lastmodified (updateStatePtr state, glong lastmodified);

const gchar * update_state_get_etag (updateStatePtr state);
void update_state_set_etag (updateStatePtr state, const gchar *etag);

gint update_state_get_cache_maxage (updateStatePtr state);
void update_state_set_cache_maxage (updateStatePtr state, const gint maxage);

const gchar * update_state_get_cookies (updateStatePtr state);
void update_state_set_cookies (updateStatePtr state, const gchar *cookies);

/**
 * update_state_free:
 * @updateState:  the update state
 * 
 * Frees the given update state.
 */
void update_state_free (updateStatePtr updateState);

/**
 * update_options_copy: (skip)
 * @options:  the options to copy
 * 
 * Copies the given update options.
 *
 * Returns: a new update options structure (to be free'd using update_options_free())
 */
updateOptionsPtr update_options_copy (updateOptionsPtr options);

/**
 * update_options_free: (skip)
 * @options:	the update options
 * 
 * Frees the given update options
 */
void update_options_free (updateOptionsPtr options);

/**
 * update_request_new:
 * @source:	URI to download
 * @state:	a previous update state of the requested URL (or NULL) will not be owned, but copied!
 * @options:	update options to be used (or NULL) will not be owned but copied!
 *
 * Creates a new request structure.
 *
 * Returns: a new request GObject to be passed to update_execute_request()
 */
UpdateRequest * update_request_new (const gchar *source, updateStatePtr state, updateOptionsPtr options);

/**
 * update_request_set_source:
 * @request:       the update request
 * @source:        the new source URL
 * 
 * Sets the source for an updateRequest. Only use this when the source
 * is not known at update_request_new() calling time.
 */
void update_request_set_source (UpdateRequest *request, const gchar* source);

/**
 * update_request_set_auth_value:
 * @request:        the update request
 * @authValue:      the authorization header value
 * 
 * Sets a custom authorization header value.
 */
void update_request_set_auth_value (UpdateRequest *request, const gchar* authValue);

/**
 * update_request_allow_commands:
 * @request:            the update request
 * @allowCommands:      TRUE if the request can run commands, FALSE otherwise.
 * 
 * Allows *this* request to run local commands.
 *
 * At first it may look this flag should be in updateOptions, but we can
 * take a safer path: feed commands are restricted to a few use cases while
 * options are propagated to downstream requests (feed enrichment, comments,
 * etc.), so it is a good idea to prevent these from running commands in the
 * local system via tricky URLs without needing to validate these options
 * everywhere (which is error-prone).
 */
void update_request_allow_commands (UpdateRequest *request, gboolean allowCommands);


#define UPDATE_RESULT_TYPE (update_result_get_type ())
G_DECLARE_FINAL_TYPE (UpdateResult, update_result, UPDATE, RESULT, GObject)

struct _UpdateResult {
	GObject parent_instance;

	gchar 		*source;	/*<< Location of the downloaded document, in case of redirects different from
						 the one given along with the update request */
	int		httpstatus;	/*<< HTTP status. Set to 200 for any valid command, file access, etc.... Set to 0 for unknown */
	gchar		*data;		/*<< Downloaded data */
	size_t		size;		/*<< Size of downloaded data */
	gchar		*contentType;	/*<< Content type of received data */
	gchar		*filterErrors;	/*<< Error messages from filter execution */
	updateStatePtr	updateState;	/*<< New update state of the requested object (etags, last modified...) */
};

/**
 * update_result_cb:
 * @result:	the update result
 * @user_data:	update processing callback data
 * @flags:	update processing flags
 *
 * Generic update result processing callback type.
 * This callback must not free the result structure. It will be
 * free'd by the download system after the callback returns.
 */
typedef void (*update_result_cb) (const UpdateResult * const result, gpointer user_data, updateFlags flags);

#define update_result_new() UPDATE_RESULT (g_object_new (UPDATE_RESULT_TYPE, NULL))

G_END_DECLS

// for convenience (after splitting up the code)
#include "update_job.h"
#include "update_job_queue.h"

#endif
