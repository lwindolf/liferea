/*
   feed updating functionality
   
   The basics of this code were inspired by queue.c of
   the Pan News Reader which was written by Charles Kerr.
   
   Adoption and additional coding
   
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

#include "feed.h"
#include "vfolder.h"
#include "netio.h"
#include "update.h"
#include "callbacks.h"
#include "support.h"

extern GHashTable	*feeds;
extern GHashTable	*feedHandler;
extern feedPtr		selected_fp;

GMutex * feeds_lock = NULL;
static GMutex * cond_mutex = NULL;
/*static GMutex * todo_mutex = NULL;*/

static GCond * qcond = NULL;
static gboolean work_to_do = FALSE;

/* prototypes */
static void *update_mainloop(void *data);
static void updateFeeds(gboolean autoupdate);

void updateNow(void)
{
	g_mutex_lock(cond_mutex);
	work_to_do = TRUE;
	g_cond_signal(qcond);
	g_mutex_unlock(cond_mutex);
}

GThread * initUpdateThread(void) {

	feeds_lock = g_mutex_new();
	cond_mutex = g_mutex_new();

	qcond = g_cond_new();
	
	return g_thread_create(update_mainloop, NULL, FALSE, NULL);
}

static void *update_mainloop(void *data) {
	const int TIMEOUT_SECS = 60;
	
	for (;;)
	{
		gboolean was_timeout;
		GTimeVal sleep_until;

		g_mutex_lock (cond_mutex);

		/* sleep for TIMEOUT_SECS unless someone wakes us up */
		g_get_current_time (&sleep_until);
		g_time_val_add (&sleep_until, TIMEOUT_SECS*G_USEC_PER_SEC);
		was_timeout = FALSE;
		while (!work_to_do && !was_timeout) {
			was_timeout = !g_cond_timed_wait (qcond, cond_mutex, &sleep_until);
		}
		work_to_do = FALSE;
                g_mutex_unlock (cond_mutex);
		updateFeeds(was_timeout);		
	}
}

static void doUpdateFeedCounter(gpointer key, gpointer value, gpointer userdata) {
	feedPtr		fp = (feedPtr)value;
	gint 		counter;
//g_print("update counter for %s is %d\n", getFeedTitle(fp), getFeedUpdateCounter(fp));
	if(0 < (counter = getFeedUpdateCounter(fp))) 
		setFeedUpdateCounter(fp, counter - 1);
}

static void doUpdateFeeds(gpointer key, gpointer value, gpointer userdata) {
	struct feed_request	request;
	feedHandlerPtr		fhp;
	feedPtr			fp = (feedPtr)value;
	feedPtr			tmp_fp, new_fp;
	gint			type;
	gchar			*tmp_key, *source;
		
	if(fp == NULL)
		return;

	type = getFeedType(fp);
	g_assert(NULL != feedHandler);
	if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&type))) {
		/* can happen during a long update e.g. of an OCS directory, then the type is not set, FIXME ! */
		//g_warning(g_strdup_printf(_("internal error! unknown feed type %d while updating feeds!"), type));
		return;
	}

	if(0 == getFeedUpdateCounter(fp)) {	
		fp->updateCounter = fp->updateInterval;
		
		if(NULL == (source = getFeedSource(fp))) {
			g_warning(_("Feed source is NULL! This should never happen - cannot update!"));
			return;
		}

		new_fp = getNewFeedStruct();
		request.feedurl = g_strdup(source);
		if(NULL != (new_fp->data = downloadURL(&request))) {
			new_fp->source = g_strdup(source);
			g_assert(NULL != fhp->readFeed);
			(*(fhp->readFeed))(new_fp);
			
			if(TRUE == fhp->merge)
				/* If the feed type supports merging... */
				mergeFeed(fp, new_fp);
			else {
				/* Otherwise we simply use the new feed info... */
				copyFeed(fp, new_fp);
				print_status(g_strdup_printf(_("\"%s\" updated..."), fp->title));
			}

			/* note this is to update the feed URL on permanent redirects */
			if(0 != strcmp(request.feedurl, fp->source)) {
				setFeedSource(fp, g_strdup(request.feedurl));			
				print_status(g_strdup_printf(_("The URL of \"%s\" has changed permanently and was updated."), fp->title));				
			}
		} else {
			print_status(g_strdup_printf(_("\"%s\" is not available!"), fp->title));
			freeFeed(new_fp);
			fp->available = FALSE;
		}
		g_free(request.feedurl);
		g_free(request.lastmodified);

		/* now fp contains the actual feed infos */
		saveFeed(fp);
			
		gdk_threads_enter();
		if((NULL != fp) && (selected_fp == fp)) {
			clearItemList();
			loadItemList(fp, NULL);
			preFocusItemlist();
		}
		redrawFeedList();	// FIXME: maybe this is overkill ;=)	
		gdk_threads_leave();
	}
}

static void updateFeeds(gboolean autoupdate) {

	/* we update in two steps, first we decrement all update counters */
	if(autoupdate) {
		g_mutex_lock(feeds_lock);
		g_hash_table_foreach(feeds, doUpdateFeedCounter, NULL);
		g_mutex_unlock(feeds_lock);
	}
	
	/* second step - scan the feed list for feeds that have to be updated */
	g_hash_table_foreach(feeds, doUpdateFeeds, NULL);	
}
