/**
 * @file browser.h  Launching different external browsers
 *
 * Copyright (C) 2003-2008 Lars Lindner <lars.lindner@gmail.com>
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
#include "debug.h"
#include "ui/liferea_shell.h"
#include "ui/ui_prefs.h"

static gboolean
browser_execute (const gchar *cmd, const gchar *uri, gboolean remoteEscape, gboolean sync)
{
	GError		*error = NULL;
	gchar 		*tmpUri, *tmp, **argv, **iter;
	gint 		argc;
	gint		status = 0;
	gboolean 	done = FALSE;
  
	g_assert (cmd != NULL);
	g_assert (uri != NULL);

	/* If the command is using the X remote API we must
	   escaped all ',' in the URL */
	if (remoteEscape)
		tmpUri = common_strreplace (g_strdup (uri), ",", "%2C");
	else
		tmpUri = g_strdup (uri);

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
		debug2 (DEBUG_GUI, "Browser command failed: %s : %s", tmp, error->message);
		g_error_free (error);
		return FALSE;
	}
  
	if (argv) {
		for (iter = argv; *iter != NULL; iter++)
			*iter = common_strreplace (*iter, "%s", tmpUri);
	}

	tmp = g_strjoinv (" ", argv);
	debug2 (DEBUG_GUI, "Running the browser-remote %s command '%s'", sync ? "sync" : "async", tmp);
	if (sync)
		g_spawn_sync (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, NULL, &status, &error);
	else 
		g_spawn_async (NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &error);
  
	if (error && (0 != error->code)) {
		debug2 (DEBUG_GUI, "Browser command failed: %s : %s", tmp, error->message);
		liferea_shell_set_important_status_bar (_("Browser command failed: %s"), error->message);
		g_error_free (error);
	} else if (status == 0) {
		liferea_shell_set_status_bar (_("Starting: \"%s\""), tmp);
		done = TRUE;
	}
  
	g_free (tmpUri);
	g_free (tmp);
	g_strfreev (argv);
  
	return done;
}

gboolean
browser_launch_URL_external (const gchar *uri)
{
	struct browser	*browser;
	gchar		*cmd = NULL;
	gboolean	done = FALSE;	
	
	g_assert (uri != NULL);
	
	browser = prefs_get_browser ();
	if (browser) {
		/* try to execute synchronously... */
		cmd = prefs_get_browser_command (browser, TRUE /* remote */, FALSE /* fallback */);
		if (cmd) {
			done = browser_execute (cmd, uri, browser->escapeRemote, TRUE);
			g_free (cmd);
		}
	}
	
	if (done)
		return TRUE;
	
	/* if it failed try to execute asynchronously... */		
	cmd = prefs_get_browser_command (browser, FALSE /* remote */, TRUE /* fallback */);
	if (!cmd) {
		liferea_shell_set_important_status_bar ("Fatal: cannot retrieve browser command!");
		g_warning ("Fatal: cannot retrieve browser command!");
		return FALSE;
	}
	done = browser_execute (cmd, uri, browser?browser->escapeRemote:FALSE, FALSE);
	g_free (cmd);
	return done;
}
