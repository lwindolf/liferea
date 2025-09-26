/**
 * @file update_job.c  handling update processing (network/local/filter/XSLT)
 *
 * Copyright (C) 2003-2025 Lars Windolf <lars.windolf@gmx.de>
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

#include "update_job.h"

#include <libxml/parser.h>
#include <libxslt/xslt.h>
#include <libxslt/xsltInternals.h>
#include <libxslt/transform.h>
#include <libxslt/xsltutils.h>

#include <unistd.h>
#include <stdio.h>
#if !defined (G_OS_WIN32) || defined (HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif
#include <string.h>

#include "common.h"
#include "debug.h"
#include "net.h"
#include "update.h"
#include "xml.h"
#include "parsers/gopher.h"
#include "ui/liferea_shell.h"

#if defined (G_OS_WIN32) && !defined (WIFEXITED) && !defined (WEXITSTATUS)
#define WIFEXITED(x) (x != 0)
#define WEXITSTATUS(x) (x)
#endif

/* filter idea (and some of the code) was taken from Snownews */
static gchar *
update_exec_filter_cmd (UpdateJob *job)
{
	int		fd, status;
	gchar		*command;
	const gchar	*tmpdir = g_get_tmp_dir();
	char		*tmpfilename;
	char		*out = NULL;
	FILE		*file, *p;
	size_t		size = 0;

	tmpfilename = g_build_filename (tmpdir, "liferea-XXXXXX", NULL);

	fd = g_mkstemp (tmpfilename);

	if (fd == -1) {
		debug (DEBUG_UPDATE, "Error opening temp file %s to use for filtering!", tmpfilename);
		job->result->filterErrors = g_strdup_printf (_("Error opening temp file %s to use for filtering!"), tmpfilename);
		g_free (tmpfilename);
		return NULL;
	}

	if((file = fdopen (fd, "w"))) {
		fwrite (job->result->data, strlen (job->result->data), 1, file);
		fclose (file);

		command = g_strdup_printf("%s < %s", job->request->filtercmd, tmpfilename);
		p = popen (command, "r");
		if (NULL != p) {
			while (!feof (p) && !ferror (p)) {
				size_t len;
				out = g_realloc (out, size + 1025);
				len = fread (&out[size], 1, 1024, p);
				if (len > 0)
					size += len;
			}
			status = pclose (p);
			if (!(WIFEXITED (status) && WEXITSTATUS (status) == 0)) {
				debug (DEBUG_UPDATE, "%s exited with status %d!", command, WEXITSTATUS(status));
				job->result->filterErrors = g_strdup_printf (_("%s exited with status %d"), command, WEXITSTATUS(status));
				size = 0;
			}
			if (out)
				out[size] = '\0';
		} else {
			job->result->filterErrors = g_strdup_printf (_("Error: Could not open pipe \"%s\""), command);
		}
		g_free (command);
	} else {
		job->result->filterErrors = g_strdup (_("Error: Could not write temporary file!"));
	}

	/* Clean up. */
	unlink (tmpfilename);
	g_free (tmpfilename);
	return out;
}

static gchar *
update_apply_xslt (UpdateJob *job)
{
	xsltStylesheetPtr	xslt = NULL;
	xmlOutputBufferPtr	buf;
	xmlDocPtr		srcDoc = NULL, resDoc = NULL;
	gchar			*output = NULL;

	g_assert (NULL != job->result);

	do {
		srcDoc = xml_parse (job->result->data, job->result->size, NULL);
		if (!srcDoc) {
			g_warning("fatal: parsing request result XML source failed (%s)!", job->request->filtercmd);
			break;
		}

		/* load localization stylesheet */
		xslt = xsltParseStylesheetFile ((xmlChar *)job->request->filtercmd);
		if (!xslt) {
			g_warning ("fatal: could not load filter stylesheet \"%s\"!", job->request->filtercmd);
			break;
		}

		resDoc = xsltApplyStylesheet (xslt, srcDoc, NULL);
		if (!resDoc) {
			g_warning ("fatal: applying stylesheet \"%s\" failed!", job->request->filtercmd);
			break;
		}

		buf = xmlAllocOutputBuffer (NULL);
		if (-1 == xsltSaveResultTo (buf, resDoc, xslt)) {
			g_warning ("fatal: retrieving result of filter stylesheet failed (%s)!", job->request->filtercmd);
			break;
		}

#ifdef LIBXML2_NEW_BUFFER
		if (xmlOutputBufferGetSize (buf) > 0)
			output = (gchar *)xmlCharStrdup ((char *)xmlOutputBufferGetContent (buf));
#else
		if (xmlBufferLength (buf->buffer) > 0)
			output = (gchar *)xmlCharStrdup ((char *)xmlBufferContent (buf->buffer));
#endif

		xmlOutputBufferClose (buf);
	} while (FALSE);

	if (srcDoc)
		xmlFreeDoc (srcDoc);
	if (resDoc)
		xmlFreeDoc (resDoc);
	if (xslt)
		xsltFreeStylesheet (xslt);

	return output;
}

static void
update_apply_filter (UpdateJob *job)
{
	gchar	*filterResult;

	g_assert (NULL == job->result->filterErrors);

	/* we allow two types of filters: XSLT stylesheets and arbitrary commands */
	if ((strlen (job->request->filtercmd) > 4) &&
	    (0 == strcmp (".xsl", job->request->filtercmd + strlen (job->request->filtercmd) - 4)))
		filterResult = update_apply_xslt (job);
	else
		filterResult = update_exec_filter_cmd (job);

	if (filterResult) {
		g_free (job->result->data);
		job->result->data = filterResult;
		job->result->size = strlen(filterResult);
	}
}

static void
update_exec_cmd_cb_child_watch (GPid pid, gint status, gpointer user_data)
{
	UpdateJob *	job = (UpdateJob *) user_data;
	debug (DEBUG_UPDATE, "Child process %d terminated", job->cmd.pid);

	job->cmd.pid = 0;
	if (WIFEXITED (status) && WEXITSTATUS (status) == 0) {
		job->result->httpstatus = 200;
	} else if (job->result->httpstatus == 0) {
		/* If there is no more specific error code. */
		job->result->httpstatus = 500;  /* Internal server error. */
	}

	job->cmd.child_watch_id = 0;	/* Caller will remove source. */
	if (job->cmd.timeout_id > 0) {
		g_source_remove (job->cmd.timeout_id);
		job->cmd.timeout_id = 0;
	}
	if (job->cmd.io_watch_id > 0) {
		g_source_remove (job->cmd.io_watch_id);
		job->cmd.io_watch_id = 0;
	}
	update_job_finished (job);
}

static gboolean
update_exec_cmd_cb_out_watch (GIOChannel *source, GIOCondition condition, gpointer user_data)
{
	UpdateJob *	job = (UpdateJob *) user_data;
	GError		*err = NULL;
	gboolean	ret = TRUE;	/* Do not remove event source yet. */
	GIOStatus	st;
	gsize		nread;

	if (condition == G_IO_HUP) {
		debug (DEBUG_UPDATE, "Pipe closed, child process %d is terminating", job->cmd.pid);
		ret = FALSE;

	} else if (condition == G_IO_IN) {
		while (TRUE) {
			job->result->data = g_realloc (job->result->data, job->result->size + 1025);

			nread = 0;
			st = g_io_channel_read_chars (source,
				job->result->data + job->result->size,
				1024, &nread, &err);
			job->result->size += nread;
			job->result->data[job->result->size] = 0;

			if (err) {
				debug (DEBUG_UPDATE, "Error %d when reading from child %d", err->code, job->cmd.pid);
				g_error_free (err);
				err = NULL;
				ret = FALSE;	/* remove event */
			}

			if (nread == 0) {
				/* Finished reading */
				break;
			} else if (st == G_IO_STATUS_AGAIN) {
				/* just try again */
			} else if (st == G_IO_STATUS_EOF) {
				/* Pipe closed */
				ret = FALSE;
				break;
			} else if (st == G_IO_STATUS_ERROR) {
				debug (DEBUG_UPDATE, "Got a G_IO_STATUS_ERROR from child %d", job->cmd.pid);
				ret = FALSE;
				break;
			}
		}

	} else {
		debug (DEBUG_UPDATE, "Unexpected condition %d for child process %d", condition, job->cmd.pid);
		ret = FALSE;
	}

	if (ret == FALSE) {
		close (job->cmd.fd);
		job->cmd.fd = -1;
		job->cmd.io_watch_id = 0;	/* Caller will remove source. */
	}

	return ret;
}

static gboolean
update_exec_cmd_cb_timeout (gpointer user_data)
{
	UpdateJob *	job = (UpdateJob *) user_data;
	debug (DEBUG_UPDATE, "Child process %d timed out, killing.", job->cmd.pid);

	/* Kill child. Result will still be processed by update_exec_cmd_cb_child_watch */
	kill((pid_t) job->cmd.pid, SIGKILL);
	job->cmd.timeout_id = 0;
	job->result->httpstatus = 504;	/* Gateway timeout */
	return FALSE;	/* Remove timeout source */
}

static int
get_exec_timeout_ms(void)
{
	const gchar	*val;
	int	i;
	if ((val = g_getenv("LIFEREA_FEED_CMD_TIMEOUT")) != NULL) {
		if ((i = atoi(val)) > 0) {
			return 1000*i;
		}
	}
	return 60000; /* Default timeout */
}

static void
update_exec_cmd (UpdateJob *job)
{
	gboolean	ret;
	gchar		*cmd = (job->request->source) + 1;

	/* Previous versions ran through popen() and a lot of users may be depending
	 * on this behavior, so we run through a shell and keep compatibility. */
	gchar		*cmd_args[] = { "/bin/sh", "-c", cmd, NULL };

	job->result->httpstatus = 0;
	debug (DEBUG_UPDATE, "executing command \"%s\"...", cmd);
	ret = g_spawn_async_with_pipes (NULL, cmd_args, NULL,
		G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_STDERR_TO_DEV_NULL,
		NULL, NULL, &job->cmd.pid, NULL,
		&job->cmd.fd, NULL, NULL);

	if (!ret) {
		debug (DEBUG_UPDATE, "g_spawn_async_with_pipes failed");
		liferea_shell_set_status_bar (_("Error: Could not open pipe \"%s\""), cmd);
		job->result->httpstatus = 404; /* Not found */
		return;
	}

	debug (DEBUG_UPDATE, "New child process launched with pid %d", job->cmd.pid);

	job->cmd.child_watch_id = g_child_watch_add (job->cmd.pid, (GChildWatchFunc) update_exec_cmd_cb_child_watch, job);
	job->cmd.stdout_ch = g_io_channel_unix_new (job->cmd.fd);
	job->cmd.io_watch_id = g_io_add_watch (job->cmd.stdout_ch, G_IO_IN | G_IO_HUP, (GIOFunc) update_exec_cmd_cb_out_watch, job);

	job->cmd.timeout_id = g_timeout_add (get_exec_timeout_ms(), (GSourceFunc) update_exec_cmd_cb_timeout, job);
}

static void
update_load_file (UpdateJob *job)
{
	gchar *filename = job->request->source;
	gchar *anchor;

	if (!strncmp (filename, "file://",7))
		filename += 7;

	anchor = strchr (filename, '#');
	if (anchor)
		*anchor = 0;	 /* strip anchors from filenames */

	if (g_file_test (filename, G_FILE_TEST_EXISTS)) {
		/* we have a file... */
		if ((!g_file_get_contents (filename, &(job->result->data), &(job->result->size), NULL)) || (job->result->data[0] == '\0')) {
			job->result->httpstatus = 403;	/* FIXME: maybe setting request->returncode would be better */
			liferea_shell_set_status_bar (_("Error: Could not open file \"%s\""), filename);
		} else {
			job->result->httpstatus = 200;
			debug (DEBUG_UPDATE, "Successfully read %d bytes from file %s.", job->result->size, filename);
		}
	} else {
		liferea_shell_set_status_bar (_("Error: There is no file \"%s\""), filename);
		job->result->httpstatus = 404;	/* FIXME: maybe setting request->returncode would be better */
	}

	update_job_finished (job);
}

G_DEFINE_TYPE (UpdateJob, update_job, G_TYPE_OBJECT)

static void
update_job_finalize (GObject *object)
{
        UpdateJob *job = UPDATE_JOB (object);
	if (!job)
		return;

        update_job_queue_remove (job);

	g_object_unref (job->request);
	g_object_unref (job->result);

	if (job->cmd.fd >= 0) {
		debug (DEBUG_UPDATE, "Found an open cmd.fd %d when freeing!", job->cmd.fd);
		close (job->cmd.fd);
	}
	if (job->cmd.timeout_id > 0) {
		g_source_remove (job->cmd.timeout_id);
	}
	if (job->cmd.io_watch_id > 0) {
		g_source_remove (job->cmd.io_watch_id);
	}
	if (job->cmd.child_watch_id > 0) {
		g_source_remove (job->cmd.child_watch_id);
	}
	if (job->cmd.stdout_ch) {
		g_io_channel_unref (job->cmd.stdout_ch);
	}

	G_OBJECT_CLASS(update_job_parent_class)->finalize(object);
}

static void update_job_class_init (UpdateJobClass *klass) {
	GObjectClass *object_class = G_OBJECT_CLASS(klass);
	object_class->finalize = (GObjectFinalizeFunc) update_job_finalize;
}

static void update_job_init (UpdateJob *self) {
	/* Add any instance initialization code here */
}

// dequeue job and execute it according to its job type (download, command, local file)
void
update_job_execute (UpdateJob *job)
{
	/* Here we decide on the source type and the proper execution
	   methods which then do anything they want with the job and
	   pass the processed job to update_job_finished()
	   for result dequeuing */

	/* everything starting with '|' is a local command */
	if (*(job->request->source) == '|') {
		if (job->request->allowCommands) {
			debug (DEBUG_UPDATE, "Recognized local command: %s", job->request->source);
			update_exec_cmd (job);
		} else {
			debug (DEBUG_UPDATE, "Refusing to run local command from unexpected source: %s", job->request->source);
			job->result->httpstatus = 403;  /* Forbidden. */
			update_job_finished (job);
		}
		return;
	}

	if (strncmp (job->request->source, "file://", 7) == 0) {
		debug (DEBUG_UPDATE, "Recognized file URI: %s", job->request->source);
		update_load_file (job);
		return;
	}

	if (strncmp (job->request->source, "gopher://", 9) == 0) {
		debug (DEBUG_UPDATE, "Recognized gopher URI: %s", job->request->source);
		gopher_process_request (job);
		return;
	}

	/* if it has a protocol "://" prefix, but not "file://" it is an URI */
	if (strstr (job->request->source, "://")) {
		network_process_request (job);
		return;
	}

	g_warning ("Invalid protocol requested with URI \"%s\"!", job->request->source);
}

UpdateJob *
update_job_new (gpointer owner,
                UpdateRequest *request,
		update_result_cb callback,
		gpointer user_data,
		updateFlags flags)
{
	UpdateJob *job = UPDATE_JOB (g_object_new (UPDATE_JOB_TYPE, NULL));

	g_assert (request->options != NULL);
	g_assert (request->source != NULL);

	job->owner = owner;
	job->request = UPDATE_REQUEST (request);
	job->result = update_result_new ();
	job->callback = callback;
	job->user_data = user_data;
	job->flags = flags;
	job->state = JOB_STATE_PENDING;

	job->cmd.fd = -1;
	job->cmd.pid = 0;

        update_job_queue_add ((gpointer)job, flags);
	return job;
}

gint
update_job_get_state (UpdateJob *job)
{
	return job->state;
}

static gboolean
update_process_result_idle_cb (gpointer user_data)
{
	UpdateJob *job = (UpdateJob *)user_data;

	job->state = JOB_STATE_FINISHED;
        
	if (job->callback)
		(job->callback) (job->result, job->user_data, job->flags);

	g_object_unref (job);

	return FALSE;
}

static void
update_apply_filter_async(GTask *task, gpointer src, gpointer tdata, GCancellable *ccan)
{
    update_apply_filter (UPDATE_JOB (tdata));
    g_task_return_int (task, 0);
}

static void
update_apply_filter_finish(GObject *src, GAsyncResult *result, gpointer user_data)
{
    g_idle_add (update_process_result_idle_cb, user_data);
}

void
update_job_finished (UpdateJob *job)
{
	/* Handling abandoned requests (e.g. after feed deletion) */
	if (job->callback == NULL) {
		debug (DEBUG_UPDATE, "freeing cancelled request (%s)", job->request->source);
		g_object_unref (job);
		return;
	}

	/* Finally execute the postfilter */
	if (job->result->data && job->request->filtercmd) {
                GTask *task = g_task_new(NULL, NULL, update_apply_filter_finish, job);
                g_task_set_task_data(task, job, NULL);
                g_task_run_in_thread(task, update_apply_filter_async);
                g_object_unref(task);
                return;
        }

	g_idle_add (update_process_result_idle_cb, job);
}