/**
 * @file update.c feed update request processing
 *
 * Copyright (C) 2003-2006 Lars Lindner <lars.lindner@gmx.net>
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

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "support.h"
#include "update.h"
#include "ui/ui_htmlview.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_tray.h"
#include "net/downloadlib.h"

/* must never be smaller than 2, because first thread works exclusivly on high prio requests */
#define DEFAULT_UPDATE_THREAD_CONCURRENCY	4

/* communication queues for requesting updates and sending the results */
GAsyncQueue	*requests_high_prio = NULL;
GAsyncQueue	*requests_normal_prio = NULL;
GAsyncQueue	*results = NULL;

/* condition mutex for offline mode */
static GMutex	*cond_mutex = NULL;
static GCond	*offline_cond = NULL;
static gboolean	online = TRUE;

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

void update_state_import(xmlNodePtr cur, updateStatePtr updateState) {
	xmlChar	*tmp;
	
	tmp = xmlGetProp(cur, BAD_CAST"etag");
	if(tmp) {
		update_state_set_etag(updateState, tmp);
		xmlFree(tmp);
	}
	
	tmp = xmlGetProp(cur, BAD_CAST"lastModified");
	if(tmp) {
		update_state_set_lastmodified(updateState, tmp);
		xmlFree(tmp);
	}
		
	/* Last poll time*/
	tmp = xmlGetProp(cur, BAD_CAST"lastPollTime");
	updateState->lastPoll.tv_sec = common_parse_long(tmp, 0L);
	updateState->lastPoll.tv_usec = 0L;
	if(tmp)
		xmlFree(tmp);

	tmp = xmlGetProp(cur, BAD_CAST"lastFaviconPollTime");
	updateState->lastFaviconPoll.tv_sec = common_parse_long(tmp, 0L);
	updateState->lastFaviconPoll.tv_usec = 0L;
	if(tmp)
		xmlFree(tmp);
}

void update_state_export(xmlNodePtr cur, updateStatePtr updateState) {

	if(update_state_get_etag(updateState)) 
		xmlNewProp(cur, BAD_CAST"etag", BAD_CAST update_state_get_etag(updateState));

	if(update_state_get_lastmodified(updateState)) 
		xmlNewProp(cur, BAD_CAST"lastModified", BAD_CAST update_state_get_lastmodified(updateState));
		
	if(updateState->lastPoll.tv_sec > 0) {
		gchar *lastPoll = g_strdup_printf("%ld", updateState->lastPoll.tv_sec);
		xmlNewProp(cur, BAD_CAST"lastPollTime", BAD_CAST lastPoll);
		g_free(lastPoll);
	}
	
	if(updateState->lastFaviconPoll.tv_sec > 0) {
		gchar *lastPoll = g_strdup_printf("%ld", updateState->lastFaviconPoll.tv_sec);
		xmlNewProp(cur, BAD_CAST"lastFaviconPollTime", BAD_CAST lastPoll);
		g_free(lastPoll);
	}
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
	
	if (fd == -1) {
		debug1(DEBUG_UPDATE, "Error opening temp file %s to use for filtering!", tmpfilename);
		*errorOutput = g_strdup_printf(_("Error opening temp file %s to use for filtering!"), tmpfilename);
		g_free(tmpfilename);
		return NULL;
	}	
		
	file = fdopen (fd, "w");
	fwrite (data, strlen(data), 1, file);
	fclose (file);

	*size = 0;
	command = g_strdup_printf("%s < %s", cmd, tmpfilename);
	p = popen(command, "r");
	g_free(command);
	if(NULL != p) {
		while(!feof(p) && !ferror(p)) {
			size_t len;
			out = g_realloc(out, *size+1025);
			len = fread(&out[*size], 1, 1024, p);
			if (len > 0)
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

static gchar * update_apply_xslt(struct request *request) {
	xsltStylesheetPtr	xslt = NULL;
	xmlOutputBufferPtr	buf;
	xmlDocPtr		srcDoc = NULL, resDoc = NULL;
	gchar			*output = NULL;

	do {
		if(NULL == (srcDoc = common_parse_xml(request->data, request->size, FALSE, NULL))) {
			g_warning("fatal: parsing request result XML source failed (%s)!", request->filtercmd);
			break;
		}

		/* load localization stylesheet */
		if(NULL == (xslt = xsltParseStylesheetFile(request->filtercmd))) {
			g_warning("fatal: could not load filter stylesheet \"%s\"!", request->filtercmd);
			break;
		}

		if(NULL == (resDoc = xsltApplyStylesheet(xslt, srcDoc, NULL))) {
			g_warning("fatal: applying stylesheet \"%s\" failed!", request->filtercmd);
			break;
		}

		buf = xmlAllocOutputBuffer(NULL);
		if(-1 == xsltSaveResultTo(buf, resDoc, xslt)) {
			g_warning("fatal: retrieving result of filter stylesheet failed (%s)!", request->filtercmd);
			break;
		}
		
		if(xmlBufferLength(buf->buffer) > 0)
			output = xmlCharStrdup(xmlBufferContent(buf->buffer));

		xmlOutputBufferClose(buf);
	} while(FALSE);

	if(srcDoc)
		xmlFreeDoc(srcDoc);
	if(resDoc)
		xmlFreeDoc(resDoc);
	if(xslt)
		xsltFreeStylesheet(xslt);
	
	return output;
}

static void update_apply_filter(struct request *request) {
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

static void update_exec_cmd(struct request *request) {
	FILE	*f;
	int	status;
	size_t	len;
		
	/* if the first char is a | we have a pipe else a file */
	debug1(DEBUG_UPDATE, "executing command \"%s\"...\n", (request->source) + 1);	
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

static void update_load_file(struct request *request) {
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

void update_execute_request_sync(struct request *request) {

	g_assert(request->data == NULL);
	g_assert(request->size == 0);
	
	if(*(request->source) == '|') {
		update_exec_cmd(request);
		
	} else if(strstr(request->source, "://") && strncmp(request->source, "file://",7)) {
		downloadlib_process_url(request);
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

gpointer update_request_new() {
	struct request	*request;

	request = g_new0(struct request, 1);
	request->state = REQUEST_STATE_INITIALIZED;
	
	return (gpointer)request;
}

void update_request_free(struct request *request) {
	
	debug_enter("update_request_free");

	g_free(request->source);
	g_free(request->filtercmd);
	g_free(request->filterErrors);
	g_free(request->data);
	g_free(request->contentType);
	g_free(request);

	debug_exit("update_request_free");
}

static void *update_dequeue_requests(void *data) {
	struct request	*request;
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
		debug0(DEBUG_VERBOSE, "waiting for request...");
		if(high_priority) {
			request = g_async_queue_pop(requests_high_prio);
		} else {
			do {
				request = g_async_queue_try_pop(requests_high_prio);
				if(!request) {
					GTimeVal wait;
					g_get_current_time(&wait);
					g_time_val_add(&wait, 5000);
					request = g_async_queue_timed_pop(requests_normal_prio, &wait);
				}
			} while(!request);
		}
		g_assert(NULL != request);
		request->state = REQUEST_STATE_PROCESSING;

		debug1(DEBUG_UPDATE, "processing received request (%s)", request->source);
		if(request->callback == NULL) {
			debug1(DEBUG_UPDATE, "freeing cancelled request (%s)", request->source);
			update_request_free(request);
		} else {
			update_execute_request_sync(request);
			
			/* return the request so the GUI thread can merge the feeds and display the results... */
			debug1(DEBUG_UPDATE, "request (%s) finished", request->source);
			g_async_queue_push(results, (gpointer)request);
		}
	}
}

void update_execute_request(struct request *new_request) {

	g_assert(new_request->data == NULL);
	g_assert(new_request->size == 0);
	
	new_request->state = REQUEST_STATE_PENDING;

	if(1 == new_request->priority)
		g_async_queue_push(requests_high_prio, new_request);
	else
		g_async_queue_push(requests_normal_prio, new_request);
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
	struct request* request = (struct request*)data;
	
	if(request->callback == NULL) {
		debug2(DEBUG_UPDATE, "Freeing request of cancelled retry #%d for \"%s\"", request->retriesCount, request->source);
		update_request_free(request);
	} else {
		update_execute_request(request);
	}
	return FALSE;
}

/* Schedules a retry for the given request */
static void update_request_retry(struct request *request) {
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

gboolean update_request_cancel_retry(struct request *request) {

	if(0 < request->retriesCount) {
		request->callback = NULL;
		debug2(DEBUG_UPDATE, "cancelling retry #%d (%s)", request->retriesCount, request->source);
		return TRUE;
	}
	return FALSE;
}

static gboolean update_dequeue_results(gpointer user_data) {
	struct request	*request;
	
	while(NULL != (request = g_async_queue_try_pop(results))) {
		request->state = REQUEST_STATE_FINISHED;

		/* Handling abandoned requests (e.g. after feed deletion) */
		if(request->callback == NULL) {	
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
				(request->callback)(request);
				update_request_free(request);
			}
			continue;
		}
		
		/* Normal result processing */
		(request->callback)(request);
		update_request_free(request);
	}
	return TRUE;
}

void update_init(void) {
	int	i;
	int	count;

	downloadlib_init();
	
	requests_high_prio = g_async_queue_new();
	requests_normal_prio = g_async_queue_new();
	results = g_async_queue_new();
	
	offline_cond = g_cond_new();
	cond_mutex = g_mutex_new();
		
	if(1 >= (count = getNumericConfValue(UPDATE_THREAD_CONCURRENCY)))
		count = DEFAULT_UPDATE_THREAD_CONCURRENCY;
	
	for(i = 0; i < count; i++)
		g_thread_create(update_dequeue_requests, GINT_TO_POINTER((i == 0)), FALSE, NULL);

	/* setup the processing of feed update results */
	g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
	                   100, 
			   update_dequeue_results, 
			   NULL,
			   NULL);
}
