/** 
 * @file ui_session.c session management
 *
 * session management for Gaim (from revision 1.16 of gaim's CVS tree)
 *
 * Gaim is the legal property of its developers, whose names are too
 * numerous to list here.  Please refer to the COPYRIGHT file
 * distributed with the gaim source code.
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
 *
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

#ifdef USE_SM

#include <X11/ICE/ICElib.h>
#include <X11/SM/SMlib.h>
#include <gdk/gdkx.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "debug.h"
#include "ui/ui_mainwindow.h"
#include "ui/ui_shell.h"

#define GAIM_DEBUG_ALL 0
#define GAIM_DEBUG_INFO 1
#define GAIM_DEBUG_WARNING 2
#define GAIM_DEBUG_MISC 3
#define GAIM_DEBUG_ERROR 4

void gaim_debug_vargs(int level, const char *category,
			  const char *format, va_list args)
{
	gchar *str;
	g_return_if_fail(format != NULL);
	str = g_strdup_vprintf(format, args);
	debug2(DEBUG_GUI, "%s: %s", category, str);
	g_free(str);
}


static void
gaim_debug(int level, const char *category,
		 const char *format, ...)
{
	va_list args;

	g_return_if_fail(format != NULL);

	va_start(args, format);
	gaim_debug_vargs(level, category, format, args);
	va_end(args);
}

#define GAIM_GTK_READ_COND  (G_IO_IN | G_IO_HUP | G_IO_ERR)
#define GAIM_GTK_WRITE_COND (G_IO_OUT | G_IO_HUP | G_IO_ERR | G_IO_NVAL)

#define GAIM_READ_COND 1
#define GAIM_WRITE_COND 2

typedef enum
	{
		GAIM_INPUT_READ  = 1 << 0,  /**< A read condition.  */
		GAIM_INPUT_WRITE = 1 << 1   /**< A write condition. */
		
	} GaimInputCondition;
typedef void (*GaimInputFunction)(gpointer, gint, GaimInputCondition);

typedef struct _GaimGtkIOClosure {
	GaimInputFunction function;
	guint result;
	gpointer data;

} GaimGtkIOClosure;

static gboolean gaim_gtk_io_invoke(GIOChannel *source, GIOCondition condition, gpointer data)
{
	GaimGtkIOClosure *closure = data;
	GaimInputCondition gaim_cond = 0;

	if (condition & GAIM_GTK_READ_COND)
		gaim_cond |= GAIM_INPUT_READ;
	if (condition & GAIM_GTK_WRITE_COND)
		gaim_cond |= GAIM_INPUT_WRITE;

	closure->function(closure->data, g_io_channel_unix_get_fd(source), gaim_cond);

	return TRUE;
}

static guint gaim_input_add(gint fd, GaimInputCondition condition, GaimInputFunction function,
					   gpointer data) {
	GaimGtkIOClosure *closure = g_new0(GaimGtkIOClosure, 1);
	GIOChannel *channel;
	GIOCondition cond = 0;

	closure->function = function;
	closure->data = data;

	if (condition & GAIM_INPUT_READ)
		cond |= GAIM_GTK_READ_COND;
	if (condition & GAIM_INPUT_WRITE)
		cond |= GAIM_GTK_WRITE_COND;

	channel = g_io_channel_unix_new(fd);
	closure->result = g_io_add_watch_full(channel, G_PRIORITY_DEFAULT, cond,
								   gaim_gtk_io_invoke, closure, g_free);
	
	g_io_channel_unref(channel);
	return closure->result;
}

static gboolean gaim_input_remove(guint tag) {
	return g_source_remove(tag);
}

#define ERROR_LENGTH 512

static IceIOErrorHandler ice_installed_io_error_handler;
static SmcConn session = NULL;
static gchar *myself = NULL;
static gchar *client_id = NULL;
static gboolean had_first_save = FALSE;
gboolean session_managed = FALSE;

/* ICE belt'n'braces stuff */

struct ice_connection_info {
	IceConn connection;
	guint input_id;
};

static void ice_process_messages(gpointer data, gint fd,
						   GaimInputCondition condition) {
	struct ice_connection_info *conninfo = (struct ice_connection_info*) data;
	IceProcessMessagesStatus status;

	/* please don't block... please! */
	status = IceProcessMessages(conninfo->connection, NULL, NULL);

	if (status == IceProcessMessagesIOError) {
		gaim_debug(GAIM_DEBUG_INFO, "Session Management",
				   "ICE IO error, closing connection... ");

		/* IO error, please disconnect */
		IceSetShutdownNegotiation(conninfo->connection, False);
		IceCloseConnection(conninfo->connection);

		gaim_debug(GAIM_DEBUG_INFO, "Session Management", "done.");

		/* cancel the handler */
		gaim_input_remove(conninfo->input_id);
	}
}

static void ice_connection_watch(IceConn connection, IcePointer client_data,
	      Bool opening, IcePointer *watch_data) {
	struct ice_connection_info *conninfo = NULL;

	if (opening) {
		gaim_debug(GAIM_DEBUG_INFO, "Session Management",
				   "Handling new ICE connection... ");

		/* ensure ICE connection is not passed to child processes */
		fcntl(IceConnectionNumber(connection), F_SETFD, FD_CLOEXEC);

		conninfo = g_new(struct ice_connection_info, 1);
		conninfo->connection = connection;

		/* watch the connection */
		conninfo->input_id = gaim_input_add(IceConnectionNumber(connection), GAIM_INPUT_READ,
											ice_process_messages, conninfo);
		*watch_data = conninfo;
	} else {
		gaim_debug(GAIM_DEBUG_INFO, "Session Management",
				   "Handling closed ICE connection... ");

		/* get the input ID back and stop watching it */
		conninfo = (struct ice_connection_info*) *watch_data;
		gaim_input_remove(conninfo->input_id);
		g_free(conninfo);
	}

	gaim_debug(GAIM_DEBUG_INFO, "Session Management", "done.");
}

/* We call any handler installed before (or after) ice_init but 
 * avoid calling the default libICE handler which does an exit().
 *
 * This means we do nothing by default, which is probably correct,
 * the connection will get closed by libICE
 */

static void ice_io_error_handler(IceConn connection) {
	gaim_debug(GAIM_DEBUG_INFO, "Session Management",
			   "Handling ICE IO error... ");

	if (ice_installed_io_error_handler)
		(*ice_installed_io_error_handler)(connection);

	gaim_debug(GAIM_DEBUG_INFO, "Session Management", "done.");
}

static void ice_init() {
	IceIOErrorHandler default_handler;

	ice_installed_io_error_handler = IceSetIOErrorHandler(NULL);
	default_handler = IceSetIOErrorHandler(ice_io_error_handler);

	if (ice_installed_io_error_handler == default_handler)
		ice_installed_io_error_handler = NULL;

	IceAddConnectionWatch(ice_connection_watch, NULL);

	gaim_debug(GAIM_DEBUG_INFO, "Session Management",
			   "ICE initialized.");
}

/* my magic utility function */

static gchar **session_make_command(gchar *client_id, gchar *config_dir, gint mainwindowState) {
	gint i = 2;
	gint j = 0;
	gchar **ret;
	
	if (client_id) i += 2;
	if (config_dir)	i += 2; /* we will specify gaim's user dir */
	if (TRUE) i += 1; /* Always specify the state */
	ret = g_new(gchar *, i);
	ret[j++] = g_strdup(myself);

	if (client_id) {
		ret[j++] = g_strdup("--session");
		ret[j++] = g_strdup(client_id);
	}

	if (config_dir) {
		ret[j++] = g_strdup("--config");
		ret[j++] = g_strdup(config_dir);
	}
	
	if (mainwindowState == MAINWINDOW_SHOWN)
		ret[j++] = g_strdup("--mainwindow-state=shown");
	else if (mainwindowState == MAINWINDOW_ICONIFIED)
		ret[j++] = g_strdup("--mainwindow-state=iconified");
	else if (mainwindowState == MAINWINDOW_HIDDEN)
		ret[j++] = g_strdup("--mainwindow-state=hidden");
	ret[j++] = NULL;
	
	return ret;
}

/* SM callback handlers */

void session_save_yourself(SmcConn conn, SmPointer data, int save_type,
       Bool shutdown, int interact_style, Bool fast) {
	if (had_first_save == FALSE && save_type == SmSaveLocal &&
	      interact_style == SmInteractStyleNone && !shutdown &&
	      !fast) {
		/* this is just a dry run, spit it back */
		gaim_debug(GAIM_DEBUG_INFO, "Session Management",
				   "Received first save_yourself");
		SmcSaveYourselfDone(conn, True);
		had_first_save = TRUE;
		return;
	}

	/* tum ti tum... don't add anything else here without *
         * reading SMlib.PS from an X.org ftp server near you */

	gaim_debug(GAIM_DEBUG_INFO, "Session Management",
			   "Received save_yourself");

	if (save_type == SmSaveGlobal || save_type == SmSaveBoth) {
		/* may as well do something ... */
		/* or not -- save_prefs(); */
	}

	SmcSaveYourselfDone(conn, True);
}

void session_die(SmcConn conn, SmPointer data) {
	gaim_debug(GAIM_DEBUG_INFO, "Session Management",
			   "Received die");
	g_idle_add (quit, NULL);
}

void session_save_complete(SmcConn conn, SmPointer data) {
	gaim_debug(GAIM_DEBUG_INFO, "Session Management",
			   "Received save_complete");
}

void session_shutdown_cancelled(SmcConn conn, SmPointer data) {
	gaim_debug(GAIM_DEBUG_INFO, "Session Management",
			   "Received shutdown_cancelled");
}

/* utility functions stolen from Gnome-client */

static void session_set_value(SmcConn conn, gchar *name, char *type,
	      int num_vals, SmPropValue *vals) {
	SmProp *proplist[1];
	SmProp prop;

	g_return_if_fail(conn);

	prop.name = name;
	prop.type = type;
	prop.num_vals = num_vals;
	prop.vals = vals;

	proplist[0] = &prop;
	SmcSetProperties(conn, 1, proplist);
}

static void session_set_string(SmcConn conn, gchar *name, gchar *value) {
	SmPropValue val;

	g_return_if_fail(name);

	val.length = strlen (value)+1;
	val.value  = value;

	session_set_value(conn, name, SmARRAY8, 1, &val);
}

static void session_set_gchar(SmcConn conn, gchar *name, gchar value) {
	SmPropValue val;

	g_return_if_fail(name);

	val.length = 1;
	val.value  = &value;

	session_set_value(conn, name, SmCARD8, 1, &val);
}

static void session_set_array(SmcConn conn, gchar *name, gchar *array[]) {
	gint    argc;
	gchar **ptr;
	gint    i;

	SmPropValue *vals;

	g_return_if_fail (name);

	/* We count the number of elements in our array.  */
	for (ptr = array, argc = 0; *ptr ; ptr++, argc++) /* LOOP */;

	/* Now initialize the 'vals' array.  */
	vals = g_new (SmPropValue, argc);
	for (ptr = array, i = 0 ; i < argc ; ptr++, i++) {
		vals[i].length = strlen (*ptr);
		vals[i].value  = *ptr;
	}

	session_set_value(conn, name, SmLISTofARRAY8, argc, vals);

	g_free (vals);
}

#endif /* USE_SM */

/* setup functions */

void session_set_cmd(gchar *config_dir, gint mainwindowState) {
#ifdef USE_SM
	gchar **cmd = NULL;
	
	if (client_id == NULL)
		return;
	
	cmd = session_make_command(NULL, config_dir, mainwindowState);
	session_set_array(session, SmCloneCommand, cmd);
	g_strfreev(cmd);
	
	cmd = session_make_command(client_id, config_dir, mainwindowState);
	session_set_array(session, SmRestartCommand, cmd);
	g_strfreev(cmd);
#endif
}

void session_init(gchar *argv0, gchar *previous_id) {
#ifdef USE_SM
	SmcCallbacks callbacks;
	gchar error[ERROR_LENGTH] = "";
	gchar *tmp = NULL;
	gchar **cmd = NULL;
	
	if (session != NULL) {
		/* session is already established, what the hell is going on? */
		gaim_debug(GAIM_DEBUG_WARNING, "Session Management",
				   "Duplicated call to session_init!");
		return;
	}

	if (g_getenv("SESSION_MANAGER") == NULL) {
		gaim_debug(GAIM_DEBUG_ERROR, "Session Management",
				   "No SESSION_MANAGER found, aborting.");
		return;
	}

	ice_init();

	callbacks.save_yourself.callback         = session_save_yourself;
	callbacks.die.callback                   = session_die;
	callbacks.save_complete.callback         = session_save_complete;
	callbacks.shutdown_cancelled.callback    = session_shutdown_cancelled;

	callbacks.save_yourself.client_data      = NULL;
	callbacks.die.client_data                = NULL;
	callbacks.save_complete.client_data      = NULL;
	callbacks.shutdown_cancelled.client_data = NULL;

	if (previous_id) {
		gaim_debug(GAIM_DEBUG_INFO, "Session Management",
				   "Connecting with previous ID %s", previous_id);
	} else {
		gaim_debug(GAIM_DEBUG_INFO, "Session Management",
				   "Connecting with no previous ID");
	}

	session = SmcOpenConnection(NULL, "session", SmProtoMajor, SmProtoMinor, SmcSaveYourselfProcMask |
		    SmcDieProcMask | SmcSaveCompleteProcMask | SmcShutdownCancelledProcMask,
		    &callbacks, previous_id, &client_id, ERROR_LENGTH, error);

	if (session == NULL) {
		if (error[0] != '\0') {
			gaim_debug(GAIM_DEBUG_ERROR, "Session Management",
					   "Connection failed with error: %s", error);
		} else {
			gaim_debug(GAIM_DEBUG_ERROR, "Session Management",
					   "Connetion failed with unknown error.");
		}
		return;
	}

	tmp = SmcVendor(session);
	gaim_debug(GAIM_DEBUG_INFO, "Session Management",
			   "Connected to manager (%s) with client ID %s",
			   tmp, client_id);
	g_free(tmp);

	session_managed = TRUE;
	gdk_set_sm_client_id(client_id);

	tmp = g_get_current_dir();
	session_set_string(session, SmCurrentDirectory, tmp);
	g_free(tmp);

	tmp = g_strdup_printf("%d", (int) getpid());
	session_set_string(session, SmProcessID, tmp);
	g_free(tmp);

	tmp = g_strdup(g_get_user_name());
	session_set_string(session, SmUserID, tmp);
	g_free(tmp);

	session_set_gchar(session, SmRestartStyleHint, (gchar) SmRestartIfRunning);
	session_set_string(session, SmProgram, g_get_prgname());

	myself = g_strdup(argv0);
	gaim_debug(GAIM_DEBUG_MISC, "Session Management",
			   "Using %s as command", myself);

	/* this is currently useless, but gnome-session warns 'the following applications will not
	   save their current status' bla bla if we don't have it and the user checks 'Save Session'
	   when they log out */
	cmd = g_new(gchar *, 2);
	cmd[0] = g_strdup("/bin/true");
	cmd[1] = NULL;
	session_set_array(session, SmDiscardCommand, cmd);
	g_strfreev(cmd);
	
#endif /* USE_SM */
}

void session_end() {
#ifdef USE_SM
	if (session == NULL) /* no session to close */
		return;

	SmcCloseConnection(session, 0, NULL);

	gaim_debug(GAIM_DEBUG_INFO, "Session Management",
			   "Connection closed.");
#endif /* USE_SM */
}
