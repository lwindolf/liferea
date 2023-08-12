/*
 * @file social.h  social networking integration
 * 
 * Copyright (C) 2006-2010 Lars Windolf <lars.windolf@gmx.de>
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

#include "item.h"

typedef struct socialSite {
	gchar		*name;		/*<< Descriptive name for HTML rendering and preferences */
	gchar		*url;		/*<< URL format string with %s for title and URL insertion */
	gboolean	title;		/*<< TRUE if title submission supported */
	gboolean	titleFirst;	/*<< TRUE if title %s comes first */
} *socialSitePtr;

/**
 * social_init: (skip)
 *
 * Initialize social bookmarking support.
 */
void social_init (void);

/**
 * social_free: (skip)
 *
 * Frees social bookmarking structures
 */
void social_free (void);

/**
 * social_set_bookmark_site:
 * @name:		name of the site
 *
 * Change the site used for bookmarking.
 */
void social_set_bookmark_site (const gchar *name);

/**
 * social_register_bookmark_site:
 * @name:		descriptive name
 * @url:		valid HTTP GET URL with one or two %s format codes
 *
 * Add a new site to the social bookmarking site list. Note that
 * the URL needs to have at least one '{url}' placeholder and optionally
 * a '{title}' placeholder.
 */
void social_register_bookmark_site (const gchar *name, const gchar *url);

/**
 * social_unregister_bookmark_site:
 * @name:		descriptive name
 *
 * Removes a site from the social bookmarking site list. Does nothing
 * if the given name is not in the list
 */
void social_unregister_bookmark_site (const gchar *name);

/**
 * social_get_bookmark_url:
 * @link:		the link to encode (mandatory)
 * @title:		the title to encode (mandatory)
 *
 * Returns a social bookmarking link for the configured site
 *
 * Returns: new URL string
 */
gchar * social_get_bookmark_url (const gchar *link, const gchar *title);

/**
 * social_add_bookmark: (skip)
 * @item:		the item
 *
 * Add a social bookmark for the link of the given item
 */
void social_add_bookmark (const itemPtr item);

/**
 * social_get_bookmark_site:
 *
 * Returns: the name of the currently configured social bookmarking site.
 */
const gchar * social_get_bookmark_site (void);

#endif
