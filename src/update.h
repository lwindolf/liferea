/**
 * @file update.h  generic update request and state processing
 *
 * Copyright (C) 2003-2014 Lars Windolf <lars.windolf@gmx.de>
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
	GTimeVal	lastPoll;		/**< time at which the feed was last updated */
	GTimeVal	lastFaviconPoll;	/**< time at which the feeds favicon was last updated */
	gchar		*cookies;		/**< cookies to be used */	
	gchar		*etag;			/**< ETag sent by the server */
} *updateStatePtr;

/** structure describing a HTTP update request */
typedef struct updateRequest {
	gchar 		*source;	/**< Location of the source. If it starts with
					     '|', it is a command. If it contains "://",
					     then it is parsed as a URL, otherwise it is a
					     filename. Eventually, everything should be a
					     URL. Use file:// and exec:// */
	gchar           *postdata;      /**< HTTP POST request data (NULL for non-POST requests) */
	gchar           *authValue;     /**< Custom value for Authorization: header */
	updateOptionsPtr options;	/**< Update options for the request */
	gchar		*filtercmd;	/**< Command will filter output of URL */
	updateStatePtr	updateState;	/**< Update state of the requested object (etags, last modified...) */
	gchar		*method;		/**< Set to 'PUT' if you don't want GET or POST, other values of it are ignored for now. */
	gchar		*contentType;	/**< Request Content-Type header (ignored if NULL) */
} *updateRequestPtr;

/** structure to store results of the processing of an update request */
typedef struct updateResult {
	gchar 		*source;	/**< Location of the downloaded document, in case of redirects different from 
					     the one given along with the update request */
	
	int		returncode;	/**< Download status (0=success, otherwise error) */
	int		httpstatus;	/**< HTTP status. Set to 200 for any valid command, file access, etc.... Set to 0 for unknown */
	gchar		*data;		/**< Downloaded data */
	size_t		size;		/**< Size of downloaded data */
	gchar		*contentType;	/**< Content type of received data */
	gchar		*filterErrors;	/**< Error messages from filter execution */
	
	updateStatePtr	updateState;	/**< New update state of the requested object (etags, last modified...) */
} *updateResultPtr;

/** structure describing an HTTP update job */
typedef struct updateJob {
	updateRequestPtr	request;
	updateResultPtr		result;
	gpointer		owner;		/**< owner of this job (used for matching when cancelling) */
	update_result_cb	callback;	/**< result processing callback */
	gpointer		user_data;	/**< result processing user data */
	updateFlags		flags;		/**< request and result processing flags */
	gint			state;		/**< State of the job (enum request_state) */
} *updateJobPtr;

/**
 * Creates a new update state structure 
 *
 * @return a new state structure (to be free'd using update_state_free())
 */
updateStatePtr update_state_new (void);

glong update_state_get_lastmodified (updateStatePtr state);
void update_state_set_lastmodified (updateStatePtr state, glong lastmodified);

const gchar * update_state_get_etag (updateStatePtr state);
void update_state_set_etag (updateStatePtr state, const gchar *etag);

const gchar * update_state_get_cookies (updateStatePtr state);
void update_state_set_cookies (updateStatePtr state, const gchar *cookies);

/**
 * Copies the given update state.
 *
 * @returns a new update state structure (to be free'd using update_state_free())
 */
updateStatePtr update_state_copy (updateStatePtr state);

/**
 * Frees the given update state.
 *
 * @param updateState	the update state
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
 * @returns a new request (to be passed and free'd by update_execute_request())
 */
updateRequestPtr update_request_new (void);

/**
 * Free's the given update request.
 *
 * @param request	the update request
 */
void update_request_free (updateRequestPtr request);

/**
 * Sets the source for an updateRequest
 * 
 * @param request       the update request
 * @param source        the new source
 */
void update_request_set_source(updateRequestPtr request, const gchar* source);

/**
 * Sets a custom authorization header value.
 *
 * @param request        the update request
 * @param authValue      the authorization header value
 */
void update_request_set_auth_value(updateRequestPtr request, const gchar* authValue);

/**
 * Creates a new update result for the given update request.
 *
 * @param request	the update request
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
                                     updateRequestPtr request,
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

#endif
