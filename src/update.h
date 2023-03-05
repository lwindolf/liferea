/**
 * @file update.h  generic update request and state processing
 *
 * Copyright (C) 2003-2020 Lars Windolf <lars.windolf@gmx.de>
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

   A request can have an update state assigned. This is to support
   the different bandwidth saving methods. For caching along feeds
   there are XML (de)serialization functions for the update state.

   For proxy support and authentication an update request can have
   update options assigned.

   Finally the request system has an on/offline state. When offline
   no new network requests are accepted. Filesystem and internal
   requests are still processed. Currently running downloads are
   not terminated. */

typedef enum {
	REQUEST_STATE_INITIALIZED = 0,	/**< request struct newly created */
	REQUEST_STATE_PENDING,		/**< request added to download queue */
	REQUEST_STATE_PROCESSING,	/**< request currently in download */
	REQUEST_STATE_DEQUEUE,		/**< download finished, callback processing */
	REQUEST_STATE_FINISHED		/**< request processing finished */
} request_state;

struct updateJob;
struct updateResult;

typedef guint32 updateFlags;

/**
 * Generic update result processing callback type.
 * This callback must not free the result structure. It will be
 * free'd by the download system after the callback returns.
 *
 * @param result	the update result
 * @param user_data	update processing callback data
 * @param flags		update processing flags
 */
typedef void (*update_result_cb) (const struct updateResult * const result, gpointer user_data, updateFlags flags);

/** defines update options to be passed to an update request */
typedef struct updateOptions {
	gchar		*username;	/**< username for HTTP auth */
	gchar		*password;	/**< password for HTTP auth */
	gboolean	dontUseProxy;	/**< no proxy flag */
} *updateOptionsPtr;

/** defines all state data an updatable object (e.g. a feed) needs */
typedef struct updateState {
	glong		lastModified;		/**< Last modified string as sent by the server */
	guint64  	lastPoll;		/**< time at which the feed was last updated */
	guint64 	lastFaviconPoll;	/**< time at which the feeds favicon was last updated */
	gchar		*cookies;		/**< cookies to be used */
	gchar		*etag;			/**< ETag sent by the server */
	gint		maxAgeMinutes;		/**< default update interval, greatest value sourced from HTTP and XML */
	gint		synFrequency;		/**< syn:updateFrequency */
	gint		synPeriod;		/**< syn:updatePeriod */
	gint		timeToLive;		/**< ttl */
} *updateStatePtr;

G_BEGIN_DECLS
#define UPDATE_REQUEST_TYPE (update_request_get_type ())
G_DECLARE_FINAL_TYPE (UpdateRequest, update_request, UPDATE, REQUEST, GObject)

struct _UpdateRequest {
	GObject		parent;

	gchar 		*source;	/**< Location of the source. If it starts with
					     '|', it is a command. If it contains "://",
					     then it is parsed as a URL, otherwise it is a
					     filename. */
	gchar           *postdata;      /**< HTTP POST request data (NULL for non-POST requests) */
	gchar           *authValue;     /**< Custom value for Authorization: header */
	updateOptionsPtr options;	/**< Update options for the request */
	gchar		*filtercmd;	/**< Command will filter output of URL */
	updateStatePtr	updateState;	/**< Update state of the requested object (etags, last modified...) */
	gboolean	allowCommands;	/**< Allow this requests to run commands */
};

/** structure to store results of the processing of an update request */
typedef struct updateResult {
	gchar 		*source;	/**< Location of the downloaded document, in case of redirects different from
					     the one given along with the update request */

	int		httpstatus;	/**< HTTP status. Set to 200 for any valid command, file access, etc.... Set to 0 for unknown */
	gchar		*data;		/**< Downloaded data */
	size_t		size;		/**< Size of downloaded data */
	gchar		*contentType;	/**< Content type of received data */
	gchar		*filterErrors;	/**< Error messages from filter execution */

	updateStatePtr	updateState;	/**< New update state of the requested object (etags, last modified...) */
} *updateResultPtr;

/** structure to store state fo running command feeds */
typedef struct updateCommandState {
	GPid		pid;		/**< child PID */
	guint		timeout_id;	/**< glib event source id for the timeout event */
	guint		io_watch_id;	/**< glib event source id for stdout */
	guint		child_watch_id;	/**< glib event source id for child termination */
	gint		fd;		/**< fd for child stdout */
	GIOChannel	*stdout_ch;	/**< child stdout as a channel */
} updateCommandState;


/** structure describing an HTTP update job */
typedef struct updateJob {
	UpdateRequest		*request;
	updateResultPtr		result;
	gpointer		owner;		/**< owner of this job (used for matching when cancelling) */
	update_result_cb	callback;	/**< result processing callback */
	gpointer		user_data;	/**< result processing user data */
	updateFlags		flags;		/**< request and result processing flags */
	gint			state;		/**< State of the job (enum request_state) */
	updateCommandState	cmd;		/**< values for command feeds */
} *updateJobPtr;

/**
 * Create new update state
 */
updateStatePtr update_state_new (void);

/**
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
 * Frees the given update state.
 *
 * @param updateState  the update state
 */
void update_state_free (updateStatePtr updateState);

/**
 * Copies the given update options.
 *
 * @returns a new update options structure (to be free'd using update_options_free())
 */
updateOptionsPtr update_options_copy (updateOptionsPtr options);

/**
 * Frees the given update options
 *
 * @param options	the update options
 */
void update_options_free (updateOptionsPtr options);

/**
 * Initialises the download subsystem.
 *
 * Must be called before gtk_init() and after thread initialization
 * as threads are used and for proper network-manager initialization.
 */
void update_init (void);

/**
 * Stops all update processing and frees all used memory.
 */
void update_deinit (void);

/**
 * Creates a new request structure.
 *
 * @oaram source	URI to download
 * @param state		a previous update state of the requested URL (or NULL)
 *                      will not be owned, but copied!
 * @param options	update options to be used (or NULL)
 *			will not be owned but copied!
 *
 * @returns a new request GObject to be passed to update_execute_request()
 */
UpdateRequest * update_request_new (const gchar *source, updateStatePtr state, updateOptionsPtr options);

/**
 * Sets the source for an updateRequest. Only use this when the source
 * is not known at update_request_new() calling time.
 *
 * @param request       the update request
 * @param source        the new source URL
 */
void update_request_set_source (UpdateRequest *request, const gchar* source);

/**
 * Sets a custom authorization header value.
 *
 * @param request        the update request
 * @param authValue      the authorization header value
 */
void update_request_set_auth_value (UpdateRequest *request, const gchar* authValue);

/**
 * Allows *this* request to run local commands.
 *
 * At first it may look this flag should be in updateOptions, but we can
 * take a safer path: feed commands are restricted to a few use cases while
 * options are propagated to downstream requests (feed enrichment, comments,
 * etc.), so it is a good idea to prevent these from running commands in the
 * local system via tricky URLs without needing to validate these options
 * everywhere (which is error-prone).
 *
 * @param request      the update request
 * @param can_run      TRUE if the request can run commands, FALSE otherwise.
 */
void update_request_allow_commands (UpdateRequest *request, gboolean allowCommands);

/**
 * Creates a new update result for the given update request.
 *
 * @returns update result (to be free'd using update_result_free())
 */
updateResultPtr update_result_new (void);

/**
 * Free's the given update result.
 *
 * @param result	the result
 */
void update_result_free (updateResultPtr result);

/**
 * Executes the given request. The request might be
 * delayed if other requests are pending.
 *
 * @param owner		request owner (allows cancelling, can be NULL)
 * @param request	the request to execute
 * @param callback	result processing callback
 * @param user_data	result processing callback parameters (or NULL)
 * @param flags		request/result processing flags
 *
 * @returns the new update job
 */
updateJobPtr update_execute_request (gpointer owner,
                                     UpdateRequest *request,
                                     update_result_cb callback,
                                     gpointer user_data,
                                     updateFlags flags);

/* Update job handling */

/**
 * To be called when an update job has been executed. Triggers
 * the job specific result processing callback.
 *
 * @param job		the update job
 */
void update_process_finished_job (updateJobPtr job);

/**
 * Cancel all pending requests for the given owner.
 *
 * @param owner		pointer passed in update_request_new()
 */
void update_job_cancel_by_owner (gpointer owner);

/**
 * Method to query the update state of currently processed jobs.
 *
 * @returns update job state (see enum request_state)
 */
gint update_job_get_state (updateJobPtr job);

/**
* update_jobs_get_count:
*
* Query current count and max count of subscriptions in update queue
*
* @count:	gint ref to pass back nr of subscriptions in update
* @maxcount:	gint ref to pass back max nr of subscriptions in update
*/
void update_jobs_get_count (guint *count, guint *maxcount);

G_END_DECLS

#endif
