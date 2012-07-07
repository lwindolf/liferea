/**
 * @file browser.h  Launching different external browsers
 *
 * Copyright (C) 2008 Lars Windolf <lars.lindner@gmail.com>
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

#ifndef _BROWSER_H
#define _BROWSER_H

#include <glib.h>

/** Browser preference definition structure */
struct browser {
	gchar *id;			/**< Unique ID used in storing the prefs */
	gchar *display;			/**< Name to display in the prefs */
	gchar *defaultplace;		/**< Default command.... Use %s to specify URL. This command is called in the background. */
	gchar *existingwin;		/**< Optional command variant for opening in existing window */
	gchar *existingwinremote;	/**< Optional command variant for opening in existing window with remote protocol */
	gchar *newwin;			/**< Optional command variant for opening in new window */
	gchar *newwinremote;		/**< Optional command variant for opening in new window with remote protocol */
	gchar *newtab;			/**< Optional command variant for opening in new tab */
	gchar *newtabremote;		/**< Optional command variant for opening in new tab with remote protocol */
};

/**
 * Return the list of all supported browser definitions.
 *
 * @returns NULL terminated array of browser definitions
 */
struct browser * browser_get_all (void);

/**
 * Function to execute the commands needed to open up a URL with the
 * browser specified in the preferences.
 *
 * @param the URI to load
 *
 * @returns TRUE if the URI was opened, or FALSE if there was an error
 */

gboolean browser_launch_URL_external (const gchar *uri);

#endif
