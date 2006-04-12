/**
 * @file update.c feed update request processing
 *
 * Copyright (C) 2003-2005 Lars Lindner <lars.lindner@gmx.net>
 * Copyright (C) 2004-2005 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include <string.h>
#include "common.h"
#include "support.h"
#include "debug.h"
#include "update.h"
#include "conf.h"
#include "ui/ui_mainwindow.h"
#include "net/downloadlib.h"
#include "ui/ui_tray.h"

/* must never be less than 2, because first thread works exclusivly on high prio requests */
#define DEFAULT_UPDATE_THREAD_CONCURRENCY	4

/* communication queues for requesting updates and sending the results */
GAsyncQueue	*requests_high_prio = NULL;
GAsyncQueue	*requests_normal_prio = NULL;
GAsyncQueue	*results = NULL;

/* condition mutex for offline mode */
static GMutex	*cond_mutex = NULL;
static GCond	*offline_cond = NULL;
static gboolean	online = TRUE;

/* prototypes */
static void *download_thread_main(void *data);
static gboolean download_dequeuer(gpointer user_data);
gboolean download_requeue(gpointer data);
gboolean download_retry(struct request * request);

/* filter idea (and some of the code) was taken from Snownews */
static char* filter(gchar *cmd, gchar *data, gchar **errorOutput, size_t *size) {
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

void download_process(struct request *request) {
	FILE	*f;
	size_t	len;
	int	status;
	gchar	*cmd, *tmp, *errorOutput;

	request->data = NULL;
	request->size = 0;
	if(*(request->source) == '|') {
		/* if the first char is a | we have a pipe else a file */
		debug1(DEBUG_UPDATE, "executing command \"%s\"...\n", (request->source) + 1);
		f = popen((request->source) + 1, "r");
		if(NULL != f) {
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
			
			if(NULL != request->data)
				request->data[request->size] = '\0';
		} else {
			ui_mainwindow_set_status_bar(_("Error: Could not open pipe \"%s\""), (request->source) + 1);
			request->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */
		}
	} else if(NULL != strstr(request->source, "://") && strncmp(request->source, "file://",7)) {
		/* just a web URL */
		downloadlib_process_url(request);
		if(request->httpstatus >= 400) {
			g_free(request->data);
			request->data = NULL;
			request->size = 0;
		}
	} else {
		gchar *filename = request->source;
		gchar *anchor;

		if(!strncmp(filename, "file://",7))
			filename += 7;
		
		if(anchor = strchr(filename, '#'))
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

	/* And execute the postfilter */
	if(request->data != NULL && request->filtercmd != NULL) {
		/* we allow two types of filters: XSLT stylesheets and arbitrary commands */
		if((strlen(request->filtercmd) > 4) &&
		   (0 == strcmp(".xsl", request->filtercmd + strlen(request->filtercmd) - 4))) {
			/* in case of XSLT we call xsltproc */
			cmd = g_strdup_printf("xsltproc %s -", request->filtercmd);
		} else {
			/* otherwise we just keep the given command */
			cmd = g_strdup(request->filtercmd);
		}

		errorOutput = NULL;
		tmp = filter(cmd, request->data, &errorOutput, &len);
		g_free(request->filterErrors);
		request->filterErrors = errorOutput;
		if(tmp != NULL) {
			g_free(request->data);
			request->data = tmp;
			request->size = len;
		}
	}
}

gpointer download_request_new() {
	struct request	*request;

	debug_enter("update_request_new");	
	   
	request = g_new0(struct request, 1);

	debug_exit("update_request_new");
	
	return (gpointer)request;
}

void download_request_free(struct request *request) {
	
	debug_enter("update_request_free");
	if(request) {
		g_free(request->source);
		g_free(request->filtercmd);
		g_free(request->filterErrors);
		g_free(request->lastmodified);
		g_free(request->etag);
		g_free(request->data);
		g_free(request->contentType);
		g_free(request);
	}
	debug_exit("update_request_free");
}

void download_init(void) {
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
		g_thread_create(download_thread_main, (void *)(i == 0), FALSE, NULL);

	/* setup the processing of feed update results */
	g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE,
	                   100, 
			   download_dequeuer, 
			   NULL,
			   NULL);
}

static void *download_thread_main(void *data) {
	struct request	*request;
	gboolean	high_priority = (gboolean)data;

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
		debug0(DEBUG_UPDATE, "waiting for request...");
		if(high_priority) {
			request = g_async_queue_pop(requests_high_prio);
		} else {
			request = g_async_queue_try_pop(requests_high_prio);
			if(NULL == request) 
				request = g_async_queue_pop(requests_normal_prio);
		}
		g_assert(NULL != request);
		debug1(DEBUG_UPDATE, "processing received request (%s)", request->source);
		if(request->callback == NULL) {
			debug1(DEBUG_UPDATE, "freeing cancelled request (%s)", request->source);
			download_request_free(request);
		} else {
			download_process(request);
			
			/* return the request so the GUI thread can merge the feeds and display the results... */
			debug0(DEBUG_UPDATE, "request finished");
			g_async_queue_push(results, (gpointer)request);
		}
	}
}

void download_queue(struct request *new_request) {

	g_assert(NULL != new_request);

	if(new_request->priority == 1)
		g_async_queue_push(requests_high_prio, new_request);
	else
		g_async_queue_push(requests_normal_prio, new_request);
}

void download_set_online(gboolean mode) {

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

gboolean download_is_online(void) {

	return online;
}

static gboolean download_dequeuer(gpointer user_data) {
	struct request	*request;
	
	while((request = g_async_queue_try_pop(results)) != NULL) {

		if(request->callback == NULL) {
			debug1(DEBUG_UPDATE, "freeing cancelled request (%s)", request->source);
			download_request_free(request);
		} else if(!(download_retry(request))) {
			(request->callback)(request);
			download_request_free(request);
		}
	}
	return TRUE;

}

/* Wrapper around download_requeue, for convenient call from g_timeout */
gboolean download_requeue(gpointer data) {
	struct request* request = (struct request*)data;
	
	if(request->callback == NULL) {
		debug2(DEBUG_UPDATE, "Freeing request of cancelled retry #%d for \"%s\"", request->retriesCount, request->source);
		download_request_free(request);
	} else {
		download_queue(request);
	}
	return FALSE;
}

/* Check if a request should be retried.
 * If yes, schedule a retry and returns TRUE. 
 * Else, returns FALSE. */
gboolean download_retry(struct request *request) {
	guint retryDelay;
	gushort i;

	if((!request->allowRetries) || (REQ_MAX_NUMBER_OF_RETRIES <= request->retriesCount))
		return FALSE;
	switch(request->returncode) {
		case NET_ERR_UNKNOWN:
		case NET_ERR_CONN_FAILED:
		case NET_ERR_SOCK_ERR:
		case NET_ERR_HOST_NOT_FOUND:
		case NET_ERR_TIMEOUT:
			break; /* Ok, there will be a retry. */
		default: return FALSE;
	}

	/* There should have been no received data, hence nothing to free. */
	g_assert(request->data == NULL);
	g_assert(request->contentType == NULL);
	/* Note: in case of permanent HTTP redirection leading to a network
	 * error, retries will be done on the redirected request->source. */

	/* Prepare for a retry: increase counter and calculate delay */
	retryDelay = REQ_MIN_DELAY_FOR_RETRY;
	for(i = 0; i < request->retriesCount; i++)
		retryDelay *= 3;
	if(retryDelay > REQ_MAX_DELAY_FOR_RETRY)
		retryDelay = REQ_MAX_DELAY_FOR_RETRY;

	/* Requeue the request after the waiting delay */
	g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, 1000 * retryDelay, download_requeue, request, NULL);
	request->retriesCount++;
	ui_mainwindow_set_status_bar(_("Could not download \"%s\". Will retry in %d seconds."), request->source, retryDelay);

	return TRUE;
}

gboolean download_cancel_retry(struct request *request) {

	if(0 < request->retriesCount) {
		request->callback = NULL;
		debug2(DEBUG_UPDATE, "Cancelled retry #%d for request \"%s\". It should be freed soon.", request->retriesCount, request->source);
		return TRUE;
	}
	debug1(DEBUG_UPDATE, "Could not cancel request \"%s\", it's not an awaiting retry.", request->source);
	return FALSE;
}
