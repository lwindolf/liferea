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

extern GHashTable	*feeds;
extern GHashTable	*feedHandler;

/* communication queues for requesting updates and sending the results */
GAsyncQueue	*requests = NULL;
GAsyncQueue	*results = NULL;

static GMutex * cond_mutex = NULL;
static GCond * qcond = NULL;

extern GMutex * feeds_lock;

/* prototypes */
static void *updateMainLoop(void *data);
static void *autoUpdateMainLoop(void *data);
static void doUpdateFeedCounter(gpointer key, gpointer value, gpointer userdata);
static void doUpdateFeed(struct feed_request *request);

/* Function to set up a new feed request structure.
   If fp is not NULL the request and the fp cross
   pointers are set electrically! */
gpointer getNewRequestStruct(feedPtr fp) {
	struct feed_request	*request;
	
	/* we always reuse one request structure per feed, to
	   allow to reuse the lastmodified attribute of the
	   last request... */
	if(NULL == (request = (struct feed_request *)g_malloc(sizeof(struct feed_request)))) {
		g_error(_("Could not allocate memory!"));
		exit(1);
	} else {
		request->feedurl = NULL;
		request->lastmodified = NULL;
		request->lasthttpstatus = 0;
		request->fp = fp;
		if(NULL != fp)
			fp->request = (gpointer)request;
	}	
	return (gpointer)request;
}

void freeRequest(gpointer request) {

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
GThread * initAutoUpdateThread(void) {

	cond_mutex = g_mutex_new();
	qcond = g_cond_new();

	return g_thread_create(autoUpdateMainLoop, NULL, FALSE, NULL);
}

/* sets up a thread to process the update
   request queue */
GThread * initUpdateThread(void) {

	requests = g_async_queue_new();
	results = g_async_queue_new();
		
	return g_thread_create(updateMainLoop, NULL, FALSE, NULL);
}

static void *autoUpdateMainLoop(void *data) {
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

static void *updateMainLoop(void *data) {

	for(;;)	{
		doUpdateFeed(g_async_queue_pop(requests));
	}
}

/* method to be called by other threads to create requests */
void requestUpdate(feedPtr fp) {
	gchar			*source;
	gchar			*msg;
	
	g_assert(NULL != fp);
	
	msg = g_strdup_printf("updating \"%s\"", getFeedTitle(fp));
	print_status(msg);
	g_free(msg);
	
	if(NULL == (source = getFeedSource(fp))) {
		g_warning(_("Feed source is NULL! This should never happen - cannot update!"));
		return;
	}

	/* reset feed update counter */
	fp->updateCounter = fp->updateInterval;

	if(NULL == fp->request)
		getNewRequestStruct(fp);
	
	/* prepare request url (strdup because it might be
	   changed on permanent HTTP redirection in netio.c) */
	((struct feed_request *)fp->request)->feedurl = g_strdup(source);

	/* FIXME: check if feed is already in the queue! */
	g_async_queue_push(requests, (gpointer)fp->request);
}

static void doUpdateFeedCounter(gpointer key, gpointer value, gpointer userdata) {
	feedPtr		fp = (feedPtr)value;
	gint 		counter;
//g_print("update counter for %s is %d\n", getFeedTitle(fp), getFeedUpdateCounter(fp));
	if(0 < (counter = getFeedUpdateCounter(fp))) 
		setFeedUpdateCounter(fp, counter - 1);
	
	if(0 == counter) {
		setFeedUpdateCounter(fp, getFeedUpdateInterval(fp));
		requestUpdate(fp);
	}
}

static void doUpdateFeed(struct feed_request *request) {
	
	g_assert(NULL != request);

	/* do the request */		
	request->data = downloadURL(request);

	/* finally we return the request so the GUI can merge the feeds
	   and display the results... */
	g_async_queue_push(results, (gpointer)request);
}
