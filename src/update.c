/*
   feed updating functionality
      
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

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

	g_assert(NULL != fp);
	g_assert(NULL != fp->request);	
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
	feedHandlerPtr		fhp;
	gint			type;
	gchar			*source;
	gint			error = 1;
	
	g_assert(NULL != request);
	g_assert(NULL != request->fp);
	
	while(1) {
		type = getFeedType(request->fp);
		g_assert(NULL != feedHandler);
		if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&type))) {
			/* can happen during a long update e.g. of an OCS directory, then the type is not set, FIXME ! */
			//g_warning(g_strdup_printf(_("internal error! unknown feed type %d while updating feeds!"), type));
			break;
		}

		/* reset feed update counter */
		request->fp->updateCounter = request->fp->updateInterval;

		if(NULL == (source = getFeedSource(request->fp))) {
			g_warning(_("Feed source is NULL! This should never happen - cannot update!"));
			break;
		}

		request->new_fp = getNewFeedStruct();
		g_free(request->feedurl);
		request->feedurl = g_strdup(source);	/* strdup because it might be changed in netio.c */
		if(NULL != (request->new_fp->data = downloadURL(request))) {
			request->new_fp->source = g_strdup(source);
			g_assert(NULL != fhp->readFeed);
			(*(fhp->readFeed))(request->new_fp);	/* parse the XML data ... */
			error = 0;
		} else {
			freeFeed(request->new_fp);
		}
		
		break;
	}
	
	if(0 != error)
		request->new_fp = NULL;
	
	/* finally return the request */
	g_async_queue_push(results, (gpointer)request);
}
