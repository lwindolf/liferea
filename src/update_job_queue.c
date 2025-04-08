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

static GAsyncQueue *pendingHighPrioJobs = NULL;
static GAsyncQueue *pendingJobs = NULL;
static guint numberOfActiveJobs = 0;
// FIXME: make configurable
#define MAX_ACTIVE_JOBS	5

static gboolean
update_dequeue_job (gpointer user_data)
{
	UpdateJob *job;

	if (!pendingJobs)
		return FALSE;	/* we must be in shutdown */

	if (numberOfActiveJobs >= MAX_ACTIVE_JOBS)
		return FALSE;	/* we'll be called again when a job finishes */

	job = (UpdateJob *)g_async_queue_try_pop (pendingHighPrioJobs);

	if (!job)
		job = (UpdateJob *)g_async_queue_try_pop (pendingJobs);

	if (!job)
		return FALSE;	/* no request at the moment */

	numberOfActiveJobs++;

	job->state = JOB_STATE_PROCESSING;

	debug (DEBUG_UPDATE, "processing request (%s)", job->request->source);
	if (job->callback == NULL) {
		update_job_finished (job);
	} else {
		update_job_execute (job);
	}

	return FALSE;
}

void
update_job_queue_add (gpointer job, updateFlags flags)
{
	jobs = g_slist_append (jobs, job);

	if (flags & UPDATE_REQUEST_PRIORITY_HIGH) {
		g_async_queue_push (pendingHighPrioJobs, (gpointer)job);
	} else {
		g_async_queue_push (pendingJobs, (gpointer)job);
	}

	g_idle_add (update_dequeue_job, NULL);
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
update_job_queue_finished (void)
{
	g_assert(numberOfActiveJobs > 0);
	numberOfActiveJobs--;
	g_idle_add (update_dequeue_job, NULL);
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

        // FIXME: when does maxcount ever get reset?

	*max = maxcount;
}

void
update_init (void)
{
	pendingJobs = g_async_queue_new ();
	pendingHighPrioJobs = g_async_queue_new ();
}

void
update_deinit (void)
{
	GSList	*iter = jobs;

	/* Cancel all jobs, to avoid async callbacks accessing the GUI */
	while (iter) {
		UpdateJob *job = (UpdateJob *)iter->data;
		job->callback = NULL;
		iter = g_slist_next (iter);
	}

	g_async_queue_unref (pendingJobs);
	g_async_queue_unref (pendingHighPrioJobs);

	g_slist_free (jobs);
	jobs = NULL;
}
