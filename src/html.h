/**
 * @file html.h HTML parsing
 *
 * Copyright (C) 2004 ahmed el-helw <ahmedre@cc.gatech.edu>
 * Copyright (C) 2017-2020 Lars Windolf <lars.windolf@gmx.de>
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

#ifndef _HTML_H
#define _HTML_H

#include <glib.h>

/**
 * html_auto_discover_feed:
 *
 * HTML feed auto discovery function. Searches the
 * passed HTML document for feed links and returns
 * one if at least one link could be found.
 *
 * @data:	HTML source
 * @baseUri:	URI that relative links will be based off of
 * Returns:	a list of feed URLs or NULL. Must be freed by caller.
 */
GSList * html_auto_discover_feed(const gchar* data, const gchar *baseUri);

/**
 * html_discover_favicon:
 *
 * Search for favicon links in a HTML file's head section
 *
 * @data:	HTML source
 * @baseUri:	URI of the downloaded HTML used to resolve relative URIs
 * Returns: URL of the favicon, or NULL. Must be freed by caller.
 */
gchar * html_discover_favicon(const gchar* data, const gchar *baseUri);

/**
 * html_get_article:
 *
 * Parse HTML as XML to check wether it contains an HTML5 article.
 *
 * @data:	the HTML to check
 * @baseUri:	URI of the downloaded HTML used to resolve relative URIs
 *
 * Returns: XHTML fragment representing the article or NULL
 */
gchar * html_get_article(const gchar *data, const gchar *baseUri);

/**
 * html_get_amp_url:
 *
 * Parse HTML and returns AMP URL if found
 *
 * @data:	the HTML to check
 *
 * Returns: AMP URL or NULL. Must be free'd by caller
 */
gchar * html_get_amp_url(const gchar *data);

#endif
