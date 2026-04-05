/**
 * @file update_job_queue.c  handling async concurrent update processing
 *
 * Copyright (C) 2003-2026 Lars Windolf <lars.windolf@gmx.de>
 * Copyright (C) 2004-2006 Nathan J. Conrad <t98502@users.sourceforge.net>
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

#include "update_job_queue.h"

#include "conf.h"
#include "debug.h"
#include "node_providers/feed.h"
#include "update.h"

/* global update job list, used for lookups when cancelling */
static GSList	*jobs = NULL;

static guint	currentJobCount = 0;	// actual number of pending / processing jobs
static guint	maxcount = 0;		// previous max number of jobs (gets reset when currentJobCount = 0)

static GThreadPool *normalPool = NULL;		// thread pool for normal priority request processing
static GThreadPool *priorityPool = NULL;	// thread pool for high priority request processing
static GThreadPool *resultPool = NULL;		// thread pool for result post-processing (needed as we support blocking filter scripts)

void
update_job_queue_add (gpointer data, updateFlags flags)
{
	UpdateJob *job = (UpdateJob *)data;

	jobs = g_slist_append (jobs, job);

	g_assert (job->state == JOB_STATE_PENDING);

	// Count all subscription jobs (but ignore HTML5, favicon and other download requests)
	if (!(job->flags & UPDATE_REQUEST_NO_FEED))
		currentJobCount++;

	if (flags & UPDATE_REQUEST_PRIORITY_HIGH)
		g_thread_pool_push (priorityPool, (gpointer)job, NULL);
	else
		g_thread_pool_push (normalPool, (gpointer)job, NULL);
}

void
update_job_queue_finish (gpointer data)
{
	UpdateJob *job = (UpdateJob *)data;

	g_assert (job->state == JOB_STATE_FINISHED);
	g_thread_pool_push (resultPool, (gpointer)job, NULL);
}

void
update_job_cancel_by_owner (gpointer owner)
{
	GSList	*iter = jobs;

	while (iter) {
		UpdateJob *job = (UpdateJob *)iter->data;
		if (job->owner == owner)
			job->callback = NULL;
		iter = g_slist_next (iter);
	}
}

void
update_job_queue_remove (gpointer job)
{
	if (!g_slist_find (jobs, job))
		return;

	jobs = g_slist_remove (jobs, job);

	// Count all subscription jobs (but ignore HTML5, favicon and other download requests)
	if (!(((UpdateJob *)job)->flags & UPDATE_REQUEST_NO_FEED))
		currentJobCount--;
}

void
update_job_queue_get_count (guint *count, guint *max)
{
	debug (DEBUG_UPDATE, "update job queue thread pools unprocessed: normal=%d / prio=%d / result=%d , running: normal=%d / prio=%d / result=%d",
	       g_thread_pool_unprocessed (normalPool),
	       g_thread_pool_unprocessed (priorityPool),
	       g_thread_pool_unprocessed (resultPool),
	       g_thread_pool_get_num_threads (normalPool),
	       g_thread_pool_get_num_threads (priorityPool),
	       g_thread_pool_get_num_threads (resultPool));

	*count = currentJobCount;
	if (*count > maxcount)
		maxcount = *count;

        if (*count == 0)
	    maxcount = 0; // reset max when no jobs are running

	*max = maxcount;
}

typedef void (*UpdateJobFunc)(gpointer job);

static void
update_job_queue_run (gpointer data, gpointer userdata)
{
	((UpdateJobFunc)userdata)(data);
}

void
update_init (void)
{
	gint max_jobs;
	conf_get_int_value (MAX_UPDATE_THREADS, &max_jobs);
	normalPool	= g_thread_pool_new (update_job_queue_run, (gpointer)update_job_execute,        max_jobs, FALSE, NULL);
	priorityPool	= g_thread_pool_new (update_job_queue_run, (gpointer)update_job_execute,        max_jobs, FALSE, NULL);
	resultPool	= g_thread_pool_new (update_job_queue_run, (gpointer)update_job_process_result, max_jobs, FALSE, NULL);
}

void
update_deinit (void)
{
	GSList	*iter = jobs;

	/* Cancel all pending jobs, to avoid async callbacks accessing the GUI */
	while (iter) {
		UpdateJob *job = (UpdateJob *)iter->data;
		job->callback = NULL;
		iter = g_slist_next (iter);
	}

	g_thread_pool_free (normalPool, TRUE, TRUE);
	g_thread_pool_free (priorityPool, TRUE, TRUE);
	g_thread_pool_free (resultPool, TRUE, TRUE);
	normalPool = NULL;
	priorityPool = NULL;
	resultPool = NULL;

	g_slist_free (jobs);
	jobs = NULL;
}
