/*
   auto update functionality
   
   The basics of this code were inspired by queue.c of
   the Pan News Reader so the following copyright is implied.

   Copyright (C) 2002  Charles Kerr <charles@rebelbase.com>
   
   Adoption and additional coding
   
   Copyright (C) 2003 Lars Lindner <lars.lindner@gmx.net>

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
   
   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.
   
   You should have received a copy of the GNU Library General Public License
   along with this library; see the file COPYING.LIB.  If not, write to
   the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include "backend.h"
#include "rss_channel.h"
#include "cdf_channel.h"
#include "ocs_dir.h"
#include "update.h"
#include "callbacks.h"
#include "support.h"

extern GHashTable	*entries;

GMutex * entries_lock = NULL;
static GMutex * cond_mutex = NULL;
/*static GMutex * todo_mutex = NULL;*/

static GCond * qcond = NULL;
static gboolean work_to_do = FALSE;

/* prototypes */
static void *update_mainloop(void *data);
static void updateEntries(gboolean autoupdate);

void updateNow(void)
{
	g_mutex_lock(cond_mutex);
	work_to_do = TRUE;
	g_cond_signal(qcond);
	g_mutex_unlock(cond_mutex);
}

GThread * initUpdateThread(void) {

	entries_lock = g_mutex_new();
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

		updateEntries(was_timeout);		
	}
}

static void doUpdateFeedCounter(gpointer key, gpointer value, gpointer userdata) {
	channelPtr	cp = (channelPtr)value;

	if((cp == NULL) || (!IS_FEED(cp->type)))
		return;

	if(cp->updateCounter > 0) {
		//g_print("-- %d %s\n", cp->updateCounter, cp->source);
		cp->updateCounter--;
	}
}

static void doUpdateEntries(gpointer key, gpointer value, gpointer userdata) {
	entryPtr	ep = (entryPtr)value;
	channelPtr	cp;
	channelPtr	new_cp;
	CDFChannelPtr	cdfp;
	CDFChannelPtr	new_cdfp;	
	directoryPtr	dp;		
	directoryPtr	new_dp;
	gchar		*tmp_key;
	
	if(ep == NULL)
		return;
	
	if(FST_FEED == ep->type) {
	
		cp = (channelPtr)ep;
		if(cp->updateCounter == 0) {
		
			new_cp = (channelPtr)readRSSFeed(cp->source);
			g_assert(new_cp != NULL);
			new_cp = (channelPtr)mergeRSSFeed(cp, new_cp);
			g_assert(new_cp != NULL);
			
			g_mutex_lock(entries_lock);			
			g_hash_table_insert(entries, key, (gpointer)new_cp);
			g_mutex_unlock(entries_lock);
			
			gdk_threads_enter();
			tmp_key = getMainFeedListViewSelection();	// FIXME: inperformant

			if(NULL != tmp_key) {
				if(0 == strcmp(tmp_key, new_cp->key)) {
					clearItemList();
					loadItemList(new_cp->key, NULL);
				}
			}

			g_mutex_lock(entries_lock);
			redrawFeedList();	// FIXME: maybe this is overkill
			g_mutex_unlock (entries_lock);
			
			gdk_threads_leave();
		}
	} else if(FST_CDF == ep->type) { /* FIXME: hmm the same code as for FST_FEED */
	
		cdfp = (CDFChannelPtr)ep;
		if(cdfp->updateCounter == 0) {
		
			new_cdfp = (CDFChannelPtr)readCDFFeed(cdfp->source);
			g_assert(new_cdfp != NULL);
			new_cdfp = (CDFChannelPtr)mergeCDFFeed(cdfp, new_cdfp);
			g_assert(new_cdfp != NULL);
			
			g_mutex_lock(entries_lock);			
			g_hash_table_insert(entries, key, (gpointer)new_cdfp);
			g_mutex_unlock(entries_lock);
			
			gdk_threads_enter();
			tmp_key = getMainFeedListViewSelection();	// FIXME: inperformant

			if(NULL != tmp_key) {
				if(0 == strcmp(tmp_key, new_cdfp->key)) {
					clearItemList();
					loadItemList(new_cdfp->key, NULL);
				}
			}

			g_mutex_lock(entries_lock);
			redrawFeedList();	// FIXME: maybe this is overkill
			g_mutex_unlock (entries_lock);
			
			gdk_threads_leave();
		}		
	} else if(FST_OCS == ep->type) {
	
		dp = (directoryPtr)ep;
		if(dp->updateCounter == 0) {
			new_dp = (directoryPtr)readOCS(dp->source);		
			// FIXME: free old OCS

			g_mutex_lock(entries_lock);			
			g_hash_table_insert(entries, key, (gpointer)new_dp);
			g_mutex_unlock(entries_lock);
			
			gdk_threads_enter();

			/* OCS directories can only be manually updated so
			   we can always reload the item list */
			clearItemList();
			loadItemList(new_dp->key, NULL);

			g_mutex_lock(entries_lock);
			redrawFeedList();	// FIXME: maybe this is overkill
			g_mutex_unlock (entries_lock);
			
			gdk_threads_leave();
		}
	} else {
	
		g_assert(_("internal error! unknown entry type while updating!"));
	}
}

static void updateEntries(gboolean autoupdate) {

	/* we update in two steps, first we decrement all updateCounter by 1 */
	if(autoupdate) {
		g_mutex_lock(entries_lock);
		g_hash_table_foreach(entries, doUpdateFeedCounter, NULL);
		g_mutex_unlock(entries_lock);
	}
	
	/* second step - scan the feed list for feeds that have to be updated */
	g_hash_table_foreach(entries, doUpdateEntries, NULL);	
}
