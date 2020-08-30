/**
 * @file browser_history.h  managing internal browser URI history
 *
 * Copyright (C) 2012-2014 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _BROWSER_HISTORY_H
#define _BROWSER_HISTORY_H

#include <glib.h>

/** structure holding all URLs visited in a browser */
typedef struct browserHistory {
	GList		*locations;	/**< list of all visited URLs */
	GList		*current;	/**< pointer into locations */
} browserHistory;

/**
 * Create a new browser history.
 *
 * @returns the browser history
 */
browserHistory * browser_history_new (void);

/**
 * Dispose of a browser history
 *
 * @param history	the browser history
 */
void browser_history_free (browserHistory *history);

/**
 * Set index of the browser history forward.
 *
 * @param history	the browser history
 *
 * @returns the new selected URL (not to be free'd!)
 */
gchar * browser_history_forward (browserHistory *history);

/**
 * Set index of the browser history backwards.
 *
 * @param history	the browser history
 *
 * @returns the new selected URL (not to be free'd!)
 */
gchar * browser_history_back (browserHistory *history);

/**
 * Check whether the history can go forward.
 *
 * @param history	the browser history
 *
 * @returns TRUE if it can go forward
 */
gboolean browser_history_can_go_forward (browserHistory *history);

/**
 * Check whether the history can go back.
 *
 * @param history	the browser history
 *
 * @returns TRUE if it can go back
 */
gboolean browser_history_can_go_back (browserHistory *history);

/**
 * Add a URL to the history.
 *
 * @param history	the browser history
 * @param url		the URL to add
 */
void browser_history_add_location (browserHistory *history, const gchar *url);

#endif
