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

#include "net/netio.h"
#include "debug.h"
#include "update.h"

/* communication queues for requesting updates and sending the results */
GAsyncQueue	*requests = NULL;
GAsyncQueue	*results = NULL;

/* prototypes */
static void *update_thread_main(void *data);

gpointer update_request_new(feedPtr fp) {
	struct feed_request	*request;

	debug_enter("update_request_new");	
	/* we always reuse one request structure per feed, to
	   allow to reuse the lastmodified attribute of the
	   last request... */
	   
	request = g_new0(struct feed_request, 1);
	request->feedurl = NULL;
	request->lastmodified = NULL;
	request->lasthttpstatus = 0;
	request->fp = fp;
	if(NULL != fp) {
		g_assert(fp->request == NULL);
		fp->request = (gpointer)request;
	}
	debug_exit("update_request_new");
	
	return (gpointer)request;
}

void update_request_free(gpointer request) {

	debug_enter("update_request_free");
	if(NULL != request) {
		g_free(((struct feed_request *)request)->lastmodified);
		g_free(((struct feed_request *)request)->feedurl);
		g_free(request);
	}
	debug_exit("update_request_free");
}

GThread * update_thread_init(void) {

	requests = g_async_queue_new();
	results = g_async_queue_new();
		
	g_thread_create(update_thread_main, NULL, FALSE, NULL);
	return g_thread_create(update_thread_main, NULL, FALSE, NULL);
}

static void *update_thread_main(void *data) {
	struct feed_request *request;
	
	for(;;)	{
		debug0(DEBUG_UPDATE, "waiting for request...");
		request = g_async_queue_pop(requests);
		g_assert(NULL != request);
		debug1(DEBUG_UPDATE, "processing received request (%s)", request->feedurl);
		downloadURL(request);

		if(NULL == request->fp) {
			debug0(DEBUG_UPDATE, "request abandoned (maybe feed was deleted)");
			g_free(request->data);
			update_request_free(request);
		} else {
			/* return the request so the GUI thread can merge the feeds and display the results... */
			debug0(DEBUG_UPDATE, "request finished");
			g_async_queue_push(results, (gpointer)request);
		}
	}
}

void update_thread_add_request(struct feed_request *new_request) {

	g_assert(NULL != new_request);
	g_async_queue_push(requests, new_request);
}

struct feed_request * update_thread_get_result(void) {

	g_assert(NULL != results);
	return g_async_queue_try_pop(results);
}
