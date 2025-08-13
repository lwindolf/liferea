/**
 * @file update_job.h  handling update processing (network/local/filter/XSLT)
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

#ifndef _UPDATE_JOB_H
#define _UPDATE_JOB_H

#include <glib.h>
#include <glib-object.h>

#include "update_job.h"

#include "update.h"

// FIXME: naming should not be JOB_STATE_... but JOB_STATE_...
typedef enum {
	JOB_STATE_PENDING = 0,	/*<< request added to download queue */
	JOB_STATE_PROCESSING,	/*<< request currently downloading */
	JOB_STATE_FINISHED		/*<< request processing finished */
} request_state;

G_BEGIN_DECLS

#define UPDATE_JOB_TYPE (update_job_get_type ())
G_DECLARE_FINAL_TYPE (UpdateJob, update_job, UPDATE, JOB, GObject)

struct _UpdateJob {
	GObject parent_instance;

	UpdateRequest		*request;
	UpdateResult		*result;
	gpointer		owner;		/*<< owner of this job (used for matching when cancelling) */
	update_result_cb	callback;	/*<< result processing callback */
	gpointer		user_data;	/*<< result processing user data */
	updateFlags		flags;		/*<< request and result processing flags */
	gint			state;		/*<< State of the job (enum request_state) */
	updateCommandState	cmd;		/*<< values for command feeds */
};

/**
 * update_job_new:
 * @owner: (nullable):		request owner (allows cancelling)
 * @request:			the request to execute
 * @callback: (scope forever)	result processing callback
 * @user_data: (nullable):	result processing callback parameters
 * @flags:			request/result processing flags
 * 
 * Executes the given request. The request might be
 * delayed if other requests are pending.
 *
 * Returns: a new update job
 */
UpdateJob * update_job_new (gpointer owner,
                            UpdateRequest *request,
                            update_result_cb callback,
                            gpointer user_data,
                            updateFlags flags);

/**
 * update_job_execute: (skip)
 * @job:	the update job
 * 
 * Called by job queue when a job is to be actually processed. 
 */
void update_job_execute (UpdateJob *job);

/**
 * update_job_finished:
 * @job:	the update job
 * 
 * To be called when an update job has been executed. Triggers
 * the job specific result processing callback.
 */
void update_job_finished (UpdateJob *job);

/**
 * update_job_get_state:
 * @returns update job state (see enum request_state)
 * 
 * Method to query the update state of currently processed jobs.
 * 
 * Returns: enum state
 */
gint update_job_get_state (UpdateJob *job);

G_END_DECLS

#endif