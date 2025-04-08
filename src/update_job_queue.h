/**
 * @file update_job_queue.h  handling async concurrent update processing
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

#ifndef _UPDATE_JOB_QUEUE_H
#define _UPDATE_JOB_QUEUE_H

#include "update.h"
#include "update_job.h"

/**
 * update_job_queue_add:
 * @job:	the job to queue
 * @flags:	request/result processing flags
 * 
 * Queues the given job. The job might be delayed if other requests are pending.
 */
void update_job_queue_add (gpointer job, updateFlags flags);

/**
 * update_job_queue_finished: (skip)
 * 
 * To be called by update_process_finished_job() when an update job has finished 
 */
void update_job_queue_finished (void);

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
 * update_init: (skip)
 * 
 * Initialises the download subsystem.
 *
 * Must be called before gtk_init() and after thread initialization
 * as threads are used and for proper network-manager initialization.
 */
void update_init (void);

/**
 * update_deinit: (skip)
 * 
 * Stops all update processing and frees all used memory.
 */
void update_deinit (void);

#endif