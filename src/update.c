/*
   auto update functionality
   
   The basics of this code were inspired by queue.c of
   the Pan News Reader so the following copyright is implied.

   Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
   
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
#include "update.h"
#include "callbacks.h"
#include "support.h"

extern GHashTable	*feeds;
extern GHashTable	*feedHandler;

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

	if(0 < (counter = getFeedUpdateCounter(fp))) 
		setFeedUpdateCounter(fp, counter - 1);
}

static void doUpdateFeeds(gpointer key, gpointer value, gpointer userdata) {
	feedHandlerPtr	fhp;
	feedPtr		fp = (feedPtr)value;
	feedPtr		new_fp;
	gint		type;
	gchar		*tmp_key, *source;
		
	if(fp == NULL)
		return;

	type = getFeedType(fp);
	if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&type))) {
		g_warning(g_strdup_printf(_("internal error! unknown feed type %d while updating feeds!"), type));
		return;
	}

	if(0 == getFeedUpdateCounter(fp)) {
		if(NULL == (source = getFeedSource(fp))) {
			g_warning(_("Feed source is NULL! This should never happen - cannot update!"));
			return;
		}

		g_assert(NULL != fhp->readFeed);
		new_fp = (feedPtr)(*(fhp->readFeed))(source);
		g_assert(NULL != new_fp);
		
		new_fp = mergeFeed(fp, new_fp);
		g_assert(new_fp != NULL);
		
		g_mutex_lock(feeds_lock);
		/* update feed key */
		g_hash_table_insert(feeds, key, (gpointer)new_fp);	
		/* update all vfolders */
		g_hash_table_foreach(feeds, removeOldItemsFromVFolders, fp);
		g_hash_table_foreach(feeds, scanFeed, new_fp);
		g_mutex_unlock(feeds_lock);
				
		gdk_threads_enter();
		tmp_key = getMainFeedListViewSelection();	// FIXME: inperformant

		if(NULL != tmp_key) {
			if(0 == strcmp(tmp_key, (gchar *)key)) {
				clearItemList();
				loadItemList(new_fp, NULL);
			}
		}
		g_free(fp);
		
		g_mutex_lock(feeds_lock);
		redrawFeedList();	// FIXME: maybe this is overkill ;=)
		g_mutex_unlock(feeds_lock);
		
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
