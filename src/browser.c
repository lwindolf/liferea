/**
 * @file browser.c  Launching different external browsers
 *
 * Copyright (C) 2003-2015 Lars Windolf <lars.windolf@gmx.de>
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

#include "browser.h"

#include <string.h>

#include "common.h"
#include "conf.h"
#include "debug.h"
#include "ui/liferea_shell.h"

/**
 * Returns a shell command format string which can be used to create
 * a browser launch command if preference for manual command is set.
 *
 * @returns a newly allocated command string (or NULL)
 */
static gchar *
browser_get_manual_command (void)
{
	gchar	*cmd = NULL;
	gchar	*libname;

	/* check for manual browser command */
	conf_get_str_value (BROWSER_ID, &libname);
	if (g_str_equal (libname, "manual")) {
		/* retrieve user defined command... */
		conf_get_str_value (BROWSER_COMMAND, &cmd);
	}
	g_free (libname);

	return cmd;
}

static gboolean
browser_execute (const gchar *cmd, const gchar *uri)
{
	GError		*error = NULL;
	gchar 		*safeUri, *tmp, **argv, **iter;
	gint 		argc;
	gboolean	done = FALSE;

	g_assert (cmd != NULL);
	g_assert (uri != NULL);

	safeUri = (gchar *)common_uri_sanitize ((xmlChar *)uri);

	/* If we run using a Mozilla like "-remote openURL()" mechanism we
	   need to escape commata, but not in other cases (see SF #2901447) */
	if (strstr(cmd, "openURL("))
		safeUri = common_strreplace (safeUri, ",", "%2C");

	/* If there is no %s in the command, then just append %s */
	if (strstr (cmd, "%s"))
		tmp = g_strdup (cmd);
	else
		tmp = g_strdup_printf ("%s %%s", cmd);

	/* Parse and substitute the %s in the command */
	g_shell_parse_argv (tmp, &argc, &argv, &error);
	g_free (tmp);
	if (error && (0 != error->code)) {
		liferea_shell_set_important_status_bar (_("Browser command failed: %s"), error->message);
		debug2 (DEBUG_GUI, "Browser command is invalid: %s : %s", tmp, error->message);
		g_error_free (error);
		return FALSE;
	}

	if (argv) {
		for (iter = argv; *iter != NULL; iter++)
			*iter = common_strreplace (*iter, "%s", safeUri);
	}

	tmp = g_strjoinv (" ", argv);
	debug1 (DEBUG_GUI, "Running the browser-remote %s command", tmp);
	g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);

	if (error && (0 != error->code)) {
		debug2 (DEBUG_GUI, "Browser command failed: %s : %s", tmp, error->message);
		liferea_shell_set_important_status_bar (_("Browser command failed: %s"), error->message);
		g_error_free (error);
	} else {
		liferea_shell_set_status_bar (_("Starting: \"%s\""), tmp);
		done = TRUE;
	}

	g_free (safeUri);
	g_free (tmp);
	g_strfreev (argv);

	return done;
}

gboolean
browser_launch_URL_external (const gchar *uri)
{
	gchar		*cmd = NULL;
	gboolean	done = FALSE;

	g_assert (uri != NULL);

	cmd = browser_get_manual_command ();
	if (cmd) {
		done = browser_execute (cmd, uri);
		g_free (cmd);
	} else {
		done = gtk_show_uri_on_window (GTK_WINDOW (liferea_shell_get_window ()), uri, GDK_CURRENT_TIME, NULL);
	}

	return done;
}
