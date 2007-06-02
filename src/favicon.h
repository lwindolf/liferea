/**
 * @file favicon.h Liferea favicon handling
 * 
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

#ifndef _FAVICON_H
#define _FAVICON_H

#include <glib.h>
#include <gtk/gtk.h>
#include "update.h"

/**
 * Tries to load a given favicon from cache.
 *
 * @param id		the favicon id
 *
 * @returns a pixmap (or NULL)
 */
GdkPixbuf * favicon_load_from_cache(const gchar *id);

/**
 * Removes a given favicon from the favicon cache.
 *
 * @param id		the favicon id
 */
void favicon_remove_from_cache(const gchar *id);

/**
 * Checks wether a given favicon needs to be updated 
 *
 * @param id		the favicon id
 * @param updateState	update state info of the favicon
 * @param now		current time
 */
gboolean favicon_update_needed (const gchar *id, updateStatePtr updateState, GTimeVal *now);

/**
 * Favicon download callback. Called after the download
 * has finished (both on success and failure).
 *
 * @param user_data	user data for the callback
 */
typedef void (*faviconUpdatedCb)(gpointer user_data);

/**
 * Tries to download a favicon from and relative to a given
 * feed source URL and an optional feed HTML URL. Can be used
 * for non-feed related favicon download too.
 *
 * @param id		cache id of the favicon (usually = node id)
 * @param html_url	URL of a website where a favicon could be found (optional)
 * @param source_url	URL (usually the feed source) where a favicon can be found (mandatory)
 * @param options	download options 
 * @param callback	callback to triggered on success
 * @param user_data	user data to be passed to callback
 */
void favicon_download(const gchar *id, const gchar *html_url, const gchar *source_url, updateOptionsPtr options, faviconUpdatedCb callback, gpointer user_data);

#endif
