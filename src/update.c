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

#include "backend.h"
#include "rss_channel.h"
#include "cdf_channel.h"
#include "ocs_dir.h"
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
	entryPtr	ep = (entryPtr)value;
	gint 		counter;
	feedHandlerPtr	fhp;
	
	if((ep == NULL) || (!IS_FEED(ep->type)))
		return;

	/* we can use set/getFeedProp() here, this would cause deadlocks */
	if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&(ep->type)))) 
		g_error(_("internal error! unknown feed type while updating feed counters!"));	
	
	g_assert(NULL != fhp->getFeedProp);
	g_assert(NULL != fhp->setFeedProp);

	if(0 < (counter = (gint)(*(fhp->getFeedProp))(ep, FEED_PROP_UPDATECOUNTER))) 
		(*(fhp->setFeedProp))(ep, FEED_PROP_UPDATECOUNTER, (gpointer)(counter - 1));
	
}

static void doUpdateFeeds(gpointer key, gpointer value, gpointer userdata) {
	entryPtr	ep = (entryPtr)value;
	entryPtr	new_ep;
	gchar		*tmp_key, *source;
	feedHandlerPtr	fhp;
		
	if(ep == NULL)
		return;

	if(NULL == (fhp = g_hash_table_lookup(feedHandler, (gpointer)&(ep->type)))) 
		g_error(_("internal error! unknown feed type while updating feeds!"));	

	g_assert(NULL != fhp->getFeedProp);
	if(0 == (*(fhp->getFeedProp))(ep, FEED_PROP_UPDATECOUNTER)) {

		if(NULL == (source = (gchar *)(*(fhp->getFeedProp))(ep, FEED_PROP_SOURCE))) {
			g_warning(_("feed source is NULL! this should never happen!"));
			return;
		}

		g_assert(NULL != fhp->readFeed);
		new_ep = (entryPtr)(*(fhp->readFeed))(source);
		if(NULL == new_ep)
			return;		/* FIXME: the assert would be better but OCS still does not behave correctly! */
		//g_assert(NULL != new_ep);
		
		/* we don't merge some types, e.g. OCS directories... */
		if(NULL != fhp->mergeFeed) {
			new_ep = (entryPtr)(*(fhp->mergeFeed))((gpointer)ep, (gpointer)new_ep);
			g_assert(new_ep != NULL);
		}
		
		g_mutex_lock(feeds_lock);
		/* update feed key */
		g_hash_table_insert(feeds, key, (gpointer)new_ep);	
		/* update all vfolders */
		g_hash_table_foreach(feeds, removeOldItemsFromVFolders, ep);
		g_hash_table_foreach(feeds, scanFeed, new_ep);
		g_mutex_unlock(feeds_lock);
				
		gdk_threads_enter();
		tmp_key = getMainFeedListViewSelection();	// FIXME: inperformant

		if(NULL != tmp_key) {
			if(0 == strcmp(tmp_key, new_ep->key)) {
				clearItemList();
				loadItemList(new_ep->key, NULL);
			}
		}

		g_mutex_lock(feeds_lock);
		redrawFeedList();	// FIXME: maybe this is overkill
		g_mutex_unlock(feeds_lock);
			
		gdk_threads_leave();
	}
}

static void updateFeeds(gboolean autoupdate) {

	/* we update in two steps, first we decrement all updateCounter by 1 */
	if(autoupdate) {
		g_mutex_lock(feeds_lock);
		g_hash_table_foreach(feeds, doUpdateFeedCounter, NULL);
		g_mutex_unlock(feeds_lock);
	}
	
	/* second step - scan the feed list for feeds that have to be updated */
	g_hash_table_foreach(feeds, doUpdateFeeds, NULL);	
}
