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
#include <libnm_glib.h>
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

/** global update job list, used for lookups when cancelling */
static GSList *jobs = NULL;

/** list of update threads */
static GSList *threads = NULL;

/* communication queues for requesting updates and sending the results */
static GAsyncQueue	*pendingHighPrioJobs = NULL;
static GAsyncQueue	*pendingJobs = NULL;
static GAsyncQueue	*finishedJobs = NULL;

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

static gboolean update_dequeue_finished_jobs (gpointer user_data);

/* update state interface */

updateStatePtr
update_state_new (void)
{
	return g_new0 (struct updateState, 1);
}

const gchar *
update_state_get_lastmodified (updateStatePtr state)
{
	return state->lastModified;
}

void
update_state_set_lastmodified (updateStatePtr state, const gchar *lastModified)
{
	g_free (state->lastModified);
	state->lastModified = NULL;
	if (lastModified)
		state->lastModified = g_strdup (lastModified);
}

const gchar *
update_state_get_etag (updateStatePtr state)
{
	return state->etag;
}

void
update_state_set_etag (updateStatePtr state, const gchar *etag)
{
	g_free (state->etag);
	state->etag = NULL;
	if (etag)
		state->etag = g_strdup (etag);
}

const gchar *
update_state_get_cookies (updateStatePtr state)
{
	return state->cookies;
}

void
update_state_set_cookies (updateStatePtr state, const gchar *cookies)
{
	g_free (state->cookies);
	state->cookies = NULL;
	if (cookies)
		state->cookies = g_strdup (cookies);
}

updateStatePtr
update_state_copy (updateStatePtr state)
{
	updateStatePtr newState;
	
	newState = update_state_new ();
	update_state_set_etag (newState, update_state_get_etag (state));
	update_state_set_lastmodified (newState, update_state_get_lastmodified (state));
	update_state_set_cookies (newState, update_state_get_cookies (state));
	
	return newState;
}

void
update_state_free (updateStatePtr state)
{
	if (!state)
		return;

	g_free (state->etag);
	g_free (state->lastModified);
	g_free (state->cookies);
	g_free (state);
}

updateOptionsPtr
update_options_copy (updateOptionsPtr options)
{
	updateOptionsPtr newOptions;
	
	newOptions = g_new0 (struct updateOptions, 1);
	newOptions->username = g_strdup (options->username);
	newOptions->password = g_strdup (options->password);
	newOptions->dontUseProxy = options->dontUseProxy;
	
	return newOptions;
}

void
update_options_free (updateOptionsPtr options)
{
	if (!options)
		return;
		
	g_free (options->username);
	g_free (options->password);
	g_free (options);
}

/* update request processing */

updateRequestPtr
update_request_new (void)
{
	return g_new0 (struct updateRequest, 1);
}

static void
update_request_free (updateRequestPtr request)
{
	if (!request)
		return;
	
	update_options_free (request->options);
	g_free (request->source);
	g_free (request->filtercmd);
	g_free (request);
}

updateResultPtr
update_result_new (void)
{
	updateResultPtr	result;
	
	result = g_new0 (struct updateResult, 1);
	result->updateState = update_state_new ();
	
	return result;
}

void
update_result_free (updateResultPtr result)
{
	if (!result)
		return;
		
	update_state_free (result->updateState);

	g_free (result->data);
	g_free (result->contentType);
	g_free (result->filterErrors);
	g_free (result);
}

/* update job handling */

typedef struct updateJob {
	updateRequestPtr	request;
	updateResultPtr		result;
	gpointer		owner;		/**< owner of this job (used for matching when cancelling) */
	update_result_cb	callback;	/**< result processing callback */
	gpointer		user_data;	/**< result processing user data */
	updateFlags		flags;		/**< request and result processing flags */
	gint			state;		/**< State of the job (enum request_state) */	
	gushort			retriesCount;	/**< Count how many retries have been done */	
} *updateJobPtr;

static updateJobPtr
update_job_new (gpointer owner,
                updateRequestPtr request,
		update_result_cb callback,
		gpointer user_data,
		updateFlags flags)
{
	updateJobPtr	job;
	
	job = g_new0 (struct updateJob, 1);
	job->owner = owner;
	job->request = request;
	job->callback = callback;
	job->user_data = user_data;
	job->flags = flags;	
	job->state = REQUEST_STATE_INITIALIZED;
	
	return job;
}

gint
update_job_get_state (updateJobPtr job)
{
	return job->state;
}

static void
update_job_free (updateJobPtr job)
{
	if (!job)
		return;
		
	jobs = g_slist_remove (jobs, job);
	
	update_request_free (job->request);
	update_result_free (job->result);
	g_free (job);
}

/* filter idea (and some of the code) was taken from Snownews */
static gchar *
update_exec_filter_cmd (gchar *cmd, gchar *data, gchar **errorOutput, size_t *size)
{
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
	g_free (tmpfilename);
	return out;
}

static gchar *
update_apply_xslt (updateJobPtr job)
{
	xsltStylesheetPtr	xslt = NULL;
	xmlOutputBufferPtr	buf;
	xmlDocPtr		srcDoc = NULL, resDoc = NULL;
	gchar			*output = NULL;

	g_assert (NULL != job->result);
	
	do {
		srcDoc = xml_parse (job->result->data, job->result->size, FALSE, NULL);
		if (!srcDoc) {
			g_warning("fatal: parsing request result XML source failed (%s)!", job->request->filtercmd);
			break;
		}

		/* load localization stylesheet */
		xslt = xsltParseStylesheetFile (job->request->filtercmd);
		if (!xslt) {
			g_warning ("fatal: could not load filter stylesheet \"%s\"!", job->request->filtercmd);
			break;
		}

		resDoc = xsltApplyStylesheet (xslt, srcDoc, NULL);
		if (!resDoc) {
			g_warning ("fatal: applying stylesheet \"%s\" failed!", job->request->filtercmd);
			break;
		}

		buf = xmlAllocOutputBuffer (NULL);
		if (-1 == xsltSaveResultTo (buf, resDoc, xslt)) {
			g_warning ("fatal: retrieving result of filter stylesheet failed (%s)!", job->request->filtercmd);
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

static void
update_apply_filter (updateJobPtr job)
{
	gchar	*filterResult;
	size_t	len;

	g_assert (NULL == job->result->filterErrors);

	/* we allow two types of filters: XSLT stylesheets and arbitrary commands */
	if ((strlen (job->request->filtercmd) > 4) &&
	    (0 == strcmp (".xsl", job->request->filtercmd + strlen (job->request->filtercmd) - 4))) {
		filterResult = update_apply_xslt (job);
		len = strlen (filterResult);
	} else {
		filterResult = update_exec_filter_cmd (job->request->filtercmd, job->result->data, &(job->result->filterErrors), &len);
	}

	if (filterResult) {
		g_free (job->result->data);
		job->result->data = filterResult;
		job->result->size = len;
	}
}

static void
update_exec_cmd (updateJobPtr job)
{
	FILE	*f;
	int	status;
	size_t	len;
	
	job->result = update_result_new ();
		
	/* if the first char is a | we have a pipe else a file */
	debug1 (DEBUG_UPDATE, "executing command \"%s\"...", (job->request->source) + 1);	
	f = popen ((job->request->source) + 1, "r");
	if (f) {
		while (!feof (f) && !ferror (f)) {
			job->result->data = g_realloc (job->result->data, job->result->size + 1025);
			len = fread (&job->result->data[job->result->size], 1, 1024, f);
			if (len > 0)
				job->result->size += len;
		}
		status = pclose (f);
		if (WIFEXITED (status) && WEXITSTATUS (status) == 0)
			job->result->httpstatus = 200;
		else 
			job->result->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */

		if (job->result->data)
			job->result->data[job->result->size] = '\0';
	} else {
		ui_mainwindow_set_status_bar (_("Error: Could not open pipe \"%s\""), (job->request->source) + 1);
		job->result->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */
	}
}

static void
update_load_file (updateJobPtr job)
{
	gchar *filename = job->request->source;
	gchar *anchor;
	
	job->result = update_result_new ();
	
	if (!strncmp (filename, "file://",7))
		filename += 7;

	anchor = strchr (filename, '#');
	if (anchor)
		*anchor = 0;	 /* strip anchors from filenames */

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		/* we have a file... */
		if ((!g_file_get_contents (filename, &(job->result->data), &(job->result->size), NULL)) || (job->result->data[0] == '\0')) {
			job->result->httpstatus = 403;	/* FIXME: maybe setting request->returncode would be better */
			ui_mainwindow_set_status_bar (_("Error: Could not open file \"%s\""), filename);
		} else {
			job->result->httpstatus = 200;
		}
	} else {
		ui_mainwindow_set_status_bar (_("Error: There is no file \"%s\""), filename);
		job->result->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */
	}
}

static void
update_job_run (updateJobPtr job) 
{
	if (*(job->request->source) == '|') {
		update_exec_cmd (job);
		
	} else if (strstr (job->request->source, "://") && 
	           strncmp (job->request->source, "file://", 7)) {
		job->result = network_process_request (job->request);
		if (job->result->httpstatus >= 400) {
			g_free (job->result->data);
			job->result->data = NULL;
			job->result->size = 0;
		}
	} else {
		update_load_file (job);		
	}

	/* Finally execute the postfilter */
	if (job->result->data && job->request->filtercmd) 
		update_apply_filter (job);
		
}

updateResultPtr
update_execute_request_sync (gpointer owner, 
                             updateRequestPtr request, 
			     guint flags)
{
	updateResultPtr	result;
	updateJobPtr	job;
	
	job = update_job_new (owner, request, NULL, NULL, flags);
	update_job_run (job);
	result = job->result;
	job->result = NULL;
	update_job_free (job);
			
	return result;
}

static void *
update_dequeue_jobs (void *data)
{
	updateJobPtr	job;
	gboolean	high_priority = (gboolean)GPOINTER_TO_INT (data);

	for (;;) {
		/* block updating if we are offline */
		if (!online) {
			debug0 (DEBUG_UPDATE, "now going offline!");
			g_mutex_lock (cond_mutex);
			g_cond_wait (offline_cond, cond_mutex);
	                g_mutex_unlock (cond_mutex);
			debug0 (DEBUG_UPDATE, "going online again!");
		}
		
		/* do update processing */
		if (DEBUG_VERBOSE & debug_level)
			debug0 (DEBUG_UPDATE, "waiting for request...");
		if (high_priority) {
			job = g_async_queue_pop (pendingHighPrioJobs);
		} else {
			do {
				job = g_async_queue_try_pop (pendingHighPrioJobs);
				if (!job) {
					GTimeVal wait;
					g_get_current_time (&wait);
					g_time_val_add (&wait, 500000);
					job = g_async_queue_timed_pop (pendingJobs, &wait);
				}
			} while (!job);
		}
		g_assert (NULL != job);
		job->state = REQUEST_STATE_PROCESSING;

		debug1 (DEBUG_UPDATE, "processing request (%s)", job->request->source);
		if (job->callback == NULL) {
			debug1 (DEBUG_UPDATE, "freeing cancelled request (%s)", job->request->source);
			update_job_free (job);
		} else {
			update_job_run (job);
			
			/* return the request so the GUI thread can merge the feeds and display the results... */
			debug1 (DEBUG_UPDATE, "request (%s) finished", job->request->source);
			g_async_queue_push (finishedJobs, (gpointer)job);
			if (!results_timer) 
				results_timer = g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 100,
				                                    update_dequeue_finished_jobs, 
								    NULL, NULL);

		}
	}
}

updateJobPtr
update_execute_request (gpointer owner, 
                        updateRequestPtr request, 
			update_result_cb callback, 
			gpointer user_data, 
			updateFlags flags)
{
	updateJobPtr job;
	
	g_assert (request->options != NULL);
	
	job = update_job_new (owner, request, callback, user_data, flags);
	job->state = REQUEST_STATE_PENDING;
	
	jobs = g_slist_append (jobs, job);
	
	if (flags & FEED_REQ_PRIORITY_HIGH)
		g_async_queue_push (pendingHighPrioJobs, job);
	else
		g_async_queue_push (pendingJobs, job);
		
	return job;
}

void
update_job_cancel_by_owner (gpointer owner)
{
	GSList	*iter = jobs;

	while (iter) {
		updateJobPtr job = (updateJobPtr)iter->data;
		if (job->owner == owner)
			job->callback = NULL;
		iter = g_slist_next (iter);
	}
}

void
update_set_online (gboolean mode)
{
	if (online != mode) {
		online = mode;
		if (online) {
			g_mutex_lock (cond_mutex);
			g_cond_signal (offline_cond);
			g_mutex_unlock (cond_mutex);
		}
		debug1 (DEBUG_UPDATE, "Changing online mode to %s", mode?"online":"offline");
		ui_mainwindow_online_status_changed (mode);
		liferea_htmlview_set_online (mode);
		ui_tray_update ();
	}
}

gboolean
update_is_online (void)
{
	return online;
}

/* Wrapper for reenqueuing requests in case of retries, for convenient call from g_timeout */
static gboolean
update_requeue_job (gpointer data)
{
	updateJobPtr job = (updateJobPtr)data;
	
	if (job->callback == NULL) {
		debug2(DEBUG_UPDATE, "Freeing request of cancelled retry #%d for \"%s\"", job->retriesCount, job->request->source);
		update_job_free (job);
	} else {
		g_async_queue_push (pendingJobs, job);
	}
	return FALSE;
}

/* Schedules a retry for the given request */
static void
update_job_retry (updateJobPtr job)
{
	guint retryDelay;
	gushort i;	

	if (job->result) {
		update_result_free (job->result);
		job->result = NULL;
	}
		
	/* Note: in case of permanent HTTP redirection leading to a network
	 * error, retries will be done on the redirected request->source. */

	/* Prepare for a retry: increase counter and calculate delay */
	retryDelay = REQ_MIN_DELAY_FOR_RETRY;
	for (i = 0; i < job->retriesCount; i++)
		retryDelay *= 3;
	if (retryDelay > REQ_MAX_DELAY_FOR_RETRY)
		retryDelay = REQ_MAX_DELAY_FOR_RETRY;

	/* Requeue the request after the waiting delay */
	job->retriesCount++;	
	g_timeout_add_full (G_PRIORITY_DEFAULT_IDLE, 1000 * retryDelay, update_requeue_job, job, NULL);
	ui_mainwindow_set_status_bar (_("Could not download \"%s\". Will retry in %d seconds."), job->request->source, retryDelay);
}

gboolean
update_job_cancel_retry (updateJobPtr job)
{
	if (0 < job->retriesCount) {
		job->callback = NULL;
		debug2 (DEBUG_UPDATE, "cancelling retry #%d (%s)", job->retriesCount, job->request->source);
		return TRUE;
	}
	return FALSE;
}

static gboolean
update_process_result_idle_cb (gpointer user_data)
{
	updateJobPtr job = (updateJobPtr)user_data;
	
	if (job->callback)
		(job->callback) (job->result, job->user_data, job->flags);

	update_job_free (job);
		
	return FALSE;
}

static gboolean
update_dequeue_finished_jobs (gpointer user_data)
{
	updateJobPtr	job;

	results_timer = 0;
	
	while (NULL != (job = g_async_queue_try_pop (finishedJobs))) {
		job->state = REQUEST_STATE_DEQUEUE;

		/* Handling abandoned requests (e.g. after feed deletion) */
		if (job->callback == NULL) {	
			debug1 (DEBUG_UPDATE, "freeing cancelled request (%s)", job->request->source);
			update_job_free (job);
			continue;
		} 
		
		/* Retrying in some error cases */
		if ((job->result->returncode == NET_ERR_UNKNOWN) ||
		    (job->result->returncode == NET_ERR_CONN_FAILED) ||
		    (job->result->returncode == NET_ERR_SOCK_ERR) ||
		    (job->result->returncode == NET_ERR_HOST_NOT_FOUND) ||
		    (job->result->returncode == NET_ERR_TIMEOUT)) {

			if (job->request->allowRetries && (REQ_MAX_NUMBER_OF_RETRIES <= job->retriesCount)) {
				debug1 (DEBUG_UPDATE, "retrying download (%s)", job->request->source);
				update_job_retry (job);
				continue;
			}

			debug1 (DEBUG_UPDATE, "retry count exceeded (%s)", job->request->source);
		}
		
		/* Normal result processing */
		g_idle_add (update_process_result_idle_cb, job);
	}
	return FALSE;
}

void
update_init (void)
{
	gushort	i, count;

	network_init ();
	
	pendingHighPrioJobs = g_async_queue_new ();
	pendingJobs = g_async_queue_new ();
	finishedJobs = g_async_queue_new ();
	
	offline_cond = g_cond_new ();
	cond_mutex = g_mutex_new ();
		
	if (1 >= (count = conf_get_int_value (UPDATE_THREAD_CONCURRENCY)))
		count = DEFAULT_UPDATE_THREAD_CONCURRENCY;
	
	for (i = 0; i < count; i++) {
		GThread *thread = g_thread_create (update_dequeue_jobs, GINT_TO_POINTER((i == 0)), FALSE, NULL);
		threads = g_slist_append (threads, thread);
	}
}

#ifdef USE_NM
static void
update_network_monitor (libnm_glib_ctx *ctx, gpointer user_data)
{
	libnm_glib_state	state;
	gboolean online;

	g_return_if_fail (ctx != NULL);

	state = libnm_glib_get_network_state (ctx);
	online = update_is_online ();

	if (online && state == LIBNM_NO_NETWORK_CONNECTION) {
		debug0 (DEBUG_UPDATE, "network manager: no network connection -> going offline");
		update_set_online (FALSE);
	} else if (!online && state == LIBNM_ACTIVE_NETWORK_CONNECTION) {
		debug0 (DEBUG_UPDATE, "network manager: active connection -> going online");
		update_set_online (TRUE);
	}
}

gboolean
update_nm_initialize (void)
{
	debug0 (DEBUG_UPDATE, "network manager: registering network state change callback");
	
	if (!nm_ctx) {
		nm_ctx = libnm_glib_init ();
		if (!nm_ctx) {
			fprintf (stderr, "Could not initialize libnm.\n");
			return FALSE;
		}	
	}

	nm_id = libnm_glib_register_callback (nm_ctx, update_network_monitor, NULL, NULL);
	
	return TRUE;
}

void
update_nm_cleanup (void)
{
	debug0 (DEBUG_UPDATE, "network manager: unregistering network state change callback");
	
	if (nm_id != 0 && nm_ctx != NULL) {
		libnm_glib_unregister_callback (nm_ctx, nm_id);
		libnm_glib_shutdown (nm_ctx);
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
	
	//network_deinit ();
	
	/* FIXME: terminate update threads to be able to remove the queues
	
	g_async_queue_unref (pendingHighPrioJobs);
	g_async_queue_unref (pendingJobs);
	g_async_queue_unref (finishedJobs);
	
	
	g_free (offline_cond);
	g_free (cond_mutex);
	
	g_slist_free (jobs);
	jobs = NULL;
	*/
	
	debug_exit ("update_deinit");
}
