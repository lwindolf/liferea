/*
   feed updating functionality
      
   Copyright (C) 2003, 2004 Lars Lindner <lars.lindner@gmx.net>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "netio.h"
#include "update.h"
#include "callbacks.h"
#include "support.h"

/* communication queues for requesting updates and sending the results */
GAsyncQueue	*requests = NULL;
GAsyncQueue	*results = NULL;

static GMutex * cond_mutex = NULL;	// FIXME: 0.4.8 remove me
static GCond * qcond = NULL;		// FIXME: 0.4.8 remove me

extern GMutex * feeds_lock;		// FIXME: 0.4.8 remove me
extern GHashTable *feeds;		// FIXME: 0.4.8 remove me
/* prototypes */
static void *update_thread_main(void *data);
static void *autoUpdateMainLoop(void *data);	// FIXME: 0.4.8 remove me
static void doUpdateFeedCounter(gpointer key, gpointer value, gpointer userdata);	// FIXME: 0.4.8 remove me

/* Function to set up a new feed request structure.
   If fp is not NULL the request and the fp cross
   pointers are set electrically! */
gpointer update_request_new(feedPtr fp) {
	struct feed_request	*request;
	
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
	return (gpointer)request;
}

void update_request_free(gpointer request) {

	if(NULL != request) {
		g_free(((struct feed_request *)request)->lastmodified);
		g_free(((struct feed_request *)request)->feedurl);
		g_free(request);
	}
}

/* sets up a thread which minutly does update
   the feed update counter values and adds
   feed update requests to the update queue
   if necessary */
GThread * initAutoUpdateThread(void) {	// FIXME: 0.4.8 timeout!!!

	cond_mutex = g_mutex_new();
	qcond = g_cond_new();

	return g_thread_create(autoUpdateMainLoop, NULL, FALSE, NULL);
}

/* sets up a thread to process the update
   request queue */
GThread * update_thread_init(void) {

	requests = g_async_queue_new();
	results = g_async_queue_new();
		
	return g_thread_create(update_thread_main, NULL, FALSE, NULL);
}

static void *autoUpdateMainLoop(void *data) {	// FIXME: 0.4.8 timeout!!!
	const int	TIMEOUT_SECS = 60;
	GTimeVal 	sleep_until;
	gboolean	was_timeout;
	
	for(;;)	{
                g_mutex_lock(cond_mutex);
		
		/* sleep for TIMEOUT_SECS unless someone wakes us up */
		g_get_current_time(&sleep_until);
		g_time_val_add(&sleep_until, TIMEOUT_SECS*G_USEC_PER_SEC);
		
		was_timeout = FALSE;
		while(!was_timeout){
			was_timeout = !g_cond_timed_wait(qcond, cond_mutex, &sleep_until);
		}
                g_mutex_unlock(cond_mutex);
		
		g_mutex_lock(feeds_lock);
		g_hash_table_foreach(feeds, doUpdateFeedCounter, NULL);
		g_mutex_unlock(feeds_lock);
	}
}

static void *update_thread_main(void *data) {
	struct feed_request *request;
	
	for(;;)	{
		request = g_async_queue_pop(requests);
		g_assert(NULL != request);
		downloadURL(request);

		if(NULL == request->fp) {
			/* request was abandoned (feed deleted) */
			g_free(request->data);
			update_request_free(request);
		} else {
			/* return the request so the GUI can merge the feeds and display the results... */
			g_async_queue_push(results, (gpointer)request);
		}
	}
}

/** adds another download request to the request queue */
void update_thread_add_request(struct feed_request *new_request) {

	g_async_queue_push(requests, new_request);
}

// FIXME: 0.4.8 move this to feed.c
static void doUpdateFeedCounter(gpointer key, gpointer value, gpointer userdata) {
	feedPtr		fp = (feedPtr)value;
	gint 		counter;
//g_print("update counter for %s is %d\n", getFeedTitle(fp), getFeedUpdateCounter(fp));
	if(0 < (counter = getFeedUpdateCounter(fp))) 
		setFeedUpdateCounter(fp, counter - 1);
	
	if(0 == counter) {
		setFeedUpdateCounter(fp, getFeedUpdateInterval(fp));
		update_thread_add_request((struct feed_request *)fp->request);
	}
}
