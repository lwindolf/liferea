/**
 * @file browser.c  Launching different external browsers
 *
 * Copyright (C) 2003-2010 Lars Windolf <lars.lindner@gmail.com>
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

static struct browser browsers[] = {
	{
		"default", N_("Default Browser"), NULL, /* triggering gtk_show_uri() */
		NULL, NULL,
		NULL, NULL,
		NULL, NULL
	},
	{
		/* tested with Firefox 1.5 and 2.0 */
		"firefox", "Firefox", "firefox \"%s\"",
		NULL, "firefox -a firefox -remote \"openURL(%s)\"",
		NULL, "firefox -a firefox -remote 'openURL(%s,new-window)'",
		NULL, "firefox -a firefox -remote 'openURL(%s,new-tab)'"
	},
	{
		"google-chrome", "Chrome", "google-chrome \"%s\"",
		NULL, NULL,
		NULL, NULL,
		NULL, NULL
	},		
	{
		"opera", "Opera", "opera \"%s\"",
		"opera \"%s\"", "opera -remote \"openURL(%s)\"",
		"opera -newwindow \"%s\"", NULL,
		"opera -newpage \"%s\"", NULL
	},
	{
		"epiphany", "Epiphany", "epiphany \"%s\"",
		NULL, NULL,
		"epiphany \"%s\"", NULL,
		"epiphany -n \"%s\"", NULL
	},
	{
		/* tested with SeaMonkey 1.0.6 */
		"mozilla", "Mozilla", "mozilla %s",
		NULL, "mozilla -remote openURL(%s)",
		NULL, "mozilla -remote 'openURL(%s,new-window)'",
		NULL, "mozilla -remote 'openURL(%s,new-tab)'"
	},
	{
		"konqueror", "Konqueror", "kfmclient openURL \"%s\"",
		NULL, NULL,
		NULL, NULL,
		NULL, NULL
	},
	{	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL }
};

/**
 * Returns a shell command format string which can be used to create
 * a browser launch command. The string will contain exactly one %s
 * to fill in the URL. 
 *
 * @param browser	browser definition (or NULL) as returned
 *			by prefs_get_browser()
 * @param remote	TRUE if remote command variant is requested
 * @param fallback	TRUE if the default command is to be returned
 *			if the specific launch type is not available.
 *			If set to FALSE no command might be returned.
 *
 * @returns a newly allocated command string
 */
static gchar *
browser_get_command (struct browser *browser, gboolean remote, gboolean fallback)
{
	gchar	*cmd = NULL;
	gchar	*libname;
	gint	place;

	conf_get_int_value (BROWSER_PLACE, &place);

	/* check for manual browser command */
	conf_get_str_value (BROWSER_ID, &libname);
	if (g_str_equal (libname, "manual")) {
		/* retrieve user defined command... */
		conf_get_str_value (BROWSER_COMMAND, &cmd);
	} else {
		/* non manual browser definitions... */
		if (browser) {
			if (remote) {
				switch (place) {
					case 1:
						cmd = browser->existingwinremote;
						break;
					case 2:
						cmd = browser->newwinremote;
						break;
					case 3:
						cmd = browser->newtabremote;
						break;
				}
			} else {
				switch (place) {
					case 1:
						cmd = browser->existingwin;
						break;
					case 2:
						cmd = browser->newwin;
						break;
					case 3:
						cmd = browser->newtab;
						break;
				}
			}

			if (fallback && !cmd)	/* Default when no special mode defined */
				cmd = browser->defaultplace;
		}

		if (fallback && !cmd)	/* Last fallback: first browser default */
			cmd = browsers[0].defaultplace;
	}
	g_free (libname);
		
	return g_strdup (cmd);
}

/** 
 * Returns the browser definition structure for the currently
 * configured external browser or NULL if a user defined 
 * browser command is defined.
 *
 * @returns external browser definition
 */
static struct browser *
browser_get_default (void)
{
	gchar		*libname;
	struct browser	*browser = NULL;
	
	conf_get_str_value (BROWSER_ID, &libname);
	if (!g_str_equal (libname, "manual")) {
		struct browser *iter;
		for (iter = browsers; iter->id != NULL; iter++) {
			if (g_str_equal (libname, iter->id))
				browser = iter;
		}
	}
	g_free (libname);

	return browser;
}

struct browser *
browser_get_all (void)
{
	return browsers;
}

static gboolean
browser_execute (const gchar *cmd, const gchar *uri, gboolean sync)
{
	GError		*error = NULL;
	gchar 		*safeUri, *tmp, **argv, **iter;
	gint 		argc;
	gint		status = 0;
	gboolean 	done = FALSE;
  
	g_assert (cmd != NULL);
	g_assert (uri != NULL);

	safeUri = common_uri_sanitize (uri);

	/* If we run using a "-remote openURL()" mechanism we need to escape commata, but not in other cases (see SF #2901447) */
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
		debug2 (DEBUG_GUI, "Browser command failed: %s : %s", tmp, error->message);
		g_error_free (error);
		return FALSE;
	}
  
	if (argv) {
		for (iter = argv; *iter != NULL; iter++)
			*iter = common_strreplace (*iter, "%s", safeUri);
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
  
	g_free (safeUri);
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
	gchar		*libname = NULL;
	
	g_assert (uri != NULL);
	
	browser = browser_get_default ();
	if (browser) {
		/* try to execute synchronously... */
		cmd = browser_get_command (browser, TRUE /* remote */, FALSE /* fallback */);
		if (cmd) {
			done = browser_execute (cmd, uri, TRUE);
			g_free (cmd);
		} else {
			/* the "default" browser has no command to use the GTK
			   launch mechanism, so we use gtk_show_uri() instead */
			conf_get_str_value (BROWSER_ID, &libname);
			if (g_str_equal (libname, "default"))
				done = gtk_show_uri (NULL, uri, 0, NULL);
			g_free (libname);
		}
	}
	
	if (done)
		return TRUE;
	
	/* if it failed try to execute asynchronously... */		
	cmd = browser_get_command (browser, FALSE /* remote */, TRUE /* fallback */);
	if (!cmd) {
		liferea_shell_set_important_status_bar ("Fatal: cannot retrieve browser command!");
		g_warning ("Fatal: cannot retrieve browser command!");
		return FALSE;
	}
	done = browser_execute (cmd, uri, FALSE);
	g_free (cmd);
	return done;
}
