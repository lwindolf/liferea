/**
 * @file update.c feed update request processing
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

#include <unistd.h>
#include <stdio.h>
#include <sys/wait.h>
#include "support.h"
#include "debug.h"
#include "update.h"
#include "conf.h"
#include "ui_queue.h"
#include "ui_mainwindow.h"
#include "net/downloadlib.h"

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

/* filter idea (and some of the code) was taken from Snownews */
static char* filter(gchar *cmd, gchar *data, size_t *size) {
	int fd;
	gchar *command;
	const gchar *tmpdir = g_get_tmp_dir();
	char *tmpfilename;
	char		*out = NULL;
	FILE *file, *p;
	
	tmpfilename = g_strdup_printf("%s" G_DIR_SEPARATOR_S "liferea-XXXXXX", tmpdir);
	
	fd = g_mkstemp(tmpfilename);
	
	if (fd == -1) {
		odebug1("Error opening temp file %s to use for filtering!", tmpfilename);
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
		pclose(p);
		out[*size] = '\0';
	}
	/* Clean up. */
	unlink (tmpfilename);
	g_free(tmpfilename);
	return out;
}

void download_process(struct request *request) {
	FILE *f;
	int status;

	request->data = NULL;

	if(request->source[0] == '|') {
		/* if the first char is a | we have a pipe else a file */
		request->size = 0;
		f = popen(&request->source[1], "r");
		if(NULL != f) {
			while(!feof(f)) {
				size_t len;
				request->data = g_realloc(request->data, request->size+1025);
				len = fread(&request->data[request->size], 1, 1024, f);
				if (len > 0)
					request->size += len;
			}
			status = pclose(f);
			if (WIFEXITED(status) && WEXITSTATUS(status) == 0) request->httpstatus = 200;
			else request->httpstatus = 404;
			request->data[request->size] = '\0';
		} else {
			ui_mainwindow_set_status_bar(_("Error: Could not open pipe \"%s\""), &request->source[1]);
		}
	} else if (NULL != strstr(request->source, "://")) {
		downloadlib_process_url(request);
		if (request->httpstatus >= 400) {
			g_free(request->data);
			request->data = NULL;
			request->size = 0;
		}
	} else {
		if(g_file_test(request->source, G_FILE_TEST_EXISTS)) {
			/* we have a file... */
			if((!g_file_get_contents(request->source, &(request->data), &(request->size), NULL)) || (request->data[0] == '\0')) {
				request->httpstatus = 403;
				ui_mainwindow_set_status_bar(_("Error: Could not open file \"%s\""), request->source);
			} else {
				g_assert(NULL != request->data);
				request->httpstatus = 200;
			}
		} else {
			ui_mainwindow_set_status_bar(_("Error: There is no file \"%s\""), request->source);
			request->httpstatus = 404;
		}
		
	}

	/* And execute the postfilter */
	if (request->data != NULL && request->filtercmd != NULL) {
		size_t size;
		gchar *tmp = filter(request->filtercmd, request->data, &size);
		if (tmp != NULL) {
			g_free(request->data);
			request->data = tmp;
			request->size = size;
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
	if(NULL != request) {
		g_free(request->source);
		g_free(request->filtercmd);
		g_free(request->etag);
		g_free(request->data);
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
	ui_timeout_add(100, download_dequeuer, NULL);
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
			if(NULL == request) request = g_async_queue_pop(requests_normal_prio);
		}
		g_assert(NULL != request);
		debug1(DEBUG_UPDATE, "processing received request (%s)", request->source);
		if (request->callback == NULL) {
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

	if((online = mode)) {
		g_mutex_lock(cond_mutex);
		g_cond_signal(offline_cond);
		g_mutex_unlock(cond_mutex);
	}
}

gboolean download_is_online(void) {

	return online;
}

static gboolean download_dequeuer(gpointer user_data) {
	struct request *request;
	
	while((request = g_async_queue_try_pop(results)) != NULL) {

		if(request->callback == NULL) {
			debug1(DEBUG_UPDATE, "freeing cancelled request (%s)", request->source);
		} else
			(request->callback)(request);
		download_request_free(request);
	}
	return TRUE;

}

