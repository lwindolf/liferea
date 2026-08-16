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

enum {
	UPDATE_RUNNING,
	LAST_SIGNAL
};

static guint update_job_queue_signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (UpdateJobQueue, update_job_queue, G_TYPE_OBJECT)

static UpdateJobQueue *queue = NULL;

typedef void (*UpdateJobFunc)(gpointer job);

static void
update_job_queue_run (gpointer data, gpointer userdata)
{
	((UpdateJobFunc)userdata)(data);
}

static void
update_job_queue_finalize (GObject *object)
{
	G_OBJECT_CLASS(update_job_queue_parent_class)->finalize(object);

	/* Cancel all pending jobs, to avoid async callbacks accessing the GUI */
	GSList *iter = queue->jobs;
	while (iter) {
		UpdateJob *job = (UpdateJob *)iter->data;
		job->callback = NULL;
		iter = g_slist_next (iter);
	}

	g_thread_pool_free (queue->normalPool, TRUE, TRUE);
	g_thread_pool_free (queue->priorityPool, TRUE, TRUE);
	g_thread_pool_free (queue->resultPool, TRUE, TRUE);
	queue->normalPool = NULL;
	queue->priorityPool = NULL;
	queue->resultPool = NULL;

	g_slist_free (queue->jobs);
	queue->jobs = NULL;
	queue = NULL;
}

static void
update_job_queue_class_init (UpdateJobQueueClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = update_job_queue_finalize;

	update_job_queue_signals[UPDATE_RUNNING] =
		g_signal_new ("update-running",
		G_OBJECT_CLASS_TYPE (object_class),
		(GSignalFlags)(G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION),
		0,
		NULL,
		NULL,
		g_cclosure_marshal_VOID__VOID,
		G_TYPE_NONE,
		0);
}

static void
update_job_queue_init (UpdateJobQueue *self)
{
	gint max_jobs;
	conf_get_int_value (MAX_UPDATE_THREADS, &max_jobs);
	queue = self;
	queue->normalPool	= g_thread_pool_new (update_job_queue_run, (gpointer)update_job_execute,        max_jobs, FALSE, NULL);
	queue->priorityPool	= g_thread_pool_new (update_job_queue_run, (gpointer)update_job_execute,        max_jobs, FALSE, NULL);
	queue->resultPool	= g_thread_pool_new (update_job_queue_run, (gpointer)update_job_process_result, max_jobs, FALSE, NULL);
}

void
update_job_queue_add (gpointer data, updateFlags flags)
{
	UpdateJob *job = (UpdateJob *)data;

	if (!queue)
		return;

	// flow jobs are re-added to the queue for each job, let's count them once only
	if (NULL == g_slist_find (queue->jobs, job)) {
		queue->jobs = g_slist_append (queue->jobs, job);
		
		// Count all subscription jobs (but ignore HTML5, favicon and other download requests)
		if (!(job->flags & UPDATE_REQUEST_NO_FEED)) {
			queue->currentJobCount++;
			queue->maxCount++;
			g_signal_emit_by_name (queue, "update-running");
		}
	}

	g_assert (job->state == JOB_STATE_PENDING);

	if (flags & UPDATE_REQUEST_PRIORITY_HIGH)
		g_thread_pool_push (queue->priorityPool, data, NULL);
	else
		g_thread_pool_push (queue->normalPool, data, NULL);
}

void
update_job_queue_finish (gpointer data)
{
	UpdateJob *job = (UpdateJob *)data;

	if (!queue)
		return;

	g_assert (job->state == JOB_STATE_FINISHED || job->state == JOB_STATE_FAILED);
	g_thread_pool_push (queue->resultPool, (gpointer)job, NULL);
}

void
update_job_cancel_by_owner (gpointer owner)
{
	if (!queue)
		return;

	GSList	*iter = queue->jobs;
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
	if (!queue)
		return;

	if (!g_slist_find (queue->jobs, job)) {
		debug (DEBUG_UPDATE, "update_job_queue_remove: BAD job %p not found in queue", job);
		return;
	}
	queue->jobs = g_slist_remove (queue->jobs, job);

	// Count all subscription jobs (but ignore HTML5, favicon and other download requests)
	if (!(((UpdateJob *)job)->flags & UPDATE_REQUEST_NO_FEED)) {
		if (queue->currentJobCount > 0)
			queue->currentJobCount--;
		g_signal_emit_by_name (queue, "update-running");
	}
}

void
update_job_queue_get_count (guint *count, guint *max)
{
	guint normal, prio, result;
	guint normalRunning, prioRunning, resultRunning;

	if (!queue) {
		*count = 0;
		*max = 0;
		return;
	}

	normal = g_thread_pool_unprocessed (queue->normalPool);
	prio = g_thread_pool_unprocessed (queue->priorityPool);
	result = g_thread_pool_unprocessed (queue->resultPool);
	normalRunning = g_thread_pool_get_num_threads (queue->normalPool);
	prioRunning = g_thread_pool_get_num_threads (queue->priorityPool);
	resultRunning = g_thread_pool_get_num_threads (queue->resultPool);

	debug (DEBUG_UPDATE, "update job queue thread pools unprocessed: normal=%d / prio=%d / result=%d , running: normal=%d / prio=%d / result=%d",
	       normal, prio, result,
	       normalRunning, prioRunning, resultRunning);

	if (g_slist_length(queue->jobs) == 0) // correct miscounting
		queue->currentJobCount = 0;

	*count = queue->currentJobCount;

	if (*count > queue->maxCount)
		queue->maxCount = *count;
	else if (*count == 0)
		queue->maxCount = 0; // reset max when no jobs are running

	*max = queue->maxCount;
}

void
update_job_queue_to_json (gpointer builder)
{
	JsonBuilder *b = JSON_BUILDER (builder);

	if (!queue)
		return;

	json_builder_set_member_name (b, "queues");
	json_builder_begin_array (b);

	json_builder_begin_object (b);
	json_builder_set_member_name (b, "name");
	json_builder_add_string_value (b, "high priority requests");
	json_builder_set_member_name (b, "pending");
	json_builder_add_int_value (b, g_thread_pool_unprocessed (queue->priorityPool));
	json_builder_set_member_name (b, "running");
	json_builder_add_int_value (b, g_thread_pool_get_num_threads (queue->priorityPool));
	json_builder_set_member_name (b, "max");
	json_builder_add_int_value (b, g_thread_pool_get_max_threads (queue->priorityPool));
	json_builder_end_object (b);

	json_builder_begin_object (b);
	json_builder_set_member_name (b, "name");
	json_builder_add_string_value (b, "normal requests");
	json_builder_set_member_name (b, "pending");
	json_builder_add_int_value (b, g_thread_pool_unprocessed (queue->normalPool));
	json_builder_set_member_name (b, "running");
	json_builder_add_int_value (b, g_thread_pool_get_num_threads (queue->normalPool));
	json_builder_set_member_name (b, "max");
	json_builder_add_int_value (b, g_thread_pool_get_max_threads (queue->normalPool));
	json_builder_end_object (b);
	
	json_builder_begin_object (b);
	json_builder_set_member_name (b, "name");
	json_builder_add_string_value (b, "result processing");
	json_builder_set_member_name (b, "pending");
	json_builder_add_int_value (b, g_thread_pool_unprocessed (queue->resultPool));
	json_builder_set_member_name (b, "running");
	json_builder_add_int_value (b, g_thread_pool_get_num_threads (queue->resultPool));
	json_builder_set_member_name (b, "max");
	json_builder_add_int_value (b, g_thread_pool_get_max_threads (queue->resultPool));
	json_builder_end_object (b);
	json_builder_end_array (b);

	json_builder_set_member_name (b, "jobs");
	json_builder_begin_array (b);
	GSList *iter = queue->jobs;
	while (iter) {
		UpdateJob *job = (UpdateJob *)iter->data;
		json_builder_begin_object (b);
		json_builder_set_member_name (b, "source");
		json_builder_add_string_value (b, job->request->source);
		json_builder_set_member_name (b, "state");
		json_builder_add_int_value (b, (gint64)job->state);
		json_builder_set_member_name (b, "flags");
		json_builder_add_int_value (b, (gint64)job->flags);
		json_builder_end_object (b);
		iter = g_slist_next (iter);	
	}
	json_builder_end_array (b);
}

UpdateJobQueue *update_job_queue_get_instance (void)
{
	if (!queue)
		queue = g_object_new (UPDATE_JOB_QUEUE_TYPE, NULL);
	return queue;
}