/*
 * @file update_job_queue.h  handling async concurrent update processing
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

#ifndef _UPDATE_JOB_QUEUE_H
#define _UPDATE_JOB_QUEUE_H

#include "update_job.h"

#define UPDATE_JOB_QUEUE_TYPE (update_job_queue_get_type ())
G_DECLARE_FINAL_TYPE (UpdateJobQueue, update_job_queue, UPDATE, JOB_QUEUE, GObject)

struct _UpdateJobQueue {
	GObject parent_instance;

	GSList	*jobs;

	guint	currentJobCount;	// actual number of pending / processing jobs
	guint	maxCount;		// previous max number of jobs (gets reset when currentJobCount = 0)

	GThreadPool *normalPool;	// thread pool for normal priority request processing
	GThreadPool *priorityPool;	// thread pool for high priority request processing
	GThreadPool *resultPool;	// thread pool for result post-processing (needed as we support blocking filter scripts)
};

/**
 * update_job_queue_add:
 * @job:	the job to queue
 * @flags:	request/result processing flags
 * 
 * Queues the given job. The job might be delayed if other requests are pending.
 */
void update_job_queue_add (gpointer job, updateFlags flags);

/**
 * update_job_queue_finish:
 * @job:	the job to finish
  * 
 * Queues the given job for result processing. The job might be delayed if other requests are pending.
 */
void update_job_queue_finish (gpointer job);

/**
 * update_job_cancel_by_owner: (skip)
 * @owner:	pointer passed in update_request_new()
 * 
 * Cancel all pending requests for the given owner.
 */
void update_job_cancel_by_owner (gpointer owner);

/**
 * update_job_queue_remove: (skip)
 * @job:	the update job
 * 
 * Removes the given job from the job queue. To be used when deleting jobs.
 */
void update_job_queue_remove (gpointer job);

/**
* update_job_queue_get_count: (skip)
* @count:	gint ref to pass back nr of subscriptions in update
* @maxcount:	gint ref to pass back max nr of subscriptions in update
*
* Query current count and max count of subscriptions in update queue
*/
void update_job_queue_get_count (guint *count, guint *maxcount);

/**
 * update_job_queue_get_instance: (skip)
 * 
 * Gets the singleton instance of the update job queue.

 * Must be called before gtk_init() and after thread initialization
 * as threads are used and for proper network-manager initialization.
 */
UpdateJobQueue *update_job_queue_get_instance (void);

/**
 * update_job_queue_to_json:
 * @b:  a JsonBuilder to append to
 */
void update_job_queue_to_json (gpointer b);

#endif
