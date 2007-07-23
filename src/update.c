/**
 * @file update.c  generic update request processing
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <libxml/parser.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>

#ifdef USE_NM
#include <NetworkManager/libnm_glib.h>
#endif

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "net.h"
#include "update.h"
#include "xml.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_tray.h"

/* must never be smaller than 2, because first thread works exclusivly on high prio requests */
#define DEFAULT_UPDATE_THREAD_CONCURRENCY	6

/** global request list, used for lookups when cancelling */
static GSList *requests = NULL;

/** list of update threads */
static GSList *threads = NULL;

/* communication queues for requesting updates and sending the results */
static GAsyncQueue	*requests_high_prio = NULL;
static GAsyncQueue	*requests_normal_prio = NULL;
static GAsyncQueue	*results = NULL;

static guint		results_timer = 0;

/* condition mutex for offline mode */
static GMutex	*cond_mutex = NULL;
static GCond	*offline_cond = NULL;
static gboolean	online = TRUE;

#ifdef USE_NM
/* State for NM support */
static libnm_glib_ctx *nm_ctx = NULL;
static guint nm_id = 0;
#endif

static gboolean update_dequeue_results(gpointer user_data);

/* update state interface */

updateStatePtr update_state_new(void) {

	return g_new0(struct updateState, 1);
}

const gchar * update_state_get_lastmodified(updateStatePtr updateState) { return updateState->lastModified; };
void update_state_set_lastmodified(updateStatePtr updateState, const gchar *lastModified) {

	g_free(updateState->lastModified);
	updateState->lastModified = NULL;
	if(lastModified)
		updateState->lastModified = g_strdup(lastModified);
}

const gchar * update_state_get_etag(updateStatePtr updateState) { return updateState->etag; };
void update_state_set_etag(updateStatePtr updateState, const gchar *etag) {

	g_free(updateState->etag);
	updateState->etag = NULL;
	if(etag)
		updateState->etag = g_strdup(etag);
}

void update_state_free(updateStatePtr updateState) {

	g_free(updateState->etag);
	g_free(updateState->lastModified);
	g_free(updateState->cookies);
	g_free(updateState);
}

/* update request processing */

/* filter idea (and some of the code) was taken from Snownews */
static char* update_exec_filter_cmd(gchar *cmd, gchar *data, gchar **errorOutput, size_t *size) {
	int		fd, status;
	gchar		*command;
	const gchar	*tmpdir = g_get_tmp_dir();
	char		*tmpfilename;
	char		*out = NULL;
	FILE		*file, *p;
	
	*errorOutput = NULL;
	tmpfilename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "liferea-XXXXXX", tmpdir);
	
	fd = g_mkstemp(tmpfilename);
	
	if(fd == -1) {
		debug1(DEBUG_UPDATE, "Error opening temp file %s to use for filtering!", tmpfilename);
		*errorOutput = g_strdup_printf(_("Error opening temp file %s to use for filtering!"), tmpfilename);
		g_free(tmpfilename);
		return NULL;
	}	
		
	file = fdopen(fd, "w");
	fwrite(data, strlen(data), 1, file);
	fclose(file);

	*size = 0;
	command = g_strdup_printf("%s < %s", cmd, tmpfilename);
	p = popen(command, "r");
	g_free(command);
	if(NULL != p) {
		while(!feof(p) && !ferror(p)) {
			size_t len;
			out = g_realloc(out, *size+1025);
			len = fread(&out[*size], 1, 1024, p);
			if(len > 0)
				*size += len;
		}
		status = pclose(p);
		if(!(WIFEXITED(status) && WEXITSTATUS(status) == 0)) {
			*errorOutput = g_strdup_printf(_("%s exited with status %d"),
			                              cmd, WEXITSTATUS(status));
			*size = 0;
		}
		out[*size] = '\0';
	} else {
		g_warning(_("Error: Could not open pipe \"%s\""), command);
		*errorOutput = g_strdup_printf(_("Error: Could not open pipe \"%s\""), command);
	}
	/* Clean up. */
	unlink (tmpfilename);
	g_free(tmpfilename);
	return out;
}

static gchar *
update_apply_xslt (requestPtr request)
{
	xsltStylesheetPtr	xslt = NULL;
	xmlOutputBufferPtr	buf;
	xmlDocPtr		srcDoc = NULL, resDoc = NULL;
	gchar			*output = NULL;

	do {
		srcDoc = xml_parse (request->data, request->size, FALSE, NULL);
		if (!srcDoc) {
			g_warning("fatal: parsing request result XML source failed (%s)!", request->filtercmd);
			break;
		}

		/* load localization stylesheet */
		xslt = xsltParseStylesheetFile (request->filtercmd);
		if (!xslt) {
			g_warning ("fatal: could not load filter stylesheet \"%s\"!", request->filtercmd);
			break;
		}

		resDoc = xsltApplyStylesheet (xslt, srcDoc, NULL);
		if (!resDoc) {
			g_warning ("fatal: applying stylesheet \"%s\" failed!", request->filtercmd);
			break;
		}

		buf = xmlAllocOutputBuffer (NULL);
		if (-1 == xsltSaveResultTo (buf, resDoc, xslt)) {
			g_warning ("fatal: retrieving result of filter stylesheet failed (%s)!", request->filtercmd);
			break;
		}
		
		if (xmlBufferLength (buf->buffer) > 0)
			output = xmlCharStrdup (xmlBufferContent (buf->buffer));
 
		xmlOutputBufferClose (buf);
	} while (FALSE);

	if (srcDoc)
		xmlFreeDoc (srcDoc);
	if (resDoc)
		xmlFreeDoc (resDoc);
	if (xslt)
		xsltFreeStylesheet (xslt);
	
	return output;
}

static void update_apply_filter(requestPtr request) {
	gchar	*filterResult;
	size_t	len;

	g_free(request->filterErrors);
	request->filterErrors = NULL;

	/* we allow two types of filters: XSLT stylesheets and arbitrary commands */
	if((strlen(request->filtercmd) > 4) &&
	   (0 == strcmp(".xsl", request->filtercmd + strlen(request->filtercmd) - 4))) {
		filterResult = update_apply_xslt(request);
		len = strlen(filterResult);
	} else {
		filterResult = update_exec_filter_cmd(request->filtercmd, request->data, &(request->filterErrors), &len);
	}

	if(filterResult) {
		g_free(request->data);
		request->data = filterResult;
		request->size = len;
	}
}

static void update_exec_cmd(requestPtr request) {
	FILE	*f;
	int	status;
	size_t	len;
		
	/* if the first char is a | we have a pipe else a file */
	debug1(DEBUG_UPDATE, "executing command \"%s\"...", (request->source) + 1);	
	f = popen((request->source) + 1, "r");
	if(f) {
		while(!feof(f) && !ferror(f)) {
			request->data = g_realloc(request->data, request->size + 1025);
			len = fread(&request->data[request->size], 1, 1024, f);
			if(len > 0)
				request->size += len;
		}
		status = pclose(f);
		if(WIFEXITED(status) && WEXITSTATUS(status) == 0)
			request->httpstatus = 200;
		else 
			request->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */

		if(request->data)
			request->data[request->size] = '\0';
	} else {
		ui_mainwindow_set_status_bar(_("Error: Could not open pipe \"%s\""), (request->source) + 1);
		request->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */
	}
}

static void update_load_file(requestPtr request) {
	gchar *filename = request->source;
	gchar *anchor;

	if(!strncmp(filename, "file://",7))
		filename += 7;

	anchor = strchr(filename, '#');
	if(anchor)
		*anchor = 0;	 /* strip anchors from filenames */

	if(g_file_test(filename, G_FILE_TEST_EXISTS)) {
		/* we have a file... */
		if((!g_file_get_contents(filename, &(request->data), &(request->size), NULL)) || (request->data[0] == '\0')) {
			request->httpstatus = 403;	/* FIXME: maybe setting request->returncode would be better */
			ui_mainwindow_set_status_bar(_("Error: Could not open file \"%s\""), filename);
		} else {
			g_assert(NULL != request->data);
			request->httpstatus = 200;
		}
	} else {
		ui_mainwindow_set_status_bar(_("Error: There is no file \"%s\""), filename);
		request->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */
	}
}

void update_execute_request_sync(requestPtr request) {

	g_assert(request->data == NULL);
	g_assert(request->size == 0);
	
	if(*(request->source) == '|') {
		update_exec_cmd(request);
		
	} else if(strstr(request->source, "://") && strncmp(request->source, "file://",7)) {
		network_process_request (request);
		if(request->httpstatus >= 400) {
			g_free(request->data);
			request->data = NULL;
			request->size = 0;
		}
	} else {
		update_load_file(request);		
	}

	/* Finally execute the postfilter */
	if(request->data && request->filtercmd) 
		update_apply_filter(request);
}

gpointer update_request_new(gpointer owner) {
	requestPtr	request;

	request = g_new0(struct request, 1);
	request->owner = owner;
	request->state = REQUEST_STATE_INITIALIZED;
	g_get_current_time (&request->timestamp);
	
	return (gpointer)request;
}

void update_request_free(requestPtr request) {

	requests = g_slist_remove(requests, request);
	
	g_free(request->source);
	g_free(request->filtercmd);
	g_free(request->filterErrors);
	g_free(request->data);
	g_free(request->contentType);
	g_free(request);
}

static void *update_dequeue_requests(void *data) {
	requestPtr	request;
	gboolean	high_priority = (gboolean)GPOINTER_TO_INT(data);

	for(;;)	{
		/* block updating if we are offline */
		if(!online) {
			debug0(DEBUG_UPDATE, "now going offline!");
			g_mutex_lock(cond_mutex);
			g_cond_wait(offline_cond, cond_mutex);
	                g_mutex_unlock(cond_mutex);
			debug0(DEBUG_UPDATE, "going online again!");
		}
		
		/* do update processing */
		if(DEBUG_VERBOSE & debug_level)
			debug0(DEBUG_UPDATE, "waiting for request...");
		if(high_priority) {
			request = g_async_queue_pop(requests_high_prio);
		} else {
			do {
				request = g_async_queue_try_pop(requests_high_prio);
				if(!request) {
					GTimeVal wait;
					g_get_current_time(&wait);
					g_time_val_add(&wait, 500000);
					request = g_async_queue_timed_pop(requests_normal_prio, &wait);
				}
			} while(!request);
		}
		g_assert(NULL != request);
		request->state = REQUEST_STATE_PROCESSING;

		debug1(DEBUG_UPDATE, "processing request (%s)", request->source);
		if(request->callback == NULL) {
			debug1(DEBUG_UPDATE, "freeing cancelled request (%s)", request->source);
			update_request_free(request);
		} else {
			update_execute_request_sync(request);
			
			/* return the request so the GUI thread can merge the feeds and display the results... */
			debug1(DEBUG_UPDATE, "request (%s) finished", request->source);
			g_async_queue_push(results, (gpointer)request);
			if (!results_timer) 
				results_timer = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
		                   100, 
				   update_dequeue_results, 
				   NULL,
				   NULL);

		}
	}
}

void update_execute_request(requestPtr new_request) {

	g_assert(new_request->data == NULL);
	g_assert(new_request->size == 0);
	g_assert(new_request->callback != NULL);
	g_assert(new_request->options != NULL);
	
	new_request->state = REQUEST_STATE_PENDING;
	
	requests = g_slist_append(requests, new_request);
	
	if(1 == new_request->priority)
		g_async_queue_push(requests_high_prio, new_request);
	else
		g_async_queue_push(requests_normal_prio, new_request);
}

void update_cancel_requests(gpointer owner) {
	GSList	*iter = requests;

	while(iter) {
		requestPtr request = (requestPtr)iter->data;
		if(request->owner == owner)
			request->callback = NULL;
		iter = g_slist_next(iter);
	}
}

void update_set_online(gboolean mode) {

	if(online != mode) {
		online = mode;
		if(online) {
			g_mutex_lock(cond_mutex);
			g_cond_signal(offline_cond);
			g_mutex_unlock(cond_mutex);
		}
		debug1(DEBUG_UPDATE, "Changing online mode to %s", mode?"online":"offline");
		ui_mainwindow_online_status_changed(mode);
		ui_htmlview_online_status_changed(mode);
		ui_tray_update();
	}
}

gboolean update_is_online(void) {

	return online;
}

/* Wrapper for reenqueuing requests in case of retries, for convenient call from g_timeout */
static gboolean update_requeue_request(gpointer data) {
	requestPtr request = (requestPtr)data;
	
	if(request->callback == NULL) {
		debug2(DEBUG_UPDATE, "Freeing request of cancelled retry #%d for \"%s\"", request->retriesCount, request->source);
		update_request_free(request);
	} else {
		update_execute_request(request);
	}
	return FALSE;
}

/* Schedules a retry for the given request */
static void update_request_retry(requestPtr request) {
	guint retryDelay;
	gushort i;	

	/* Normally there should have been no received data, 
	   hence nothing to free. But depending on the errors
	   checked above this is not always the case... */
	if(request->data) {
		g_free(request->data);
		request->data = NULL;
	}
	if(request->contentType) {
		g_free(request->contentType);
		request->contentType = NULL;
	}

	/* Note: in case of permanent HTTP redirection leading to a network
	 * error, retries will be done on the redirected request->source. */

	/* Prepare for a retry: increase counter and calculate delay */
	retryDelay = REQ_MIN_DELAY_FOR_RETRY;
	for(i = 0; i < request->retriesCount; i++)
		retryDelay *= 3;
	if(retryDelay > REQ_MAX_DELAY_FOR_RETRY)
		retryDelay = REQ_MAX_DELAY_FOR_RETRY;

	/* Requeue the request after the waiting delay */
	g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 1000 * retryDelay, update_requeue_request, request, NULL);
	request->retriesCount++;
	ui_mainwindow_set_status_bar(_("Could not download \"%s\". Will retry in %d seconds."), request->source, retryDelay);
}

gboolean update_request_cancel_retry(requestPtr request) {

	if(0 < request->retriesCount) {
		request->callback = NULL;
		debug2(DEBUG_UPDATE, "cancelling retry #%d (%s)", request->retriesCount, request->source);
		return TRUE;
	}
	return FALSE;
}

static gboolean update_dequeue_results(gpointer user_data) {
	requestPtr	request;
	request_cb	callback;

	results_timer = 0;
	
	while(NULL != (request = g_async_queue_try_pop(results))) {
		callback = request->callback;
		request->state = REQUEST_STATE_DEQUEUE;

		/* Handling abandoned requests (e.g. after feed deletion) */
		if(callback == NULL) {	
			debug1(DEBUG_UPDATE, "freeing cancelled request (%s)", request->source);
			update_request_free(request);
			continue;
		} 
		
		/* Retrying in some error cases */
		if((request->returncode == NET_ERR_UNKNOWN) ||
		   (request->returncode == NET_ERR_CONN_FAILED) ||
		   (request->returncode == NET_ERR_SOCK_ERR) ||
		   (request->returncode == NET_ERR_HOST_NOT_FOUND) ||
		   (request->returncode == NET_ERR_TIMEOUT)) {

			if(request->allowRetries && (REQ_MAX_NUMBER_OF_RETRIES <= request->retriesCount)) {
				debug1(DEBUG_UPDATE, "retrying download (%s)", request->source);
				update_request_retry(request);
			} else {
				debug1(DEBUG_UPDATE, "retry count exceeded (%s)", request->source);
				(callback)(request);
			}
			continue;
		}
		
		/* Normal result processing */
		(callback)(request);
	}
	return FALSE;
}

void update_init(void) {
	gushort	i, count;

	network_init ();
	
	requests_high_prio = g_async_queue_new();
	requests_normal_prio = g_async_queue_new();
	results = g_async_queue_new();
	
	offline_cond = g_cond_new();
	cond_mutex = g_mutex_new();
		
	if(1 >= (count = getNumericConfValue(UPDATE_THREAD_CONCURRENCY)))
		count = DEFAULT_UPDATE_THREAD_CONCURRENCY;
	
	for (i = 0; i < count; i++) {
		GThread *thread = g_thread_create (update_dequeue_requests, GINT_TO_POINTER((i == 0)), FALSE, NULL);
		threads = g_slist_append (threads, thread);
	}
}

#ifdef USE_NM
static void update_network_monitor(libnm_glib_ctx *ctx, gpointer user_data)
{
	libnm_glib_state	state;
	gboolean online;

	g_return_if_fail(ctx != NULL);

	state = libnm_glib_get_network_state(ctx);
	online = update_is_online();

	if(online && state == LIBNM_NO_NETWORK_CONNECTION) {
		debug0(DEBUG_UPDATE, "network manager: no network connection -> going offline");
		update_set_online(FALSE);
	} else if(!online && state == LIBNM_ACTIVE_NETWORK_CONNECTION) {
		debug0(DEBUG_UPDATE, "network manager: active connection -> going online");
		update_set_online(TRUE);
	}
}


gboolean update_nm_initialize(void)
{

	debug0(DEBUG_UPDATE, "network manager: registering network state change callback");
	
	if (!nm_ctx)
	{
		nm_ctx = libnm_glib_init();
		if (!nm_ctx) {
				fprintf(stderr, "Could not initialize libnm.\n");
				return FALSE;
			  }	
	}

	nm_id = libnm_glib_register_callback(nm_ctx, update_network_monitor, NULL, NULL);
	
	return TRUE;
}

void update_nm_cleanup(void)
{
	debug0(DEBUG_UPDATE, "network manager: unregistering network state change callback");
	
	if (nm_id != 0 && nm_ctx != NULL) {
		libnm_glib_unregister_callback(nm_ctx, nm_id);
		libnm_glib_shutdown(nm_ctx);
		nm_ctx = NULL;
		nm_id = 0;
	}
}
#endif

void
update_deinit (void)
{
	debug_enter ("update_deinit");
	
	//update_nm_cleanup ();
	
	network_deinit ();
	
	/* FIXME: terminate update threads to be able to remove the queues
	
	g_async_queue_unref (requests_high_prio);
	g_async_queue_unref (requests_normal_prio);
	g_async_queue_unref (results);
	*/
	
	g_free (offline_cond);
	g_free (cond_mutex);
	
	g_slist_free (requests);
	requests = NULL;
	
	debug_exit ("update_deinit");
}
