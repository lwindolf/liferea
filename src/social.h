/**
 * @file social.h social networking integration
 * 
 * Copyright (C) 2006 Lars Lindner <lars.lindner@gmx.net>
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

#ifndef _SOCIAL_H
#define _SOCIAL_H

#include <glib.h>

typedef struct socialBookmarkSite {
	gchar		*name;		/**< Descriptive name for HTML rendering and preferences */
	gchar		*url;		/**< URL format string with %s for title and URL insertion */
	gboolean	title;		/**< TRUE if title submission supported */
	gboolean	titleFirst;	/**< TRUE if title %s comes first */
} *socialBookmarkSitePtr;


void social_init(void);

/**
 * Returns a social bookmarking link for the configured service
 *
 * @param link		the link to encode (mandatory)
 * @param title		the title to encode (mandatory)
 *
 * @returns new URL string
 */
gchar * social_get_url(const gchar *link, const gchar *title);

/**
 * Changes the current social bookmarking configuration to the given service.
 *
 * @param name		name (id) of the social bookmarking service
 */
void social_set_site(const gchar *name);

#endif
