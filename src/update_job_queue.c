/**
 * @file update_job_queue.c  handling async concurrent update processing
 *
 * Copyright (C) 2003-2024 Lars Windolf <lars.windolf@gmx.de>
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

#include "debug.h"
#include "node_providers/feed.h"
#include "update.h"

/* global update job list, used for lookups when cancelling */
static GSList	*jobs = NULL;

static GThreadPool *normalPool = NULL;
static GThreadPool *priorityPool = NULL;
// FIXME: make configurable
#define MAX_ACTIVE_JOBS	5

static void
update_start_job (gpointer data, gpointer user_data)
{
	UpdateJob *job = (UpdateJob *)data;

	job->state = JOB_STATE_PROCESSING;

	debug (DEBUG_UPDATE, "processing request (%s)", job->request->source);
	if (job->callback == NULL)
		update_job_finished (job);
	else
		update_job_execute (job);
}

void
update_job_queue_add (gpointer job, updateFlags flags)
{
	jobs = g_slist_append (jobs, job);

	if (flags & UPDATE_REQUEST_PRIORITY_HIGH)
		g_thread_pool_push (priorityPool, (gpointer)job, NULL);
	else
		g_thread_pool_push (normalPool, (gpointer)job, NULL);
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
	jobs = g_slist_remove (jobs, job);
}

static void
update_job_queue_count_foreach_func (gpointer data, gpointer user_data)
{
	UpdateJob *	job = (UpdateJob *)data;
	guint		*count = (guint *)user_data;

	// Count all subscription jobs (but ignore HTML5, favicon and other download requests)
	if (!(job->flags & UPDATE_REQUEST_NO_FEED))
		(*count)++;
}

static guint maxcount = 0;

void
update_job_queue_get_count (guint *count, guint *max)
{
	*count = 0;
	g_slist_foreach (jobs, update_job_queue_count_foreach_func, count);

	if (*count > maxcount)
		maxcount = *count;

        if (*count == 0)
	    maxcount = 0; // reset max when no jobs are running

	*max = maxcount;
}

void
update_init (void)
{
	normalPool = g_thread_pool_new ((GFunc)update_start_job, NULL, MAX_ACTIVE_JOBS, FALSE, NULL);
	priorityPool = g_thread_pool_new ((GFunc)update_start_job, NULL, MAX_ACTIVE_JOBS, FALSE, NULL);
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
	normalPool = NULL;
	priorityPool = NULL;

	g_slist_free (jobs);
	jobs = NULL;
}
