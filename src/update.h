/**
 * @file update.h  generic update request processing
 *
 * Copyright (C) 2003-2007 Lars Lindner <lars.lindner@gmail.com>
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

#include <libxml/parser.h>

/* Requests represent feed updates, favicon and enclosure downloads
   and (if GtkHTML2 is used) HTML browser traffic. A request can be
   started synchronously or asynchronously. In the latter case it
   can be cancelled at any time. If the processing of a request is
   done the request callback will be triggered. 
   
   A request can have an update state assigned. This is to support
   the different bandwidth saving methods. For caching along feeds
   there are XML (de)serialization functions for the update state. 
   
   Finally the request system has an on/offline state. When offline
   no new network requests are accepted. Filesystem and internal 
   requests are still processed. Currently running downloads are 
   not terminated. */

/* Maximum number of retries that can be done */
#define REQ_MAX_NUMBER_OF_RETRIES 3

/* Delay (in seconds) to wait before the first retry.
 * It will then increase for the later ones. */
#define REQ_MIN_DELAY_FOR_RETRY 30

/* Maximum delay (in seconds) beetween two retries. Useful to avoid 
 * stupidly long waits if REQ_MAX_NUMBER_OF_RETRIES is high. */
#define REQ_MAX_DELAY_FOR_RETRY 500

typedef enum {
	REQUEST_STATE_INITIALIZED = 0,	/**< request struct newly created */
	REQUEST_STATE_PENDING,		/**< request added to download queue */
	REQUEST_STATE_PROCESSING,	/**< request currently in download */
	REQUEST_STATE_DEQUEUE,		/**< download finished, callback processing */
	REQUEST_STATE_FINISHED		/**< request processing finished */
} request_state;

typedef enum {
	NET_ERR_OK = 0,
	/* Init errors */
	NET_ERR_URL_INVALID,
	/* Connect errors */
	NET_ERR_PROTO_INVALID,
	NET_ERR_SOCK_ERR,
	NET_ERR_HOST_NOT_FOUND,
	NET_ERR_CONN_REFUSED,
	NET_ERR_CONN_FAILED,
	NET_ERR_TIMEOUT,
	NET_ERR_UNKNOWN,
	/* Transfer errors */
	NET_ERR_REDIRECT_COUNT_ERR,
	NET_ERR_REDIRECT_ERR,
	NET_ERR_HTTP_410,
	NET_ERR_HTTP_404,
	NET_ERR_HTTP_NON_200,
	NET_ERR_HTTP_PROTO_ERR,
	NET_ERR_AUTH_FAILED,
	NET_ERR_AUTH_NO_AUTHINFO,
	NET_ERR_AUTH_GEN_AUTH_ERR,
	NET_ERR_AUTH_UNSUPPORTED,
	NET_ERR_GZIP_ERR
} netio_error_type;

struct request;

/**
 *  This callback must not free the request structure. It will be
 *  freed by the download system after the callback returns.
 */
typedef void (*request_cb)(struct request *request);

/** defines update options to be passed to an update request */
typedef struct updateOptions {
	gchar		*username;	/**< username for HTTP auth (FIXME: not yet used) */
	gchar		*password;	/**< password for HTTP auth (FIXME: not yet used) */
	gboolean	dontUseProxy;	/**< no proxy flag */
} *updateOptionsPtr;

/** defines all state data an updatable object (e.g. a feed) needs */
typedef struct updateState {
	gchar		*lastModified;		/**< Last modified string as sent by the server */
	gchar		*etag;			/**< E-Tag sent by the server */
	GTimeVal	lastPoll;		/**< time at which the feed was last updated */
	GTimeVal	lastFaviconPoll;	/**< time at which the feeds favicon was last updated */
	gchar		*cookies;		/**< cookies to be used */	
} *updateStatePtr;

/** structure storing all information about a single update request */
typedef struct request {
	/* Set when requesting */
	gchar 		*source;	/**< Location of the source. If it starts with
					     '|', it is a command. If it contains "://",
					     then it is parsed as a URL, otherwise it is a
					     filename. Eventually, everything should be a
					     URL. Use file:// and exec:// */
	updateOptionsPtr options;	/**< Update options for the request */
	gchar		*filtercmd;	/**< Command will filter output of URL */
	request_cb	callback;	/**< Function to be called after retreival */
	gpointer	user_data;	/**< Accessed by the callback. Usually contains the nodePtr the download result is for (to be accessed by the callback). */
	guint32		flags;		/**< Flags to be passed to the callback */
	gint		priority;	/**< priority of the request. Set to 1 for high priority */
	gboolean	allowRetries;	/**< Allow download retries on network errors */
	GTimeVal	timestamp;	/**< Timestamp of request start time */
	
	/* Set by download system*/
	gpointer	owner;		/**< Pointer to anything used for lookup when cancelling requests */
	int		returncode;	/**< Download status (0=success, otherwise error) */
	int		httpstatus;	/**< HTTP status. Set to 200 for any valid command, file access, etc.... Set to 0 for unknown */
	gint		state;		/**< State of the request (enum request_state) */
	updateStatePtr	updateState;	/**< Update state of the requested object (etags, last modified...) */
	gchar		*data;		/**< Downloaded data */
	size_t		size;		/**< Size of downloaded data */
	gchar		*contentType;	/**< Content type of received data */
	gushort		retriesCount;	/**< Count how many retries have been done */	 
	gchar		*filterErrors;	/**< Error messages from filter execution */
} *requestPtr;

/**
 * Creates a new update state structure 
 *
 * @return a newly allocated state structure
 */
updateStatePtr update_state_new(void);

/* update state attribute encapsulation */
const gchar * update_state_get_lastmodified(updateStatePtr updateState);
void update_state_set_lastmodified(updateStatePtr updateState, const gchar *lastmodified);

const gchar * update_state_get_etag(updateStatePtr updateState);
void update_state_set_etag(updateStatePtr updateState, const gchar *etag);

/**
 * Frees the given update state.
 *
 * @param updateState	the update state
 */
void update_state_free(updateStatePtr updateState);

/**
 * Initialises the download subsystem, including its thread(s). 
 */
void update_init (void); 

/**
 * Stops all update processing and frees all used memory.
 */
void update_deinit (void);

/** 
 * Creates a new request structure.
 * 
 * @param owner		some pointer identifying the owner
 *			of the request (used to cancel requests)
 *
 * @returns a new request
 */
gpointer update_request_new(gpointer owner);

/**
 * Used to free a request structure. Frees all members, including data.
 *
 * @param request	pointer to a request structure
 */
void update_request_free(struct request *request);

/**
 * Sets the online status according to mode.
 *
 * @param mode	TRUE for online, FALSE for offline
 */ 
void update_set_online(gboolean mode);

/**
 * Queries the online status.
 *
 * @return TRUE if online
 */
gboolean update_is_online(void);

/**
 * Executes the given request. The request might be
 * delayed if other requests are pending. 
 *
 * @param request	the request to execute
 */
void update_execute_request(struct request *request);

/**
 * Executes the given request. Will block.
 *
 * @param request	the request to execute
 */
void update_execute_request_sync(struct request *request);

/**
 * Cancel all pending requests for the given owner.
 *
 * @param owner		pointer passed in update_request_new()
 */
void update_cancel_requests(gpointer owner);

/**
 * Cancel a request if it is waiting to be retried.
 * In case of success, the request is freed when its retry timeout expires,
 * so it is safe to just forget about it.
 * 
 * @param request	pointer to a request structure
 * @return TRUE if successfully cancelled the request
 */
gboolean update_request_cancel_retry(struct request *request);

#ifdef USE_NM
/**
 * Initialize NetworkManager support
 * Set up NM support, so that offline state can be set based on availability of
 * a network
 * 
 * @return TRUE if successfully initialized NM support
 */
gboolean update_nm_initialize(void);

/**
 * Clean up NetworkManager support
 */
void update_nm_cleanup(void);
#endif // USE_NM

#endif
